/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */
#include <unistd.h>
#include <net/if.h>
#include <rte_malloc.h>
#include <rte_vfio.h>
#include <rte_vhost.h>
#include <rte_vdpa.h>
#include <vdpa_driver.h>
#include <rte_kvargs.h>

#include <virtio_api.h>
#include <virtio_lm.h>
#include "rte_vf_rpc.h"
#include "virtio_vdpa.h"

RTE_LOG_REGISTER(virtio_vdpa_logtype, pmd.vdpa.virtio, NOTICE);
#define DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_vdpa_logtype, \
		"VIRTIO VDPA %s(): " fmt "\n", __func__, ##args)

int virtio_vdpa_lcore_id = 0;
#define VIRTIO_VDPA_DEV_CLOSE_WORK_CLEAN 0
#define VIRTIO_VDPA_DEV_CLOSE_WORK_START 1
#define VIRTIO_VDPA_DEV_CLOSE_WORK_DONE 2
#define VIRTIO_VDPA_DEV_CLOSE_WORK_ERR 3

#define VIRTIO_VDPA_STATE_ALIGN 4096

#define RTE_ROUNDUP(x, y) ((((x) + ((y) - 1)) / (y)) * (y))

extern struct virtio_vdpa_device_callback virtio_vdpa_blk_callback;
extern struct virtio_vdpa_device_callback virtio_vdpa_net_callback;

static TAILQ_HEAD(virtio_vdpa_privs, virtio_vdpa_priv) virtio_priv_list =
						  TAILQ_HEAD_INITIALIZER(virtio_priv_list);
static pthread_mutex_t priv_list_lock = PTHREAD_MUTEX_INITIALIZER;

static struct virtio_vdpa_priv *
virtio_vdpa_find_priv_resource_by_vdev(const struct rte_vdpa_device *vdev)
{
	struct virtio_vdpa_priv *priv;
	bool found = false;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &virtio_priv_list, next) {
		if (vdev == priv->vdev) {
			found = true;
			break;
		}
	}
	pthread_mutex_unlock(&priv_list_lock);
	if (!found) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		rte_errno = ENODEV;
		return NULL;
	}
	return priv;
}

const struct rte_memzone *
virtio_vdpa_dev_dp_map_get(struct virtio_vdpa_priv *priv, size_t len)
{
	if (!priv->vdpa_dp_map) {
		if (!priv->vdpa_dp_map) {
			char dp_mzone_name[64];

			snprintf(dp_mzone_name, sizeof(dp_mzone_name), "VIRTIO_VDPA_DEBUG_DP_MZ_%u",
					priv->vid);
			priv->vdpa_dp_map = rte_memzone_reserve_aligned(dp_mzone_name,
					len, rte_socket_id(), RTE_MEMZONE_IOVA_CONTIG,
					VIRTIO_VRING_ALIGN);
			if (!priv->vdpa_dp_map) {
				DRV_LOG(ERR, "Fail to alloc mem zone for VIRTIO_VDPA_DEBUG_DP_MZ_%u", priv->vid);
			}
		}
	}

	return priv->vdpa_dp_map;
}

struct virtio_vdpa_priv *
virtio_vdpa_find_priv_resource_by_name(const char *vf_name)
{
	struct virtio_vdpa_priv *priv;
	bool found = false;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &virtio_priv_list, next) {
		if (strcmp(vf_name, priv->pdev->device.name) == 0) {
			found = true;
			break;
		}
	}
	pthread_mutex_unlock(&priv_list_lock);
	if (!found) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vf_name);
		rte_errno = ENODEV;
		return NULL;
	}
	return priv;
}

uint64_t
virtio_vdpa_gpa_to_hva(int vid, uint64_t gpa)
{
	struct rte_vhost_memory *mem = NULL;
	struct rte_vhost_mem_region *reg;
	uint32_t i;
	uint64_t hva = 0;

	if (rte_vhost_get_mem_table(vid, &mem) < 0) {
		if (mem)
			free(mem);
		DRV_LOG(ERR, "Virtio dev %d get mem table fail", vid);
		return 0;
	}

	for (i = 0; i < mem->nregions; i++) {
		reg = &mem->regions[i];

		if (gpa >= reg->guest_phys_addr &&
				gpa < reg->guest_phys_addr + reg->size) {
			hva = gpa - reg->guest_phys_addr + reg->host_user_addr;
			break;
		}
	}

	free(mem);
	return hva;
}

int virtio_vdpa_dirty_desc_get(struct virtio_vdpa_priv *priv, int qix, uint64_t *desc_addr, uint32_t *write_len)
{
	return priv->dev_ops->dirty_desc_get(priv->vid, qix, desc_addr, write_len);
}

int virtio_vdpa_used_vring_addr_get(struct virtio_vdpa_priv *priv, int qix, uint64_t *used_vring_addr, uint32_t *used_vring_len)
{
	*used_vring_addr = priv->vrings[qix]->used;
	*used_vring_len = sizeof(struct vring_used);
	return 0;
}

int virtio_vdpa_max_phy_addr_get(struct virtio_vdpa_priv *priv, uint64_t *phy_addr)
{
	struct rte_vhost_memory *mem = NULL;
	struct rte_vhost_mem_region *reg;
	int ret;
	uint32_t i = 0;

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid priv device");
		return -ENODEV;
	}

	*phy_addr = 0;
	ret = rte_vhost_get_mem_table(priv->vid, &mem);
	if (ret < 0) {
		DRV_LOG(ERR, "%s failed to get VM memory layout ret:%d",
					priv->vdev->device->name, ret);
		return ret;
	}

	for (i = 0; i < mem->nregions; i++) {
		reg = &mem->regions[i];
		if (*phy_addr < reg->guest_phys_addr + reg->size)
			*phy_addr = reg->guest_phys_addr + reg->size;
	}

	DRV_LOG(INFO, "Max phy addr is 0x%" PRIx64, *phy_addr);
	free(mem);
	return 0;
}

static int
virtio_vdpa_vqs_max_get(struct rte_vdpa_device *vdev, uint32_t *queue_num)
{
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);
	int unit;

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		return -ENODEV;
	}

	unit = priv->dev_ops->vdpa_queue_num_unit_get();
	*queue_num = priv->hw_nr_virtqs / unit;
	DRV_LOG(DEBUG, "Vid %d queue num is %d unit %d", priv->vid, *queue_num, unit);
	return 0;
}

static int
virtio_vdpa_features_get(struct rte_vdpa_device *vdev, uint64_t *features)
{
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		return -ENODEV;
	}

	if (priv->configured)
		virtio_pci_dev_features_get(priv->vpdev, features);
	else
		virtio_pci_dev_state_features_get(priv->vpdev, features);

	*features |= (1ULL << VHOST_USER_F_PROTOCOL_FEATURES);
	*features |= (1ULL << VHOST_F_LOG_ALL);
	if (priv->dev_ops->add_vdpa_feature)
		priv->dev_ops->add_vdpa_feature(features);
	DRV_LOG(INFO, "%s hw feature is 0x%" PRIx64, priv->vdev->device->name, *features);

	return 0;
}

static int
virtio_vdpa_protocol_features_get(struct rte_vdpa_device *vdev,
		uint64_t *features)
{
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		return -ENODEV;
	}

	priv->dev_ops->vhost_feature_get(features);
	return 0;
}

static uint64_t
virtio_vdpa_hva_to_gpa(int vid, uint64_t hva)
{
	struct rte_vhost_memory *mem = NULL;
	struct rte_vhost_mem_region *reg;
	uint32_t i;
	uint64_t gpa = 0;

	if (rte_vhost_get_mem_table(vid, &mem) < 0) {
		if (mem)
			free(mem);
		DRV_LOG(ERR, "Virtio dev %d get mem table fail", vid);
		return 0;
	}

	for (i = 0; i < mem->nregions; i++) {
		reg = &mem->regions[i];

		if (hva >= reg->host_user_addr &&
				hva < reg->host_user_addr + reg->size) {
			gpa = hva - reg->host_user_addr + reg->guest_phys_addr;
			break;
		}
	}

	free(mem);
	return gpa;
}

static void
virtio_vdpa_virtq_handler(void *cb_arg)
{
	struct virtio_vdpa_vring_info *virtq = cb_arg;
	struct virtio_vdpa_priv *priv = virtq->priv;
	uint64_t buf;
	int nbytes,i;

	if (!priv->configured || !virtq->enable) {
		return;
	}
	if (rte_intr_fd_get(virtq->intr_handle) < 0) {
		return;
	}
	for(i = 0; i < 3; i++) {
		nbytes = read(rte_intr_fd_get(virtq->intr_handle), &buf, 8);
		if (!priv->configured || !virtq->enable) {
			return;
		}
		if (rte_intr_fd_get(virtq->intr_handle) < 0) {
			return;
		}

		if (nbytes < 0) {
			if (errno == EINTR ||
				errno == EWOULDBLOCK ||
				errno == EAGAIN)
				continue;
			DRV_LOG(ERR,  "%s failed to read kickfd of virtq %d: %s",
				priv->vdev->device->name, virtq->index, strerror(errno));
		}
		break;
	}

	if (nbytes < 0) {
		DRV_LOG(ERR,  "%s failed to read %d times kickfd of virtq %d: %s",
			priv->vdev->device->name, i, virtq->index, strerror(errno));
		return;
	}

	virtio_pci_dev_queue_notify(priv->vpdev, virtq->index);
	if (virtq->notifier_state == VIRTIO_VDPA_NOTIFIER_STATE_DISABLED) {
		if (rte_vhost_host_notifier_ctrl(priv->vid, virtq->index, true)) {
			DRV_LOG(ERR,  "%s failed to set notify ctrl virtq %d: %s",
					priv->vdev->device->name, virtq->index, strerror(errno));
			virtq->notifier_state = VIRTIO_VDPA_NOTIFIER_STATE_ERR;
		} else
			virtq->notifier_state = VIRTIO_VDPA_NOTIFIER_STATE_ENABLED;
		DRV_LOG(INFO, "%s virtq %u notifier state is %s",
						priv->vdev->device->name,
						virtq->index,
						virtq->notifier_state ==
						VIRTIO_VDPA_NOTIFIER_STATE_ENABLED ? "enabled" :
									"disabled");
	}
	DRV_LOG(DEBUG, "%s ring virtq %u doorbell i:%d",
					priv->vdev->device->name, virtq->index, i);
}

static int
virtio_vdpa_virtq_doorbell_relay_disable(struct virtio_vdpa_priv *priv,
														int vq_idx)
{
	int ret = -EAGAIN;
	struct rte_intr_handle *intr_handle;
	int retries = VIRTIO_VDPA_INTR_RETRIES;

	intr_handle = priv->vrings[vq_idx]->intr_handle;
	if (rte_intr_fd_get(intr_handle) != -1) {
		while (retries-- && ret == -EAGAIN) {
			ret = rte_intr_callback_unregister(intr_handle,
							virtio_vdpa_virtq_handler,
							priv->vrings[vq_idx]);
			if (ret == -EAGAIN) {
				DRV_LOG(DEBUG, "%s try again to unregister fd %d "
				"of virtq %d interrupt, retries = %d",
				priv->vdev->device->name,
				rte_intr_fd_get(intr_handle),
				(int)priv->vrings[vq_idx]->index, retries);
				usleep(VIRTIO_VDPA_INTR_RETRIES_USEC);
			}
		}
		rte_intr_fd_set(intr_handle, -1);
	}
	rte_intr_instance_free(intr_handle);
	return 0;
}

static int
virtio_vdpa_virtq_doorbell_relay_enable(struct virtio_vdpa_priv *priv, int vq_idx)
{
	int ret;
	struct rte_vhost_vring vq;
	struct rte_intr_handle *intr_handle;

	ret = rte_vhost_get_vhost_vring(priv->vid, vq_idx, &vq);
	if (ret)
		return ret;

	intr_handle = rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_SHARED);
	if (intr_handle == NULL) {
		DRV_LOG(ERR, "%s fail to allocate intr_handle",
						priv->vdev->device->name);
		return -EINVAL;
	}

	priv->vrings[vq_idx]->intr_handle = intr_handle;
	if (rte_intr_fd_set(intr_handle, vq.kickfd)) {
		DRV_LOG(ERR, "%s fail to set kick fd", priv->vdev->device->name);
		goto error;
	}

	if (rte_intr_fd_get(intr_handle) == -1) {
		DRV_LOG(ERR, "%s virtq %d kickfd is invalid",
					priv->vdev->device->name, vq_idx);
		goto error;
	} else {
		if (rte_intr_type_set(intr_handle, RTE_INTR_HANDLE_EXT))
			goto error;

		if (rte_intr_callback_register(intr_handle,
						   virtio_vdpa_virtq_handler,
						   priv->vrings[vq_idx])) {
			rte_intr_fd_set(intr_handle, -1);
			DRV_LOG(ERR, "%s failed to register virtq %d interrupt",
						priv->vdev->device->name,
						vq_idx);
			goto error;
		} else {
			DRV_LOG(DEBUG, "%s register fd %d interrupt for virtq %d",
				priv->vdev->device->name,
				rte_intr_fd_get(intr_handle),
				vq_idx);
		}
	}

	return 0;

error:
	virtio_vdpa_virtq_doorbell_relay_disable(priv, vq_idx);
	return -EINVAL;
}

static int
virtio_vdpa_virtq_disable(struct virtio_vdpa_priv *priv, int vq_idx)
{
	struct rte_vhost_vring vq;
	int ret;

    if (priv->configured) {
		uint64_t features;
		virtio_pci_dev_features_get(priv->vpdev, &features);
		if (!(features & VIRTIO_F_RING_RESET)) {
			DRV_LOG(WARNING, "%s can't disable queue after driver ok without queue reset support",
					priv->vdev->device->name);
			return 0;
		}
	}

	ret = virtio_vdpa_virtq_doorbell_relay_disable(priv, vq_idx);
	if (ret) {
		DRV_LOG(ERR, "%s doorbell relay disable failed ret:%d",
						priv->vdev->device->name, ret);
	}
	priv->vrings[vq_idx]->notifier_state = VIRTIO_VDPA_NOTIFIER_STATE_DISABLED;

	if (priv->configured) {
		virtio_pci_dev_queue_del(priv->vpdev, vq_idx);

		if (priv->vrings[vq_idx]->vector_enable) {
			ret = virtio_pci_dev_interrupt_disable(priv->vpdev, vq_idx + 1);
		}
		if (ret) {
			DRV_LOG(ERR, "%s virtq %d interrupt disabel failed",
							priv->vdev->device->name, vq_idx);
		}
		ret = rte_vhost_get_vhost_vring(priv->vid, vq_idx, &vq);
		if (ret) {
			DRV_LOG(ERR, "%s virtq %d fail to get hardware idx",
							priv->vdev->device->name, vq_idx);
		}

		DRV_LOG(INFO, "%s virtq %d set hardware idx:%d",
				priv->vdev->device->name, vq_idx, vq.used->idx);
		ret = rte_vhost_set_vring_base(priv->vid, vq_idx, vq.used->idx, vq.used->idx);
		if (ret) {
			DRV_LOG(ERR, "%s virtq %d fail to set hardware idx",
							priv->vdev->device->name, vq_idx);
		}
	} else {
		virtio_pci_dev_state_queue_del(priv->vpdev, vq_idx, priv->state_mz->addr);
		virtio_pci_dev_state_interrupt_disable(priv->vpdev, vq_idx + 1, priv->state_mz->addr);
	}

	priv->vrings[vq_idx]->enable = false;
	return 0;
}

static int
virtio_vdpa_virtq_enable(struct virtio_vdpa_priv *priv, int vq_idx)
{
	int ret;
	int vid;
	struct rte_vhost_vring vq;
	struct virtio_pci_dev_vring_info vring_info;
	uint64_t gpa;

	vid = priv->vid;

	ret = rte_vhost_get_vhost_vring(vid, vq_idx, &vq);
	if (ret)
		return ret;

	DRV_LOG(DEBUG, "vid:%d vq_idx:%d avl idx:%d use idx:%d", vid, vq_idx, vq.avail->idx, vq.used->idx);

	if (vq.callfd != -1) {
		if (priv->nvec < (vq_idx + 1)) {
			DRV_LOG(ERR, "%s Error: dev interrupts %d less than queue: %d",
						priv->vdev->device->name, priv->nvec, vq_idx + 1);
			return -EINVAL;
		}

		ret = priv->configured ? virtio_pci_dev_interrupt_enable(priv->vpdev, vq.callfd, vq_idx + 1) :
			virtio_pci_dev_state_interrupt_enable(priv->vpdev, vq.callfd, vq_idx + 1, priv->state_mz->addr);
		if (ret) {
			DRV_LOG(ERR, "%s virtq interrupt enable failed ret:%d",
							priv->vdev->device->name, ret);
			return ret;
		}
		if (priv->configured)
			priv->vrings[vq_idx]->vector_enable = true;
	} else {
		DRV_LOG(DEBUG, "%s virtq %d call fd is -1, interrupt is disabled",
						priv->vdev->device->name, vq_idx);
	}

	gpa = virtio_vdpa_hva_to_gpa(vid, (uint64_t)(uintptr_t)vq.desc);
	if (gpa == 0) {
		DRV_LOG(ERR, "Dev %s fail to get GPA for descriptor ring %d",
						priv->vdev->device->name, vq_idx);
		return -EINVAL;
	}
	DRV_LOG(DEBUG, "%s virtq %d desc addr%"PRIx64,
					priv->vdev->device->name, vq_idx, gpa);
	priv->vrings[vq_idx]->desc = gpa;
	vring_info.desc = gpa;

	gpa = virtio_vdpa_hva_to_gpa(vid, (uint64_t)(uintptr_t)vq.avail);
	if (gpa == 0) {
		DRV_LOG(ERR, "%s fail to get GPA for available ring",
					priv->vdev->device->name);
		return -EINVAL;
	}
	DRV_LOG(DEBUG, "Virtq %d avail addr%"PRIx64, vq_idx, gpa);
	priv->vrings[vq_idx]->avail = gpa;
	vring_info.avail = gpa;

	gpa = virtio_vdpa_hva_to_gpa(vid, (uint64_t)(uintptr_t)vq.used);
	if (gpa == 0) {
		DRV_LOG(ERR, "%s fail to get GPA for used ring",
					priv->vdev->device->name);
		return -EINVAL;
	}
	DRV_LOG(DEBUG, "Virtq %d used addr%"PRIx64, vq_idx, gpa);
	priv->vrings[vq_idx]->used = gpa;
	vring_info.used = gpa;

	/* TO_DO: need to check vq_size not exceed hw limit */
	priv->vrings[vq_idx]->size = vq.size;
	vring_info.size = vq.size;

	DRV_LOG(DEBUG, "Virtq %d nr_entrys:%d", vq_idx, vq.size);

	ret = priv->configured ? virtio_pci_dev_queue_set(priv->vpdev, vq_idx, &vring_info) :
		virtio_pci_dev_state_queue_set(priv->vpdev, vq_idx, &vring_info, priv->state_mz->addr);
	if (ret) {
		DRV_LOG(ERR, "%s setup_queue failed", priv->vdev->device->name);
		return -EINVAL;
	}

	ret = virtio_vdpa_virtq_doorbell_relay_enable(priv, vq_idx);
	if (ret) {
		DRV_LOG(ERR, "%s virtq doorbell relay failed ret:%d",
						priv->vdev->device->name, ret);
		return ret;
	}

	priv->vrings[vq_idx]->enable = true;
	virtio_pci_dev_queue_notify(priv->vpdev, vq_idx);
	return 0;
}

static int
virtio_vdpa_vring_state_set(int vid, int vq_idx, int state)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);
	int ret = 0;

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		return -ENODEV;
	}

	if (vq_idx >= (int)priv->hw_nr_virtqs) {
		DRV_LOG(ERR, "Too big vq_idx: %d", vq_idx);
		return -E2BIG;
	}

	if (!state && !priv->vrings[vq_idx]->enable) {
		DRV_LOG(INFO, "VDPA device %s vid:%d  set vring %d state %d already disabled, just return",
						priv->vdev->device->name, vid, vq_idx, state);
		return 0;
	}

	if (priv->dev_work_flag == VIRTIO_VDPA_DEV_CLOSE_WORK_START) {
		DRV_LOG(ERR, "%s is waiting dev close work finish lcore:%d", vdev->device->name, priv->lcore_id);
		rte_eal_wait_lcore(priv->lcore_id);
	}

	if (priv->dev_work_flag == VIRTIO_VDPA_DEV_CLOSE_WORK_ERR) {
		DRV_LOG(ERR, "%s is dev close work had err", vdev->device->name);
		return -EINVAL;
	}

	/* TO_DO: check if vid set here is suitable */
	priv->vid = vid;

	/* If vq is already enabled, and enable again means parameter change, so,
	 * we disable vq first, then enable
	 */
	if (!state && priv->vrings[vq_idx]->enable)
		ret = virtio_vdpa_virtq_disable(priv, vq_idx);
	else if (state && !priv->vrings[vq_idx]->enable)
		ret = virtio_vdpa_virtq_enable(priv, vq_idx);
	else if (state && priv->vrings[vq_idx]->enable) {
		ret = virtio_vdpa_virtq_disable(priv, vq_idx);
		if (ret) {
			DRV_LOG(ERR, "%s fail to disable vring,ret:%d vring:%d state:%d",
						priv->vdev->device->name, ret, vq_idx, state);
			return ret;
		}
		ret = virtio_vdpa_virtq_enable(priv, vq_idx);
	}
	if (ret) {
		DRV_LOG(ERR, "%s fail to set vring state, ret:%d vq_idx:%d state:%d",
					priv->vdev->device->name, ret, vq_idx, state);
		return ret;
	}

	DRV_LOG(INFO, "VDPA device %s vid:%d  set vring %d state %d",
					priv->vdev->device->name, vid, vq_idx, state);
	return 0;
}

static int
virtio_vdpa_dev_cleanup(int vid)
{
	uint32_t i;
	int ret;
	struct rte_vhost_memory *mem;
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		return -ENODEV;
	}

    if (priv->vfio_container_fd == -1) {
        return 0;
    }

	priv->dev_conf_read = false;

	mem = priv->mem;
	if (mem == NULL) {
		DRV_LOG(INFO, "No mem is registered: %s", vdev->device->name);
		return 0;
	}

	for (i = 0; i < mem->nregions; i++) {
		struct rte_vhost_mem_region *reg;

		reg = &mem->regions[i];
		DRV_LOG(INFO, "%s, region %u: HVA 0x%" PRIx64 ", "
			"GPA 0x%" PRIx64 ", size 0x%" PRIx64 ".",
			"DMA clean up unmap", i,
			reg->host_user_addr, reg->guest_phys_addr, reg->size);

		ret = rte_vfio_container_dma_unmap(priv->vfio_container_fd,
			reg->host_user_addr, reg->guest_phys_addr,
			reg->size);
		if (ret < 0) {
			DRV_LOG(ERR, "%s DMA clean up failed ret:%d",
						priv->vdev->device->name, ret);
			free(mem);
			priv->mem = NULL;
			return ret;
		}
	}

	free(mem);
	priv->mem = NULL;
	return 0;
}
static inline bool
virtio_vdpa_find_mem_reg(const struct rte_vhost_mem_region *key, const struct rte_vhost_memory *mem)
{
	uint32_t i;
	const struct rte_vhost_mem_region *reg;

	if (mem == NULL)
		return false;

	for (i = 0; i < mem->nregions; i++) {
		reg = &mem->regions[i];
		if ((reg->host_user_addr == key->host_user_addr) &&
			(reg->guest_phys_addr == key->guest_phys_addr) &&
			(reg->size == key->size))
			return true;
	}
	return false;
}
static int
virtio_vdpa_dev_set_mem_table(int vid)
{
	uint32_t i = 0;
	int ret;
	struct rte_vhost_memory *cur_mem = NULL;
	struct rte_vhost_mem_region *reg;
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		return -ENODEV;
	}

	priv->vid = vid;
	ret = rte_vhost_get_mem_table(priv->vid, &cur_mem);
	if (ret < 0) {
		DRV_LOG(ERR, "%s failed to get VM memory layout ret:%d",
					priv->vdev->device->name, ret);
		return ret;
	}

    if (priv->vfio_container_fd == -1) {
		DRV_LOG(ERR, "%s yajun: skip rte_vfio_container_dma_map",
					priv->vdev->device->name);
        return 0;
    }

	/* Unmap reagion dind't exsit in current */
	if (priv->mem) {
		for (i = 0; i < priv->mem->nregions; i++) {
			reg = &priv->mem->regions[i];
			if (!virtio_vdpa_find_mem_reg(reg, cur_mem)) {
				ret = rte_vfio_container_dma_unmap(priv->vfio_container_fd,
					reg->host_user_addr, reg->guest_phys_addr,
					reg->size);
				DRV_LOG(INFO, "%s, region %u: HVA 0x%" PRIx64 ", "
					"GPA 0x%" PRIx64 ", size 0x%" PRIx64 ".",
					"DMA unmap", i,
					reg->host_user_addr, reg->guest_phys_addr, reg->size);
				if (ret < 0) {
					DRV_LOG(ERR, "%s vdpa unmap reduandnat DMA failed ret:%d",
								priv->vdev->device->name, ret);
					free(cur_mem);
					return ret;
				}
			} else {
				DRV_LOG(INFO, "%s HVA 0x%" PRIx64", "
				"GPA 0x%" PRIx64 ", size 0x%" PRIx64 " exist in cur map",
				vdev->device->name, reg->host_user_addr, reg->guest_phys_addr, reg->size);
			}
		}
	}

	/* Map the region if it doesn't exist yet */
	for (i = 0; i < cur_mem->nregions; i++) {
		reg = &cur_mem->regions[i];
		if (!virtio_vdpa_find_mem_reg(reg, priv->mem)) {
			ret = rte_vfio_container_dma_map(priv->vfio_container_fd,
				reg->host_user_addr, reg->guest_phys_addr,
				reg->size);
			DRV_LOG(INFO, "%s, region %u: HVA 0x%" PRIx64 ", "
				"GPA 0x%" PRIx64 ", size 0x%" PRIx64 ".",
				"DMA map", i,
				reg->host_user_addr, reg->guest_phys_addr, reg->size);
			if (ret < 0) {
				DRV_LOG(ERR, "%s DMA map failed ret:%d",
							priv->vdev->device->name, ret);
				free(cur_mem);
				return ret;
			}
		} else {
			DRV_LOG(INFO, "%s HVA 0x%" PRIx64", "
			"GPA 0x%" PRIx64 ", size 0x%" PRIx64 " already mapped",
			vdev->device->name, reg->host_user_addr, reg->guest_phys_addr, reg->size);
		}
	}

	if (priv->mem)
		free(priv->mem);
	priv->mem = cur_mem;
	return 0;
}

#ifndef PAGE_SIZE
#define PAGE_SIZE   (sysconf(_SC_PAGESIZE))
#endif

static int
virtio_vdpa_features_set(int vid)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);
	uint64_t log_base, log_size, max_phy, log_size_align;
	uint64_t features;
	struct virtio_sge lb_sge;
	rte_iova_t iova;
	int ret;

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		return -ENODEV;
	}

	if (priv->dev_work_flag == VIRTIO_VDPA_DEV_CLOSE_WORK_START) {
		DRV_LOG(ERR, "%s is waiting dev close work finish lcore:%d", vdev->device->name, priv->lcore_id);
		rte_eal_wait_lcore(priv->lcore_id);
	}

	if (priv->dev_work_flag == VIRTIO_VDPA_DEV_CLOSE_WORK_ERR) {
		DRV_LOG(ERR, "%s is dev close work had err", vdev->device->name);
		return -EINVAL;
	}

	priv->vid = vid;
	ret = rte_vhost_get_negotiated_features(vid, &features);
	if (ret) {
		DRV_LOG(ERR, "%s failed to get negotiated features",
					priv->vdev->device->name);
		return ret;
	}
	if (RTE_VHOST_NEED_LOG(features) && priv->configured) {
		ret = rte_vhost_get_log_base(vid, &log_base, &log_size);
		if (ret) {
			DRV_LOG(ERR, "%s failed to get log base",
						priv->vdev->device->name);
			return ret;
		}

		iova = rte_mem_virt2iova((void *)log_base);
		if (iova == RTE_BAD_IOVA) {
			DRV_LOG(ERR, "%s log get iova failed ret:%d",
						priv->vdev->device->name, ret);
			return ret;
		}
		log_size_align = RTE_ROUNDUP(log_size, getpagesize());
		DRV_LOG(INFO, "log buffer %" PRIx64 " iova %" PRIx64 " log size %" PRIx64 " log size align %" PRIx64,
				log_base, iova, log_size, log_size_align);

		ret = rte_vfio_container_dma_map(RTE_VFIO_DEFAULT_CONTAINER_FD, log_base,
						 iova, log_size_align);
		if (ret < 0) {
			DRV_LOG(ERR, "%s log buffer DMA map failed ret:%d",
						priv->vdev->device->name, ret);
			return ret;
		}

		lb_sge.addr = log_base;
		lb_sge.len = log_size;
		ret = virtio_vdpa_max_phy_addr_get(priv, &max_phy);
		if (ret) {
			DRV_LOG(ERR, "%s failed to get max phy addr",
						priv->vdev->device->name);
			return ret;
		}

		ret = virtio_vdpa_cmd_dirty_page_start_track(priv->pf_priv, priv->vf_id, VIRTIO_M_DIRTY_TRACK_PUSH_BITMAP, PAGE_SIZE, 0, max_phy, 1, &lb_sge);
		DRV_LOG(INFO, "%s vfid %d start track max phy:%" PRIx64 "log_base %" PRIx64 "log_size %" PRIx64,
					priv->vdev->device->name, priv->vf_id, max_phy , log_base, log_size);
		if (ret) {
			DRV_LOG(ERR, "%s failed to start track ret:%d",
						priv->vdev->device->name, ret);
			return ret;
		}

		/* TO_DO: add log op */
	}

	/* TO_DO: check why --- */
	features |= (1ULL << VIRTIO_F_IOMMU_PLATFORM);
	features |= (1ULL << VIRTIO_F_RING_RESET);
	if (priv->configured)
		DRV_LOG(ERR, "%s vid %d set feature after driver ok, only when live migration", priv->vdev->device->name, vid);
	else
		priv->guest_features = virtio_pci_dev_state_features_set(priv->vpdev, features, priv->state_mz->addr);

	DRV_LOG(INFO, "%s vid %d guest feature is %" PRIx64 "orign feature is %" PRIx64,
					priv->vdev->device->name, vid,
					priv->guest_features, features);

	return 0;
}

static int
virtio_vdpa_dev_close_work(void *arg)
{
	int ret;
	struct virtio_vdpa_priv *priv = arg;

	DRV_LOG(INFO, "%s vfid %d dev close work of lcore:%d start", priv->vdev->device->name, priv->vf_id, priv->lcore_id);

	ret = virtio_vdpa_cmd_restore_state(priv->pf_priv, priv->vf_id, 0, priv->state_size, priv->state_mz->iova);
	if (ret) {
		DRV_LOG(ERR, "%s vfid %d failed restore state ret:%d", priv->vdev->device->name, priv->vf_id, ret);
		priv->dev_work_flag = VIRTIO_VDPA_DEV_CLOSE_WORK_ERR;
		return ret;
	}

	DRV_LOG(INFO, "%s vfid %d dev close work of lcore:%d finish", priv->vdev->device->name, priv->vf_id, priv->lcore_id);
	priv->dev_work_flag = VIRTIO_VDPA_DEV_CLOSE_WORK_DONE;
	return ret;
}

static int
virtio_vdpa_dev_close(int vid)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);
	struct virtio_dev_run_state_info *tmp_hw_idx;
	struct virtio_admin_migration_get_internal_state_pending_bytes_result res;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint64_t features = 0, max_phy, log_base, log_size, log_size_align;
	uint16_t num_vr;
	rte_iova_t iova;
	int ret, i;

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		return -ENODEV;
	}
	if (!priv->configured) {
		DRV_LOG(ERR, "vDPA device: %s isn't configured.", vdev->device->name);
		return -EINVAL;
	}

	priv->configured = false;

	/* Suspend */
	ret = virtio_vdpa_cmd_set_status(priv->pf_priv, priv->vf_id, VIRTIO_S_QUIESCED);
	if (ret) {
		DRV_LOG(ERR, "%s vfid %d failed suspend ret:%d", vdev->device->name, priv->vf_id, ret);
		// Don't return in device close, try to release all resource.
	}
	priv->lm_status = VIRTIO_S_QUIESCED;

	ret = virtio_vdpa_cmd_set_status(priv->pf_priv, priv->vf_id, VIRTIO_S_FREEZED);
	if (ret) {
		DRV_LOG(ERR, "%s vfid %d failed suspend ret:%d", vdev->device->name, priv->vf_id, ret);
	}
	priv->lm_status = VIRTIO_S_FREEZED;

	rte_vhost_get_negotiated_features(vid, &features);
	if (RTE_VHOST_NEED_LOG(features)) {

		ret = virtio_vdpa_max_phy_addr_get(priv, &max_phy);
		if (ret) {
			DRV_LOG(ERR, "%s failed to get max phy addr",
						priv->vdev->device->name);
		}

		ret = virtio_vdpa_cmd_dirty_page_stop_track(priv->pf_priv, priv->vf_id, max_phy);
		if (ret) {
			DRV_LOG(ERR, "%s failed to stop track max_phy %" PRIx64 " ret:%d",
						priv->vdev->device->name, max_phy, ret);
		}

		ret = rte_vhost_get_log_base(priv->vid, &log_base, &log_size);
		if (ret) {
			DRV_LOG(ERR, "%s failed to get log base",
						priv->vdev->device->name);
		}

		DRV_LOG(INFO, "%s vfid %d stop track max phy:%" PRIx64 "log_base %" PRIx64 "log_size %" PRIx64,
					priv->vdev->device->name, priv->vf_id, max_phy , log_base, log_size);

		iova = rte_mem_virt2iova((void *)log_base);
		if (iova == RTE_BAD_IOVA) {
			DRV_LOG(ERR, "%s log get iova failed ret:%d",
						priv->vdev->device->name, ret);
		}

		log_size_align = RTE_ROUNDUP(log_size, getpagesize());
		DRV_LOG(INFO, "log buffer %" PRIx64 " iova %" PRIx64 " log size align %" PRIx64,
				log_base, iova, log_size_align);

		ret = rte_vfio_container_dma_unmap(RTE_VFIO_DEFAULT_CONTAINER_FD, log_base,
						 iova, log_size_align);
		if (ret < 0) {
			DRV_LOG(ERR, "%s log buffer DMA map failed ret:%d",
						priv->vdev->device->name, ret);
		}

	}
	ret = virtio_vdpa_cmd_get_internal_pending_bytes(priv->pf_priv, priv->vf_id, &res);
	if (ret) {
		DRV_LOG(ERR, "%s vfid %d failed get pending bytes ret:%d", vdev->device->name, priv->vf_id, ret);
	}

	/* If pre allocated memzone is small, we will realloc */
	if (!ret && (res.pending_bytes > VIRTIO_VDPA_REMOTE_STATE_DEFAULT_SIZE)) {
		rte_memzone_free(priv->state_mz_remote);

		ret = snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "%s_remote_mz", vdev->device->name);
		if (ret < 0 || ret >= RTE_MEMZONE_NAMESIZE) {
			DRV_LOG(ERR, "%s remote mem zone print fail ret:%d", vdev->device->name, ret);
		}

		priv->state_mz_remote = rte_memzone_reserve_aligned(mz_name,
										res.pending_bytes,
										priv->pdev->device.numa_node, RTE_MEMZONE_IOVA_CONTIG,
										VIRTIO_VDPA_STATE_ALIGN);
		if (priv->state_mz_remote == NULL) {
			DRV_LOG(ERR, "Failed to reserve remote memzone dev:%s", vdev->device->name);
		}
	}

	if (res.pending_bytes ==0) {
		DRV_LOG(ERR, "Dev:%s pending bytes is 0", vdev->device->name);
	}

	DRV_LOG(INFO, "Dev:%s pending bytes is 0x%" PRIx64, vdev->device->name, res.pending_bytes);

	/*save*/
	ret = virtio_vdpa_cmd_save_state(priv->pf_priv, priv->vf_id, 0,
								res.pending_bytes,
								priv->state_mz_remote->iova);
	if (ret) {
		DRV_LOG(ERR, "%s vfid %d failed get state ret:%d", vdev->device->name, priv->vf_id, ret);
	}

	num_vr = rte_vhost_get_vring_num(vid);
	tmp_hw_idx = rte_zmalloc(NULL, num_vr * sizeof(struct virtio_dev_run_state_info), 0);

	ret = virtio_pci_dev_state_hw_idx_get(priv->state_mz_remote->addr, res.pending_bytes, tmp_hw_idx, num_vr);
	if (ret) {
		rte_free(tmp_hw_idx);
		DRV_LOG(ERR, "%s vfid %d failed get hwidx ret:%d", vdev->device->name, priv->vf_id, ret);
	}

	/* Set_vring_base */
	for(i = 0; i < num_vr; i++) {
		if (tmp_hw_idx[i].flag && priv->vrings[i]->enable) {
			DRV_LOG(INFO, "%s vid %d qid %d set last_avail_idx:%d,last_used_idx:%d",
				vdev->device->name, vid,
				i, tmp_hw_idx[i].last_avail_idx, tmp_hw_idx[i].last_used_idx);
			ret = rte_vhost_set_vring_base(vid, i, tmp_hw_idx[i].last_avail_idx, tmp_hw_idx[i].last_used_idx);
			if (ret) {
				rte_free(tmp_hw_idx);
				DRV_LOG(ERR, "%s vfid %d failed set hwidx ret:%d", vdev->device->name, priv->vf_id, ret);
			}
		}
	}

	rte_free(tmp_hw_idx);

	/* Disable all queues */
	for (i = 0; i < num_vr; i++) {
		if (priv->vrings[i]->enable)
			virtio_vdpa_vring_state_set(vid, i, 0);
	}

	virtio_pci_dev_state_all_queues_disable(priv->vpdev, priv->state_mz->addr);

	virtio_pci_dev_state_dev_status_set(priv->state_mz->addr, VIRTIO_CONFIG_STATUS_ACK |
													VIRTIO_CONFIG_STATUS_DRIVER);



	virtio_vdpa_lcore_id = rte_get_next_lcore(virtio_vdpa_lcore_id, 1, 1);
	priv->lcore_id = virtio_vdpa_lcore_id;
	DRV_LOG(INFO, "%s vfid %d launch dev close work lcore:%d", vdev->device->name, priv->vf_id, priv->lcore_id);
	ret = rte_eal_remote_launch(virtio_vdpa_dev_close_work, priv, virtio_vdpa_lcore_id);
	if (ret) {
		DRV_LOG(ERR, "%s vfid %d failed launch work ret:%d lcore:%d", vdev->device->name, priv->vf_id, ret, virtio_vdpa_lcore_id);
	}
	priv->dev_work_flag = VIRTIO_VDPA_DEV_CLOSE_WORK_START;

	DRV_LOG(INFO, "%s vid %d was closed", priv->vdev->device->name, vid);
	return ret;
}

static int
virtio_vdpa_dev_config(int vid)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);
	uint16_t last_avail_idx, last_used_idx, nr_virtqs;
	uint64_t t_start = rte_rdtsc_precise();
	uint64_t t_end;
	int ret, i;

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		return -ENODEV;
	}
	if (priv->configured) {
		DRV_LOG(ERR, "%s vid %d already configured",
					vdev->device->name, vid);
		return -EBUSY;
	}

	if (priv->dev_work_flag == VIRTIO_VDPA_DEV_CLOSE_WORK_START) {
		DRV_LOG(ERR, "%s is waiting dev close work finish lcore:%d", vdev->device->name, priv->lcore_id);
		rte_eal_wait_lcore(priv->lcore_id);
	}

	if (priv->dev_work_flag == VIRTIO_VDPA_DEV_CLOSE_WORK_ERR) {
		DRV_LOG(ERR, "%s is dev close work had err", vdev->device->name);
		return -EINVAL;
	}

	priv->dev_work_flag = VIRTIO_VDPA_DEV_CLOSE_WORK_CLEAN;

	nr_virtqs = rte_vhost_get_vring_num(vid);
	if (priv->nvec < (nr_virtqs + 1)) {
		DRV_LOG(ERR, "%s warning: dev interrupts %d less than queue: %d",
					vdev->device->name, priv->nvec, nr_virtqs + 1);
	}

	priv->vid = vid;

	for (i = 0; i < nr_virtqs; i++) {
		ret = rte_vhost_get_vring_base(vid, i, &last_avail_idx, &last_used_idx);
		if (ret) {
			DRV_LOG(ERR, "%s error get vring base ret:%d", vdev->device->name, ret);
			rte_errno = rte_errno ? rte_errno : EINVAL;
			return -rte_errno;
		}

		DRV_LOG(INFO, "%s vid %d qid %d last_avail_idx:%d,last_used_idx:%d",
						vdev->device->name, vid,
						i, last_avail_idx, last_used_idx);

		ret = virtio_pci_dev_state_hw_idx_set(priv->vpdev, i ,
											last_avail_idx,
											last_used_idx, priv->state_mz->addr);
		if (ret) {
			DRV_LOG(ERR, "%s error get vring base ret:%d", vdev->device->name, ret);
			rte_errno = rte_errno ? rte_errno : EINVAL;
			return -rte_errno;
		}
	}

	virtio_pci_dev_state_dev_status_set(priv->state_mz->addr, VIRTIO_CONFIG_STATUS_ACK |
													VIRTIO_CONFIG_STATUS_DRIVER |
													VIRTIO_CONFIG_STATUS_FEATURES_OK |
													VIRTIO_CONFIG_STATUS_DRIVER_OK);

	ret = virtio_vdpa_cmd_restore_state(priv->pf_priv, priv->vf_id, 0, priv->state_size, priv->state_mz->iova);
	if (ret) {
		DRV_LOG(ERR, "%s vfid %d failed restore state ret:%d", vdev->device->name, priv->vf_id, ret);
		virtio_pci_dev_state_dump(priv->vpdev , priv->state_mz->addr, priv->state_size);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		return -rte_errno;
	}

	ret = virtio_vdpa_cmd_set_status(priv->pf_priv, priv->vf_id, VIRTIO_S_QUIESCED);
	if (ret) {
		DRV_LOG(ERR, "%s vfid %d failed unfreeze ret:%d", vdev->device->name, priv->vf_id, ret);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		return -rte_errno;
	}
	priv->lm_status = VIRTIO_S_QUIESCED;

	ret = virtio_vdpa_cmd_set_status(priv->pf_priv, priv->vf_id, VIRTIO_S_RUNNING);
	if (ret) {
		DRV_LOG(ERR, "%s vfid %d failed unquiesced ret:%d", vdev->device->name, priv->vf_id, ret);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		return -rte_errno;
	}
	priv->lm_status = VIRTIO_S_RUNNING;

	DRV_LOG(INFO, "%s vid %d move to driver ok", vdev->device->name, vid);

	priv->configured = 1;
	t_end  = rte_rdtsc_precise();
	DRV_LOG(INFO, "%s vid %d was configured, took %lu us.", vdev->device->name,
            vid, (t_end - t_start) * 1000000 / rte_get_tsc_hz());

	return 0;
}

static int
virtio_vdpa_group_fd_get(int vid)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		return -ENODEV;
	}
	return priv->vfio_group_fd;
}

static int
virtio_vdpa_device_fd_get(int vid)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		return -ENODEV;
	}
	return priv->vfio_dev_fd;
}

static int
virtio_vdpa_notify_area_get(int vid, int qid, uint64_t *offset, uint64_t *size)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);
	int ret;

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s", vdev->device->name);
		return -ENODEV;
	}

	ret = virtio_pci_dev_notify_area_get(priv->vpdev, qid, offset, size);
	if (ret) {
		DRV_LOG(ERR, "%s fail to get notify area", vdev->device->name);
		return ret;
	}

	DRV_LOG(DEBUG, "Vid %d qid:%d offset:0x%"PRIx64"size:0x%"PRIx64,
					vid, qid, *offset, *size);
	return 0;
}
static int
virtio_vdpa_dev_config_get(int vid, uint8_t *payload, uint32_t len)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct virtio_vdpa_priv *priv = virtio_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -EINVAL;
	}

	priv->vid = vid;
	priv->dev_conf_read = true;

	virtio_pci_dev_config_read(priv->vpdev, 0, payload, len);
	DRV_LOG(INFO, "vDPA device %d get config len %d", vid, len);

	return 0;
}

static int
virtio_vdpa_dev_presetup_done(int vid)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct virtio_vdpa_priv *priv =
		virtio_vdpa_find_priv_resource_by_vdev(vdev);
	uint16_t last_avail_idx, last_used_idx, nr_virtqs;
	int ret, i;

	DRV_LOG(INFO, "virtio vDPA presetup");
	nr_virtqs = rte_vhost_get_vring_num(vid);
	if (priv->nvec < (nr_virtqs + 1)) {
		DRV_LOG(ERR, "%s warning: dev interrupts %d less than queue: %d",
				vdev->device->name, priv->nvec, nr_virtqs + 1);
	}

	priv->vid = vid;

	for (i = 0; i < nr_virtqs; i++) {
		ret = rte_vhost_get_vring_base(vid, i, &last_avail_idx, &last_used_idx);
		if (ret) {
			DRV_LOG(ERR, "%s error get vring base ret:%d", vdev->device->name, ret);
			rte_errno = rte_errno ? rte_errno : EINVAL;
			return -rte_errno;
		}

		DRV_LOG(INFO, "%s vid %d qid %d last_avail_idx:%d,last_used_idx:%d",
				vdev->device->name, vid,
				i, last_avail_idx, last_used_idx);

		ret = virtio_pci_dev_state_hw_idx_set(priv->vpdev, i,
				last_avail_idx,
				last_used_idx, priv->state_mz->addr);
		if (ret) {
			DRV_LOG(ERR, "%s error get vring base ret:%d", vdev->device->name, ret);
			rte_errno = rte_errno ? rte_errno : EINVAL;
			return -rte_errno;
		}
	}

	virtio_pci_dev_state_dev_status_set(priv->state_mz->addr, VIRTIO_CONFIG_STATUS_ACK |
			VIRTIO_CONFIG_STATUS_DRIVER |
			VIRTIO_CONFIG_STATUS_FEATURES_OK |
			VIRTIO_CONFIG_STATUS_DRIVER_OK);

	ret = virtio_vdpa_cmd_restore_state(priv->pf_priv, priv->vf_id, 0,
			priv->state_size, priv->state_mz->iova);
	if (ret) {
		DRV_LOG(ERR, "%s vfid %d failed restore state ret:%d", vdev->device->name,
			priv->vf_id, ret);
		virtio_pci_dev_state_dump(priv->vpdev , priv->state_mz->addr, priv->state_size);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		return -rte_errno;
	}

	return 0;
}

static struct rte_vdpa_dev_ops virtio_vdpa_ops = {
	.get_queue_num = virtio_vdpa_vqs_max_get,
	.get_features = virtio_vdpa_features_get,
	.get_protocol_features = virtio_vdpa_protocol_features_get,
	.dev_conf = virtio_vdpa_dev_config,
	.dev_close = virtio_vdpa_dev_close,
	.set_vring_state = virtio_vdpa_vring_state_set,
	.set_features = virtio_vdpa_features_set,
	.migration_done = NULL,
	.get_vfio_group_fd = virtio_vdpa_group_fd_get,
	.get_vfio_device_fd = virtio_vdpa_device_fd_get,
	.get_notify_area = virtio_vdpa_notify_area_get,
	.get_stats_names = NULL,
	.get_stats = NULL,
	.reset_stats = NULL,
	.get_dev_config = virtio_vdpa_dev_config_get,
	.set_mem_table = virtio_vdpa_dev_set_mem_table,
	.dev_cleanup = virtio_vdpa_dev_cleanup,
	.presetup_done = virtio_vdpa_dev_presetup_done,
};

static int vdpa_check_handler(__rte_unused const char *key,
		const char *value, void *ret_val)
{
	if (strcmp(value, VIRTIO_ARG_VDPA_VALUE_VF) == 0)
		*(int *)ret_val = 1;
	else
		*(int *)ret_val = 0;

	return 0;
}

static int
virtio_pci_devargs_parse(struct rte_devargs *devargs, int *vdpa)
{
	struct rte_kvargs *kvlist;
	int ret = 0;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL) {
		DRV_LOG(ERR, "Error when parsing param");
		return 0;
	}

	if (rte_kvargs_count(kvlist, VIRTIO_ARG_VDPA) == 1) {
		/* Vdpa mode selected when there's a key-value pair:
		 * vdpa=1
		 */
		ret = rte_kvargs_process(kvlist, VIRTIO_ARG_VDPA,
				vdpa_check_handler, vdpa);
		if (ret < 0)
			DRV_LOG(ERR, "Failed to parse %s", VIRTIO_ARG_VDPA);
	}

	rte_kvargs_free(kvlist);

	return ret;
}

static void
virtio_vdpa_queues_free(struct virtio_vdpa_priv *priv)
{
	uint16_t nr_vq = priv->hw_nr_virtqs;
	struct virtio_vdpa_vring_info *vr;
	uint16_t i;

	if (priv->vrings) {
		for (i = 0; i < nr_vq; i++) {
			vr = priv->vrings[i];
			if (!vr)
				continue;
			rte_free(vr);
			priv->vrings[i] = NULL;
		}
		rte_free(priv->vrings);
		priv->vrings = NULL;
	}

	virtio_pci_dev_queues_free(priv->vpdev, nr_vq);
}

static int
virtio_vdpa_queues_alloc(struct virtio_vdpa_priv *priv)
{
	uint16_t nr_vq = priv->hw_nr_virtqs;
	struct virtio_vdpa_vring_info *vr;
	uint16_t i;
	int ret;

	ret = virtio_pci_dev_queues_alloc(priv->vpdev, nr_vq);
	if (ret) {
		DRV_LOG(ERR, "%s failed to alloc virtio device queues",
					priv->vdev->device->name);
		return ret;
	}

	priv->vrings = rte_zmalloc(NULL,
							sizeof(struct virtio_vdpa_vring_info *) * nr_vq,
							0);
	if (!priv->vrings) {
		virtio_vdpa_queues_free(priv);
		return -ENOMEM;
	}

	for (i = 0; i < nr_vq; i++) {
		vr = rte_zmalloc_socket(NULL, sizeof(struct virtio_vdpa_vring_info),
								RTE_CACHE_LINE_SIZE,
								priv->pdev->device.numa_node);
		if (vr == NULL) {
			virtio_vdpa_queues_free(priv);
			return -ENOMEM;
		}
		priv->vrings[i] = vr;
		priv->vrings[i]->index = i;
		priv->vrings[i]->priv = priv;
	}
	return 0;
}

static int
virtio_vdpa_get_pf_name(const char *vf_name, char *pf_name, size_t pf_name_len)
{
	char pf_path[1024];
	char link[1024];
	int ret;

	if (!pf_name || !vf_name)
		return -EINVAL;

	snprintf(pf_path, 1024, "%s/%s/physfn", rte_pci_get_sysfs_path(), vf_name);
	memset(link, 0, sizeof(link));
	ret = readlink(pf_path, link, (sizeof(link)-1));
	if ((ret < 0) || ((unsigned int)ret > (sizeof(link)-1)))
		return -ENOENT;

	strlcpy(pf_name, &link[3], pf_name_len);
	DRV_LOG(DEBUG, "Link %s, vf name: %s pf name: %s", link, vf_name, pf_name);

	return 0;
}
#define VIRTIO_VDPA_MAX_VF 4096
static int
virtio_vdpa_get_vfid(const char *pf_name, const char *vf_name, int *vfid)
{
	char pf_path[1024];
	char link[1024];
	int ret, i;

	if (!pf_name || !vf_name)
		return -EINVAL;

	for(i = 0; i < VIRTIO_VDPA_MAX_VF; i++) {
		snprintf(pf_path, 1024, "%s/%s/virtfn%d", rte_pci_get_sysfs_path(), pf_name, i);
		memset(link, 0, sizeof(link));
		ret = readlink(pf_path, link, (sizeof(link)-1));
		if ((ret < 0) || ((unsigned int)ret > (sizeof(link)-1)))
			return -ENOENT;

		if (strcmp(&link[3], vf_name) == 0) {
			*vfid = i + 1;
			DRV_LOG(DEBUG, "Vf name: %s pf name: %s vfid %d", vf_name, pf_name, *vfid);
			return 0;
		}
	}
	DRV_LOG(DEBUG, "Vf name: %s pf name: %s can't get vfid", vf_name, pf_name);
	return -ENODEV;
}

static int
virtio_vdpa_dev_do_remove(struct rte_pci_device *pci_dev, struct virtio_vdpa_priv *priv)
{
	bool ret;

	if (!priv)
		return 0;

	if (priv->configured)
		virtio_vdpa_dev_close(priv->vid);

	if (priv->dev_work_flag == VIRTIO_VDPA_DEV_CLOSE_WORK_START) {
		DRV_LOG(ERR, "%s is waiting dev close work finish lcore:%d", pci_dev->name, priv->lcore_id);
		rte_eal_wait_lcore(priv->lcore_id);
	}

	if (priv->dev_work_flag == VIRTIO_VDPA_DEV_CLOSE_WORK_ERR) {
		DRV_LOG(ERR, "%s is dev close work had err", pci_dev->name);
	}

	if (priv->dev_ops && priv->dev_ops->unreg_dev_intr) {
		ret = virtio_pci_dev_interrupt_disable(priv->vpdev, 0);
		if (ret) {
			DRV_LOG(ERR, "%s error disabling virtio dev interrupts: %d (%s)",
					priv->vdev->device->name,
					ret, strerror(errno));
		}

		ret = priv->dev_ops->unreg_dev_intr(priv);
		if (ret) {
			DRV_LOG(ERR, "%s unregister dev interrupt fail ret:%d", pci_dev->name, ret);
		}
	}

	if (priv->lm_status == VIRTIO_S_FREEZED) {
		ret = virtio_vdpa_cmd_set_status(priv->pf_priv, priv->vf_id, VIRTIO_S_QUIESCED);
		if (ret) {
			DRV_LOG(ERR, "%s vfid %d failed unfreeze ret:%d", pci_dev->name, priv->vf_id, ret);
		}
		priv->lm_status = VIRTIO_S_QUIESCED;
	}

	if (priv->lm_status == VIRTIO_S_QUIESCED) {
		ret = virtio_vdpa_cmd_set_status(priv->pf_priv, priv->vf_id, VIRTIO_S_RUNNING);
		if (ret) {
			DRV_LOG(ERR, "%s vfid %d failed unquiesced ret:%d", pci_dev->name, priv->vf_id, ret);
		}
		priv->lm_status = VIRTIO_S_RUNNING;
	}

	if (priv->vdev)
		rte_vdpa_unregister_device(priv->vdev);

	if (priv->vpdev) {
		ret = virtio_pci_dev_interrupts_free(priv->vpdev);
		if (ret) {
			DRV_LOG(ERR, "Error free virtio dev interrupts: %s",
					strerror(errno));
		}
	}

	virtio_vdpa_queues_free(priv);

	if (priv->vpdev) {
		virtio_pci_dev_reset(priv->vpdev, VIRTIO_VDPA_REMOVE_RESET_TIME_OUT);
		virtio_pci_dev_free(priv->vpdev);
	}

	if (priv->vfio_container_fd  >= 0)
		rte_vfio_container_destroy(priv->vfio_container_fd);

	if (priv->state_mz)
		rte_memzone_free(priv->state_mz);

	if (priv->state_mz_remote)
		rte_memzone_free(priv->state_mz_remote);
	if (priv->vdpa_dp_map)
		rte_memzone_free(priv->vdpa_dp_map);
	rte_free(priv);

	return 0;
}

#define VIRTIO_VDPA_GET_GROUPE_RETRIES 120
static int
virtio_vdpa_dev_remove(struct rte_pci_device *pci_dev)
{
	struct virtio_vdpa_priv *priv = NULL;
	bool found = false;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &virtio_priv_list, next) {
		if (priv->pdev == pci_dev) {
			found = true;
			TAILQ_REMOVE(&virtio_priv_list, priv, next);
			break;
		}
	}
	pthread_mutex_unlock(&priv_list_lock);
	if (!found)
		return -ENODEV;
	return virtio_vdpa_dev_do_remove(pci_dev, priv);
}

static int
virtio_vdpa_dev_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	int vdpa = 0;
	int ret, fd, vf_id = 0, state_len;
	struct virtio_vdpa_priv *priv;
	char devname[RTE_DEV_NAME_MAX_LEN] = {0};
	char pfname[RTE_DEV_NAME_MAX_LEN] = {0};
	char mz_name[RTE_MEMZONE_NAMESIZE];
	int iommu_group_num;
	size_t mz_len;
	int retries = VIRTIO_VDPA_GET_GROUPE_RETRIES;

	rte_pci_device_name(&pci_dev->addr, devname, RTE_DEV_NAME_MAX_LEN);

	ret = virtio_pci_devargs_parse(pci_dev->device.devargs, &vdpa);
	if (ret < 0) {
		DRV_LOG(ERR, "Devargs parsing is failed %d dev:%s", ret, devname);
		return ret;
	}
	/* Virtio vdpa pmd skips probe if device needs to work in none vdpa mode */
	if (vdpa != 1)
		return 1;

	priv = rte_zmalloc("virtio vdpa device private", sizeof(*priv),
						RTE_CACHE_LINE_SIZE);
	if (!priv) {
		DRV_LOG(ERR, "Failed to allocate private memory %d dev:%s",
						ret, devname);
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	priv->pdev = pci_dev;

	ret = virtio_vdpa_get_pf_name(devname, pfname, sizeof(pfname));
	if (ret) {
		DRV_LOG(ERR, "%s failed to get pf name ret:%d", devname, ret);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	/* check pf_priv before use it, might be null if not set */
	priv->pf_priv = rte_vdpa_get_mi_by_bdf(pfname);;
	if (!priv->pf_priv) {
		DRV_LOG(ERR, "%s failed to get pf priv", devname);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	ret = virtio_vdpa_get_vfid(pfname, devname, &vf_id);
	if (ret) {
		DRV_LOG(ERR, "%s pf %s failed to get vfid ret:%d", devname, pfname, ret);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}
	priv->vf_id = vf_id;

	/* TO_DO: need to confirm following: */
	priv->vfio_dev_fd = -1;
	priv->vfio_group_fd = -1;
	priv->vfio_container_fd = -1;

    DRV_LOG(ERR, "yajun: PF container fd %d", priv->pf_priv->vfio_container_fd);
    if (priv->pf_priv->vfio_container_fd == 0) {
        priv->vfio_container_fd = rte_vfio_container_create();
        if (priv->vfio_container_fd < 0) {
            DRV_LOG(ERR, "%s failed to get container fd", devname);
            rte_errno = rte_errno ? rte_errno : EINVAL;
            goto error;
        }
        priv->pf_priv->vfio_container_fd = priv->vfio_container_fd;
        DRV_LOG(ERR, "yajun: container fd %d", priv->vfio_container_fd);
    }

	do {
		ret = rte_vfio_get_group_num(rte_pci_get_sysfs_path(), devname,
				&iommu_group_num);
		if (ret <= 0) {
			DRV_LOG(ERR, "%s failed to get IOMMU group ret:%d", devname, ret);
			rte_errno = rte_errno ? rte_errno : EINVAL;
			goto error;
		}

		DRV_LOG(INFO, "%s iommu_group_num:%d retries:%d", devname, iommu_group_num, retries);

		priv->vfio_group_fd = rte_vfio_container_group_bind(
				priv->pf_priv->vfio_container_fd, iommu_group_num);
		if (priv->vfio_group_fd < 0) {
			DRV_LOG(ERR, "%s failed to get group fd", devname);
			sleep(1);
			retries--;
		} else
			break;
		if (!retries)
			goto error;
	} while(retries);

	priv->vpdev = virtio_pci_dev_alloc(pci_dev);
	if (priv->vpdev == NULL) {
		DRV_LOG(ERR, "%s failed to alloc virito pci dev", devname);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	if (priv->pdev->id.device_id == VIRTIO_PCI_MODERN_DEVICEID_NET)
		priv->dev_ops = &virtio_vdpa_net_callback;
	else if (priv->pdev->id.device_id == VIRTIO_PCI_MODERN_DEVICEID_BLK)
		priv->dev_ops = &virtio_vdpa_blk_callback;

	priv->vfio_dev_fd = rte_intr_dev_fd_get(pci_dev->intr_handle);
	if (priv->vfio_dev_fd < 0) {
		DRV_LOG(ERR, "%s failed to get vfio dev fd", devname);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	priv->vdev = rte_vdpa_register_device(&pci_dev->device, &virtio_vdpa_ops);
	if (priv->vdev == NULL) {
		DRV_LOG(ERR, "%s failed to register vDPA device", devname);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	priv->hw_nr_virtqs = virtio_pci_dev_nr_vq_get(priv->vpdev);
	ret = virtio_vdpa_queues_alloc(priv);
	if (ret) {
		DRV_LOG(ERR, "%s failed to alloc vDPA device queues ret:%d",
					devname, ret);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	priv->nvec = virtio_pci_dev_interrupts_num_get(priv->vpdev);
	if (priv->nvec <= 0) {
		DRV_LOG(ERR, "%s error dev interrupts %d less than 0",
					devname, priv->nvec);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	ret = virtio_pci_dev_interrupts_alloc(priv->vpdev, priv->nvec);
	if (ret) {
		DRV_LOG(ERR, "%s error alloc virtio dev interrupts ret:%d %s",
					devname, ret, strerror(errno));
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	state_len = virtio_pci_dev_state_size_get(priv->vpdev);
	DRV_LOG(INFO, "%s state len:%d", devname, state_len);
	/*contoller use snap_dma_q_read to get data from host,len:
	*4096 --> can get all data
	*3800 --> only get data before 3792 byte
	*3796 --> only get data before 3792 byte
	*so,use 4k align RM:3216791 mail:SNAP dma read issue
	*/
	state_len = ((state_len -1 + 4096)/4096)*4096;
	DRV_LOG(INFO, "%s align state len:%d", devname, state_len);
	priv->state_size = state_len;
	priv->state_mz = rte_memzone_reserve_aligned(devname, state_len,
			priv->pdev->device.numa_node, RTE_MEMZONE_IOVA_CONTIG,
			VIRTIO_VDPA_STATE_ALIGN);
	if (priv->state_mz == NULL) {
		DRV_LOG(ERR, "Failed to reserve memzone dev:%s", devname);
		rte_errno = rte_errno ? rte_errno : ENOMEM;
		goto error;
	}

	mz_len = priv->state_mz->len;
	memset(priv->state_mz->addr, 0, mz_len);

	if (priv->dev_ops->reg_dev_intr) {
		ret = priv->dev_ops->reg_dev_intr(priv);
		if (ret) {
			DRV_LOG(ERR, "%s register dev interrupt fail ret:%d", devname, ret);
			goto error;
		}

		fd = rte_intr_fd_get(priv->pdev->intr_handle);
		ret = virtio_pci_dev_interrupt_enable(priv->vpdev, fd, 0);
		if (ret) {
			DRV_LOG(ERR, "%s error enabling virtio dev interrupts: %d(%s)",
					devname, ret, strerror(errno));
			rte_errno = rte_errno ? rte_errno : EINVAL;
			goto error;
		}
	}

	ret = virtio_pci_dev_state_bar_copy(priv->vpdev, priv->state_mz->addr, state_len);
	if (ret) {
		DRV_LOG(ERR, "%s error copy bar to state ret:%d",
					devname, ret);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	ret = virtio_vdpa_cmd_set_status(priv->pf_priv, priv->vf_id, VIRTIO_S_QUIESCED);
	if (ret) {
		DRV_LOG(ERR, "%s vfid %d failed suspend ret:%d", devname, priv->vf_id, ret);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	priv->lm_status = VIRTIO_S_QUIESCED;

	ret = virtio_vdpa_cmd_set_status(priv->pf_priv, priv->vf_id, VIRTIO_S_FREEZED);
	if (ret) {
		DRV_LOG(ERR, "%s vfid %d failed suspend ret:%d", devname, priv->vf_id, ret);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	priv->lm_status = VIRTIO_S_FREEZED;

	/* Init remote state mz */

	ret = snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "%s_remote_mz", devname);
	if (ret < 0 || ret >= RTE_MEMZONE_NAMESIZE) {
		DRV_LOG(ERR, "%s remote mem zone print fail ret:%d", devname, ret);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	priv->state_mz_remote = rte_memzone_reserve_aligned(mz_name,
			VIRTIO_VDPA_REMOTE_STATE_DEFAULT_SIZE,
			priv->pdev->device.numa_node, RTE_MEMZONE_IOVA_CONTIG,
			VIRTIO_VDPA_STATE_ALIGN);
	if (priv->state_mz_remote == NULL) {
		DRV_LOG(ERR, "Failed to reserve remote memzone dev:%s", devname);
		rte_errno = rte_errno ? rte_errno : ENOMEM;
		goto error;
	}

	mz_len = priv->state_mz_remote->len;
	memset(priv->state_mz_remote->addr, 0, mz_len);

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_INSERT_TAIL(&virtio_priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	return 0;

error:
	virtio_vdpa_dev_do_remove(pci_dev, priv);
	return -rte_errno;
}

int
virtio_vdpa_dev_pf_filter_dump(struct vdpa_vf_params *vf_info, int max_vf_num, struct virtio_vdpa_pf_priv *pf_priv)
{
	struct virtio_vdpa_priv *priv;
	int count = 0;

	if (!vf_info || !pf_priv)
		return -EINVAL;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &virtio_priv_list, next) {
		if (priv->pf_priv == pf_priv) {
			vf_info[count].msix_num = priv->nvec;
			vf_info[count].queue_num = priv->hw_nr_virtqs;
			vf_info[count].queue_size = priv->vrings[0]->size;
			vf_info[count].features = priv->guest_features;
			strlcpy(vf_info[count].vf_name, priv->vdev->device->name, RTE_DEV_NAME_MAX_LEN);
			count++;
			if (count >= max_vf_num)
				break;
		}
	}
	pthread_mutex_unlock(&priv_list_lock);

	return count;
}

int
virtio_vdpa_dev_vf_filter_dump(const char *vf_name, struct vdpa_vf_params *vf_info)
{
	struct virtio_vdpa_priv *priv;
	bool found = false;

	if (!vf_info)
		return -EINVAL;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &virtio_priv_list, next) {
		if (!strncmp(vf_name, priv->vdev->device->name, RTE_DEV_NAME_MAX_LEN)) {
			vf_info->msix_num = priv->nvec;
			vf_info->queue_num = priv->hw_nr_virtqs;
			vf_info->queue_size = priv->vrings[0]->size;
			vf_info->features = priv->guest_features;
			strlcpy(vf_info->vf_name, priv->vdev->device->name, RTE_DEV_NAME_MAX_LEN);
			found = true;
			break;
		}
	}
	pthread_mutex_unlock(&priv_list_lock);

	return found ? 0 : -EINVAL;
}

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_virtio_map[] = {
	{ RTE_PCI_DEVICE(VIRTIO_PCI_VENDORID, VIRTIO_PCI_MODERN_DEVICEID_NET) },
	{ RTE_PCI_DEVICE(VIRTIO_PCI_VENDORID, VIRTIO_PCI_MODERN_DEVICEID_BLK) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver virtio_vdpa_driver = {
	.id_table = pci_id_virtio_map,
	.drv_flags = 0,
	.probe = virtio_vdpa_dev_probe,
	.remove = virtio_vdpa_dev_remove,
};

RTE_PMD_REGISTER_PCI(VIRTIO_VDPA_DRIVER_NAME, virtio_vdpa_driver);
RTE_PMD_REGISTER_PCI_TABLE(VIRTIO_VDPA_DRIVER_NAME, pci_id_virtio_map);
RTE_PMD_REGISTER_KMOD_DEP(VIRTIO_VDPA_DRIVER_NAME, "* vfio-pci");
