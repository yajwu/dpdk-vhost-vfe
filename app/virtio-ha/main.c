/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024, NVIDIA CORPORATION & AFFILIATES.
 */

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <linux/vfio.h>
#include <unistd.h>
#include <syslog.h>
#include <inttypes.h>

#include <rte_log.h>

#include <virtio_ha.h>

RTE_LOG_REGISTER(virtio_ha_app_logtype, test.ha, INFO);

#define HA_APP_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_ha_app_logtype, \
		"VIRTIO HA APP %s(): " fmt "\n", __func__, ##args)

enum ha_msg_hdlr_res {
	HA_MSG_HDLR_ERR = 0, /* Message handling error */
	HA_MSG_HDLR_SUCCESS = 1, /* Message handling success */
	HA_MSG_HDLR_REPLY = 2, /* Message handling success and need reply */
};

typedef void (*fd_cb)(int fd, void *data);
typedef int (*ha_message_handler_t)(struct virtio_ha_msg *msg);

struct ha_event_handler {
	void *data;
	int sock;
	fd_cb cb;
};

static struct virtio_ha_device_list hs;
static struct ha_event_handler msg_hdlr;
static struct virtio_ha_msg *msg;

static int
ha_server_app_query_pf_list(struct virtio_ha_msg *msg)
{
	struct virtio_ha_pf_dev *dev;
	struct virtio_dev_name *pf_name;
	struct virtio_ha_pf_dev_list *list = &hs.pf_list;
	uint32_t i = 0;

	if (hs.nr_pf == 0)
		return HA_MSG_HDLR_REPLY;

	msg->iov.iov_len = hs.nr_pf * sizeof(struct virtio_dev_name);
	msg->iov.iov_base = malloc(msg->iov.iov_len);
	if (msg->iov.iov_base == NULL) {
		HA_APP_LOG(ERR, "Failed to alloc pf list");
		return HA_MSG_HDLR_ERR;
	}

	pf_name = (struct virtio_dev_name *)msg->iov.iov_base;
	TAILQ_FOREACH(dev, list, next) {
		memcpy(pf_name + i, &dev->pf_name, sizeof(struct virtio_dev_name));
		i++;
	}

	msg->hdr.size = msg->iov.iov_len;
	HA_APP_LOG(INFO, "Got pf list query and reply with %u pf", hs.nr_pf);

	return HA_MSG_HDLR_REPLY;
}

static int
ha_server_app_query_vf_list(struct virtio_ha_msg *msg)
{
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	struct vdpa_vf_with_devargs *vf;
	struct virtio_ha_pf_dev_list *list = &hs.pf_list;
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	uint32_t nr_vf, i = 0;

	TAILQ_FOREACH(dev, list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, msg->hdr.bdf)) {
			vf_list = &dev->vf_list;
			nr_vf = dev->nr_vf;
			break;
		}
	}

	if (vf_list == NULL || nr_vf == 0)
		return HA_MSG_HDLR_REPLY;

	msg->iov.iov_len = nr_vf * sizeof(struct vdpa_vf_with_devargs);
	msg->iov.iov_base = malloc(msg->iov.iov_len);
	if (msg->iov.iov_base == NULL) {
		HA_APP_LOG(ERR, "Failed to alloc vf list");
		return HA_MSG_HDLR_ERR;
	}

	vf = (struct vdpa_vf_with_devargs *)msg->iov.iov_base;
	TAILQ_FOREACH(vf_dev, vf_list, next) {
		memcpy(vf + i, &vf_dev->vf_devargs, sizeof(struct vdpa_vf_with_devargs));
		i++;		
	}

	msg->hdr.size = msg->iov.iov_len;
	HA_APP_LOG(INFO, "Got vf list query of pf %s and reply with %u vf", msg->hdr.bdf, nr_vf);

	return HA_MSG_HDLR_REPLY;
}

static int
ha_server_app_query_pf_ctx(struct virtio_ha_msg *msg)
{
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_pf_dev_list *list = &hs.pf_list;

	if (hs.nr_pf == 0)
		return HA_MSG_HDLR_REPLY;

	TAILQ_FOREACH(dev, list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, msg->hdr.bdf)) {
			msg->nr_fds = 2;
			msg->fds[0] = dev->pf_ctx.vfio_group_fd;
			msg->fds[1] = dev->pf_ctx.vfio_device_fd;
			HA_APP_LOG(INFO, "Got pf %s ctx query and reply with group fd %d and device fd %d",
				msg->hdr.bdf, msg->fds[0], msg->fds[1]);
			break;
		}
	}

	return HA_MSG_HDLR_REPLY;
}

static int
ha_server_app_query_vf_ctx(struct virtio_ha_msg *msg)
{
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	struct vdpa_vf_with_devargs *vf;
	struct virtio_ha_pf_dev_list *list = &hs.pf_list;
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	uint32_t nr_vf;

	TAILQ_FOREACH(dev, list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, msg->hdr.bdf)) {
			vf_list = &dev->vf_list;
			nr_vf = dev->nr_vf;
			break;
		}
	}

	if (vf_list == NULL || nr_vf == 0)
		return HA_MSG_HDLR_REPLY;

	vf = (struct vdpa_vf_with_devargs *)msg->iov.iov_base;
	TAILQ_FOREACH(vf_dev, vf_list, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf->vf_name.dev_bdf)) {
			msg->iov.iov_len = sizeof(struct virtio_vdpa_dma_mem) +
				vf_dev->vf_ctx.mem.nregions * sizeof(struct virtio_vdpa_mem_region);
			msg->iov.iov_base = malloc(msg->iov.iov_len);
			if (msg->iov.iov_base == NULL) {
				HA_APP_LOG(ERR, "Failed to alloc vf mem table");
				return HA_MSG_HDLR_ERR;
			}
			memcpy(msg->iov.iov_base, &vf_dev->vf_ctx.mem, msg->iov.iov_len);
			msg->nr_fds = 3;
			msg->fds[0] = vf_dev->vf_ctx.vfio_container_fd;
			msg->fds[1] = vf_dev->vf_ctx.vfio_group_fd;
			msg->fds[2] = vf_dev->vf_ctx.vfio_device_fd;
			close(vf_dev->vhost_fd);
			vf_dev->vhost_fd = -1;
			HA_APP_LOG(INFO, "Got vf %s ctx query and reply with container fd %d group fd %d "
				"and device fd %d", vf->vf_name.dev_bdf, msg->fds[0], msg->fds[1], msg->fds[2]);
			break;
		}
	}

	msg->hdr.size = msg->iov.iov_len;

	return HA_MSG_HDLR_REPLY;
}

static int
ha_server_pf_store_ctx(struct virtio_ha_msg *msg)
{
	struct virtio_ha_pf_dev_list *list = &hs.pf_list;
	struct virtio_ha_pf_dev *dev;

	if (msg->nr_fds != 2)
		return HA_MSG_HDLR_SUCCESS;

	/* Assume HA client will not re-set ctx */
	dev = malloc(sizeof(struct virtio_ha_pf_dev));
	if (dev == NULL) {
		HA_APP_LOG(ERR, "Failed to alloc pf device");
		return HA_MSG_HDLR_ERR;
	}

	memset(dev, 0, sizeof(struct virtio_ha_pf_dev));
	TAILQ_INIT(&dev->vf_list);
	dev->nr_vf = 0;
	strncpy(dev->pf_name.dev_bdf, msg->hdr.bdf, PCI_PRI_STR_SIZE);
	dev->pf_ctx.vfio_group_fd = msg->fds[0];
	dev->pf_ctx.vfio_device_fd = msg->fds[1];

	TAILQ_INSERT_TAIL(list, dev, next);
	hs.nr_pf++;
	HA_APP_LOG(INFO, "Stored pf %s ctx: group fd %d, device fd %d",
		msg->hdr.bdf, msg->fds[0], msg->fds[1]);

	return HA_MSG_HDLR_SUCCESS;
}

static int
ha_server_pf_remove_ctx(struct virtio_ha_msg *msg)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev_list *list = &hs.pf_list;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	bool found = false;

	TAILQ_FOREACH(dev, list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, msg->hdr.bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return HA_MSG_HDLR_SUCCESS;

	if (vf_list) {
		TAILQ_FOREACH(vf_dev, vf_list, next) {
			close(vf_dev->vf_ctx.vfio_device_fd);
			close(vf_dev->vf_ctx.vfio_group_fd);
			close(vf_dev->vf_ctx.vfio_container_fd);
			free(vf_dev);
		}
	}

	hs.nr_pf--;
	TAILQ_REMOVE(list, dev, next);

	HA_APP_LOG(INFO, "Removed pf %s ctx with %u vf: group fd %d, device fd %d",
		msg->hdr.bdf, dev->nr_vf, dev->pf_ctx.vfio_group_fd, dev->pf_ctx.vfio_device_fd);
	close(dev->pf_ctx.vfio_device_fd);
	close(dev->pf_ctx.vfio_group_fd);
	free(dev);

	return HA_MSG_HDLR_SUCCESS;
}

static int
ha_server_vf_store_devarg_vfio_fds(struct virtio_ha_msg *msg)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev_list *list = &hs.pf_list;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	size_t len;
	bool found = false;

	if (msg->nr_fds != 3)
		return HA_MSG_HDLR_SUCCESS;

	TAILQ_FOREACH(dev, list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, msg->hdr.bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return HA_MSG_HDLR_SUCCESS;

	/* To avoid memory realloc when mem table entry number changes, alloc for max entry num */
	len = sizeof(struct virtio_ha_vf_dev) +
		sizeof(struct virtio_vdpa_mem_region) * VIRTIO_HA_MAX_MEM_REGIONS;
	vf_dev = malloc(len);
	if (vf_dev == NULL) {
		HA_APP_LOG(ERR, "Failed to alloc vf device");
		return HA_MSG_HDLR_ERR;
	}

	memset(vf_dev, 0, len);
	memcpy(&vf_dev->vf_devargs, msg->iov.iov_base, msg->iov.iov_len);
	vf_dev->vf_ctx.vfio_container_fd = msg->fds[0];
	vf_dev->vf_ctx.vfio_group_fd = msg->fds[1];
	vf_dev->vf_ctx.vfio_device_fd = msg->fds[2];
	vf_dev->vhost_fd = -1;
	HA_APP_LOG(INFO, "Stored vf %s", vf_dev->vf_devargs.vf_name.dev_bdf);
	HA_APP_LOG(INFO, "vf %s: sock %s, vm_uuid %s", vf_dev->vf_devargs.vf_name.dev_bdf,
		vf_dev->vf_devargs.vhost_sock_addr, vf_dev->vf_devargs.vm_uuid);
	HA_APP_LOG(INFO, "vf %s: container fd %d, group fd %d, device fd %d",
		vf_dev->vf_devargs.vf_name.dev_bdf, msg->fds[0], msg->fds[1], msg->fds[2]);


	TAILQ_INSERT_TAIL(vf_list, vf_dev, next);
	dev->nr_vf++;

	return HA_MSG_HDLR_SUCCESS;
}

static int
ha_server_store_vhost_fd(struct virtio_ha_msg *msg)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev_list *list = &hs.pf_list;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	struct virtio_dev_name *vf_name;
	bool found = false;

	if (msg->nr_fds != 1)
		return HA_MSG_HDLR_SUCCESS;

	TAILQ_FOREACH(dev, list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, msg->hdr.bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return HA_MSG_HDLR_SUCCESS;

	vf_name = (struct virtio_dev_name *)msg->iov.iov_base;
	TAILQ_FOREACH(vf_dev, vf_list, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf_name->dev_bdf)) {
			vf_dev->vhost_fd = msg->fds[0];
			HA_APP_LOG(INFO, "Stored vf %s vhost fd %d", vf_name->dev_bdf, msg->fds[0]);
			break;
		}
	}

	return HA_MSG_HDLR_SUCCESS;
}

static int
ha_server_store_dma_tbl(struct virtio_ha_msg *msg)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev_list *list = &hs.pf_list;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	struct virtio_dev_name *vf_name;
	struct virtio_vdpa_dma_mem *mem;
	size_t len;
	bool found = false;
	uint32_t i;

	TAILQ_FOREACH(dev, list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, msg->hdr.bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return HA_MSG_HDLR_SUCCESS;

	vf_name = (struct virtio_dev_name *)msg->iov.iov_base;
	len = msg->iov.iov_len - sizeof(struct virtio_dev_name);
	TAILQ_FOREACH(vf_dev, vf_list, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf_name->dev_bdf)) {
			mem = (struct virtio_vdpa_dma_mem *)(vf_name + 1);
			memcpy(&vf_dev->vf_ctx.mem, mem, len);
			HA_APP_LOG(INFO, "Stored vf %s DMA memory table:", vf_name->dev_bdf);
			for (i = 0; i < mem->nregions; i++) {
				HA_APP_LOG(INFO, "Region %u: GPA 0x%" PRIx64 " HPA 0x%" PRIx64 " Size 0x%" PRIx64,
					i, mem->regions[i].guest_phys_addr, mem->regions[i].host_phys_addr,
					mem->regions[i].size);
			}
			break;
		}
	}

	return HA_MSG_HDLR_SUCCESS;
}

static int
ha_server_remove_devarg_vfio_fds(struct virtio_ha_msg *msg)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev_list *list = &hs.pf_list;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	struct virtio_dev_name *vf_name;
	bool found = false;

	TAILQ_FOREACH(dev, list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, msg->hdr.bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return HA_MSG_HDLR_SUCCESS;

	found = false;
	vf_name = (struct virtio_dev_name *)msg->iov.iov_base;
	TAILQ_FOREACH(vf_dev, vf_list, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf_name->dev_bdf)) {
			found = true;
			break;
		}
	}

	if (found) {
		HA_APP_LOG(INFO, "Removed vf %s ctx: container fd %d, group fd %d, device fd %d",
			vf_name->dev_bdf, vf_dev->vf_ctx.vfio_container_fd, vf_dev->vf_ctx.vfio_group_fd,
			vf_dev->vf_ctx.vfio_device_fd);
		dev->nr_vf--;
		TAILQ_REMOVE(vf_list, vf_dev, next);
		close(vf_dev->vf_ctx.vfio_device_fd);
		close(vf_dev->vf_ctx.vfio_group_fd);
		close(vf_dev->vf_ctx.vfio_container_fd);
		free(vf_dev);
	}

	return HA_MSG_HDLR_SUCCESS;
}

static int
ha_server_remove_vhost_fd(struct virtio_ha_msg *msg)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev_list *list = &hs.pf_list;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	struct virtio_dev_name *vf_name;
	bool found = false;

	TAILQ_FOREACH(dev, list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, msg->hdr.bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return HA_MSG_HDLR_SUCCESS;

	vf_name = (struct virtio_dev_name *)msg->iov.iov_base;
	TAILQ_FOREACH(vf_dev, vf_list, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf_name->dev_bdf)) {
			HA_APP_LOG(INFO, "Removed vf %s vhost fd %d", vf_name->dev_bdf, vf_dev->vhost_fd);
			close(vf_dev->vhost_fd);
			vf_dev->vhost_fd = -1;
			break;
		}
	}

	return HA_MSG_HDLR_SUCCESS;
}

static int
ha_server_remove_dma_tbl(struct virtio_ha_msg *msg)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev_list *list = &hs.pf_list;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	struct virtio_dev_name *vf_name;
	struct virtio_vdpa_dma_mem *mem;
	bool found = false;

	TAILQ_FOREACH(dev, list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, msg->hdr.bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return HA_MSG_HDLR_SUCCESS;

	vf_name = (struct virtio_dev_name *)msg->iov.iov_base;
	TAILQ_FOREACH(vf_dev, vf_list, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf_name->dev_bdf)) {
			mem = &vf_dev->vf_ctx.mem;
			mem->nregions = 0;
			HA_APP_LOG(INFO, "Removed vf %s DMA memory table", vf_name->dev_bdf);
			break;
		}
	}

	return HA_MSG_HDLR_SUCCESS;
}

static int
ha_server_store_global_cfd(struct virtio_ha_msg *msg)
{
	if (msg->nr_fds != 1)
		return HA_MSG_HDLR_SUCCESS;

	hs.global_cfd = msg->fds[0];
	HA_APP_LOG(INFO, "Saved global container fd: %d", hs.global_cfd);

	return HA_MSG_HDLR_SUCCESS;
}

static int
ha_server_query_global_cfd(struct virtio_ha_msg *msg)
{
	if (hs.global_cfd == -1)
		return HA_MSG_HDLR_REPLY;

	msg->nr_fds = 1;
	msg->fds[0] = hs.global_cfd;
	HA_APP_LOG(INFO, "Got query and replied with global container fd: %d", hs.global_cfd);

	return HA_MSG_HDLR_REPLY;
}

static int
ha_server_global_store_dma_map(struct virtio_ha_msg *msg)
{
	struct virtio_ha_global_dma_entry *entry;
	struct virtio_ha_global_dma_map *map;
	bool found = false;

	map = (struct virtio_ha_global_dma_map *)msg->iov.iov_base;
	TAILQ_FOREACH(entry, &hs.dma_tbl, next) {
		/* vDPA application should not send entries that have the same iova but different size */
		if (map->iova == entry->map.iova) {
			found = true;
			break;
		}
	}

	if (!found) {
		entry = malloc(sizeof(struct virtio_ha_global_dma_entry));
		if (!entry) {
			HA_APP_LOG(ERR, "Failed to alloc dma entry");
			return HA_MSG_HDLR_SUCCESS;
		}
		memcpy(&entry->map, map, sizeof(struct virtio_ha_global_dma_map));
		TAILQ_INSERT_TAIL(&hs.dma_tbl, entry, next);
	}

	HA_APP_LOG(INFO, "Saved global dma map: iova(0x%" PRIx64 "), len(0x%" PRIx64 ")",
		map->iova, map->size);

	return HA_MSG_HDLR_SUCCESS;
}

static int
ha_server_global_remove_dma_map(struct virtio_ha_msg *msg)
{
	struct virtio_ha_global_dma_entry *entry;
	struct virtio_ha_global_dma_map *map;
	bool found = false;

	map = (struct virtio_ha_global_dma_map *)msg->iov.iov_base;
	TAILQ_FOREACH(entry, &hs.dma_tbl, next) {
		/* vDPA application should not send entries that have the same iova but different size */
		if (map->iova == entry->map.iova) {
			found = true;
			break;
		}
	}

	if (found) {
		TAILQ_REMOVE(&hs.dma_tbl, entry, next);
		free(entry);
	}

	HA_APP_LOG(INFO, "Removed global dma map: iova(0x%" PRIx64 "), len(0x%" PRIx64 ")",
		map->iova, map->size);

	return HA_MSG_HDLR_SUCCESS;
}

static void
ha_server_cleanup_global_dma(void)
{
	struct virtio_ha_global_dma_entry *entry, *next;
	struct vfio_iommu_type1_dma_unmap dma_unmap = {};
	int ret;

	for (entry = TAILQ_FIRST(&hs.dma_tbl);
		 entry != NULL; entry = next) {
		next = TAILQ_NEXT(entry, next);
		dma_unmap.argsz = sizeof(struct vfio_iommu_type1_dma_unmap);
		dma_unmap.size = entry->map.size;
		dma_unmap.iova = entry->map.iova;
		ret = ioctl(hs.global_cfd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret) {
			HA_APP_LOG(ERR, "Cannot clear DMA remapping");
		} else if (dma_unmap.size != entry->map.size) {
			HA_APP_LOG(ERR, "Unexpected size 0x%" PRIx64
				" of DMA remapping cleared instead of 0x%" PRIx64,
				(uint64_t)dma_unmap.size, entry->map.size);
		} else {
			HA_APP_LOG(INFO, "Clean up global dma map: iova(0x%" PRIx64 "), len(0x%" PRIx64 ")",
				entry->map.iova, entry->map.size);
		}

		TAILQ_REMOVE(&hs.dma_tbl, entry, next);
		free(entry);
	}
}

static ha_message_handler_t ha_message_handlers[VIRTIO_HA_MESSAGE_MAX] = {
	[VIRTIO_HA_APP_QUERY_PF_LIST] = ha_server_app_query_pf_list,
	[VIRTIO_HA_APP_QUERY_VF_LIST] = ha_server_app_query_vf_list,
	[VIRTIO_HA_APP_QUERY_PF_CTX] = ha_server_app_query_pf_ctx,
	[VIRTIO_HA_APP_QUERY_VF_CTX] = ha_server_app_query_vf_ctx,
	[VIRTIO_HA_PF_STORE_CTX] = ha_server_pf_store_ctx,
	[VIRTIO_HA_PF_REMOVE_CTX] = ha_server_pf_remove_ctx,
	[VIRTIO_HA_VF_STORE_DEVARG_VFIO_FDS] = ha_server_vf_store_devarg_vfio_fds,
	[VIRTIO_HA_VF_STORE_VHOST_FD] = ha_server_store_vhost_fd,
	[VIRTIO_HA_VF_STORE_DMA_TBL] = ha_server_store_dma_tbl,
	[VIRTIO_HA_VF_REMOVE_DEVARG_VFIO_FDS] = ha_server_remove_devarg_vfio_fds,
	[VIRTIO_HA_VF_REMOVE_VHOST_FD] = ha_server_remove_vhost_fd,
	[VIRTIO_HA_VF_REMOVE_DMA_TBL] = ha_server_remove_dma_tbl,
	[VIRTIO_HA_GLOBAL_STORE_CONTAINER] = ha_server_store_global_cfd,
	[VIRTIO_HA_GLOBAL_QUERY_CONTAINER] = ha_server_query_global_cfd,
	[VIRTIO_HA_GLOBAL_STORE_DMA_MAP] = ha_server_global_store_dma_map,
	[VIRTIO_HA_GLOBAL_REMOVE_DMA_MAP] = ha_server_global_remove_dma_map,
};

static void
ha_message_handler(int fd, __attribute__((__unused__)) void *data)
{
	int ret;

	virtio_ha_reset_msg(msg);

	ret = virtio_ha_recv_msg(fd, msg);
	if (ret <= 0) {
		if (ret < 0)
			HA_APP_LOG(ERR, "Failed to recv ha msg");
		else
			HA_APP_LOG(ERR, "Client closed");
		return;
	}

	ret = ha_message_handlers[msg->hdr.type](msg);
	switch (ret) {
	case HA_MSG_HDLR_ERR:
	case HA_MSG_HDLR_SUCCESS:
		break;
	case HA_MSG_HDLR_REPLY:
		ret = virtio_ha_send_msg(fd, msg);
		if (ret < 0)
			HA_APP_LOG(ERR, "Failed to send ha msg");	
	default:
		break;
	}

	if (msg->iov.iov_len != 0)
		free(msg->iov.iov_base);

	return;
}

static void
add_connection(int fd, void *data)
{
	struct epoll_event event;
	int sock, epfd;

	sock = accept(fd, NULL, NULL);
	if (sock < 0) {
		HA_APP_LOG(ERR, "Failed to accept connection");
		return;
	}

	msg_hdlr.sock = sock;
	msg_hdlr.cb = ha_message_handler;
	msg_hdlr.data = NULL;

	epfd = *(int *)data;
	event.events = EPOLLIN | EPOLLHUP | EPOLLERR;
	event.data.ptr = &msg_hdlr;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &event) < 0)
		HA_APP_LOG(ERR, "Failed to epoll ctl add for message");

	return;
}

int
main(__attribute__((__unused__)) int argc, __attribute__((__unused__)) char *argv[])
{
	struct sockaddr_un addr;
	struct epoll_event event, ev[2];
	struct ha_event_handler hdl, *handler;
	int sock, epfd, nev, i;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_APP_LOG(ERR, "Failed to alloc ha msg");
		return -1;
	}

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		HA_APP_LOG(ERR, "Failed to create socket");
		goto err;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, VIRTIO_HA_UDS_PATH);
	unlink(VIRTIO_HA_UDS_PATH);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		HA_APP_LOG(ERR, "Failed to bind socket");
		goto err;
	}

	if (listen(sock, 5) < 0) {
		HA_APP_LOG(ERR, "Failed on socket listen");
		goto err;
	}

	epfd = epoll_create(1);
	if (epfd < 0) {
		HA_APP_LOG(ERR, "Failed to create epoll fd");
		goto err;
	}

	TAILQ_INIT(&hs.pf_list);
	TAILQ_INIT(&hs.dma_tbl);
	hs.nr_pf = 0;
	hs.global_cfd = -1;

	hdl.sock = sock;
	hdl.cb = add_connection;
	hdl.data = &epfd;
	event.events = EPOLLIN | EPOLLHUP | EPOLLERR;
	event.data.ptr = &hdl;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &event) < 0) {
		HA_APP_LOG(ERR, "Failed to epoll ctl add for connection");
		goto err;
	}

	HA_APP_LOG(INFO, "HA server init success");

	while (1) {
		nev = epoll_wait(epfd, ev, 2, -1);
		for (i = 0; i < nev; i++) {
			handler = (struct ha_event_handler *)ev[i].data.ptr;
			if ((ev[i].events & EPOLLERR) || (ev[i].events & EPOLLHUP)) {
				if (epoll_ctl(epfd, EPOLL_CTL_DEL, handler->sock, &ev[i]) < 0)
					HA_APP_LOG(ERR, "Failed to epoll ctl del for fd %d", handler->sock);
				close(handler->sock);
				ha_server_cleanup_global_dma();
			} else { /* EPOLLIN */
				handler->cb(handler->sock, handler->data);
			}
		}
	}

	return 0;

err:
	virtio_ha_free_msg(msg);
	return -1;
}
