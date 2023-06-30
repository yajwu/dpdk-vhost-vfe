/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */
#ifdef RTE_EXEC_ENV_LINUX
 #include <dirent.h>
 #include <fcntl.h>
#endif

#include <rte_io.h>
#include <rte_bus.h>
#include <rte_malloc.h>
#include <rte_eal_paging.h>

#include "virtio_pci.h"
#include "virtio_logs.h"
#include "virtqueue.h"
#include "virtio_pci_state.h"
#include "virtio_api.h"

struct virtio_net_dev_state {
	struct virtio_dev_common_state common_state;
	struct virtio_net_config net_dev_cfg;
	struct virtio_field_hdr rx_mod_hdr;
	union virtnet_rx_mode rxomd;
	struct virtio_dev_queue_info q_info[];
} __rte_packed;

static uint16_t
modern_net_get_queue_num(struct virtio_hw *hw)
{
	uint16_t nr_vq;

	if (virtio_dev_with_feature(hw, VIRTIO_NET_F_MQ) ||
			virtio_dev_with_feature(hw, VIRTIO_NET_F_RSS)) {
		VIRTIO_OPS(hw)->read_dev_cfg(hw,
			offsetof(struct virtio_net_config, max_virtqueue_pairs),
			&hw->max_queue_pairs,
			sizeof(hw->max_queue_pairs));
	} else {
		PMD_INIT_LOG(DEBUG,
				 "Neither VIRTIO_NET_F_MQ nor VIRTIO_NET_F_RSS are supported");
		hw->max_queue_pairs = 1;
	}

	nr_vq = hw->max_queue_pairs * 2;
	if (virtio_with_feature(hw, VIRTIO_NET_F_CTRL_VQ))
		nr_vq += 1;

	PMD_INIT_LOG(DEBUG, "Virtio net nr_vq is %d", nr_vq);
	return nr_vq;

}

static uint16_t
modern_net_get_dev_cfg_size(void)
{
	return sizeof(struct virtio_net_config);
}

static void *
modern_net_get_queue_offset(void *state)
{
	struct virtio_net_dev_state *state_net = state;

	return state_net->q_info;
}

static uint32_t
modern_net_get_state_size(uint16_t num_queues)
{
	return sizeof(struct virtio_net_config) + sizeof(struct virtio_dev_common_state) +
			num_queues * sizeof(struct virtio_dev_queue_info);
}

static void
modern_net_dev_cfg_dump(void *f_hdr)
{
	struct virtio_field_hdr *tmp_f_hdr= f_hdr;
	const struct virtio_net_config *dev_cfg;

	if (tmp_f_hdr->size < sizeof(struct virtio_net_config)) {
		PMD_DUMP_LOG(ERR, ">> net_config: state is truncated (%d < %lu)\n",
					tmp_f_hdr->size,
					sizeof(struct virtio_net_config));
		return;
	}

	dev_cfg = (struct virtio_net_config *)(tmp_f_hdr + 1);
	PMD_DUMP_LOG(INFO, ">> virtio_net_config, size:%d bytes \n", tmp_f_hdr->size);

	PMD_DUMP_LOG(INFO, ">>> mac: %02X:%02X:%02X:%02X:%02X:%02X status: 0x%x max_virtqueue_pairs: 0x%x mtu: 0x%x\n",
		  dev_cfg->mac[5], dev_cfg->mac[4], dev_cfg->mac[3], dev_cfg->mac[2], dev_cfg->mac[1], dev_cfg->mac[0],
		  dev_cfg->status, dev_cfg->max_virtqueue_pairs, dev_cfg->mtu);
}

static void
modern_net_dev_state_init(void *state)
{
	struct virtio_net_dev_state *state_net = state;

	/*Set to promisc mod, this is WA to fix ipv6 issue RM:3229948*/
	state_net->rx_mod_hdr.type = rte_cpu_to_le_32(VIRTIO_NET_RX_MODE_CFG);
	state_net->rx_mod_hdr.size = rte_cpu_to_le_32(sizeof(union virtnet_rx_mode));
	state_net->rxomd.promisc = 1;
	state_net->rxomd.val = rte_cpu_to_le_32(state_net->rxomd.val);
}

/* TODO make this function external */
static void
virtio_vdpa_init_vring(struct virtqueue *vq)
{
	uint8_t *ring_mem = vq->vq_ring_virt_mem;
	int size = vq->vq_nentries;
	struct vring *vr;

	PMD_INIT_LOG(DEBUG, ">>");

	memset(ring_mem, 0, vq->vq_ring_size);

	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_avail_idx = 0;
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;
	memset(vq->vq_descx, 0, sizeof(struct vq_desc_extra) * vq->vq_nentries);
	vr = &vq->vq_split.ring;

	vring_init_split(vr, ring_mem, VIRTIO_VRING_ALIGN, size);
	vring_desc_init_split(vr->desc, size);
	/*
	 * Disable device(host) interrupting guest
	 */
	virtqueue_disable_intr_split(vq);
}

static int
modern_net_create_cvq(struct virtio_hw *hw, int numa_node)
{
	const struct rte_memzone *mz = NULL, *hdr_mz = NULL;
	struct virtio_pci_dev *vpdev = virtio_pci_get_dev(hw);
	struct virtio_pci_dev_vring_info vr_info;
	char vq_hdr_name[VIRTQUEUE_MAX_NAME_SZ];
	char vq_name[VIRTQUEUE_MAX_NAME_SZ];
	struct virtnet_ctl *cvq = NULL;
	unsigned int vq_size, size;
	struct virtqueue *vq;
	size_t sz_hdr_mz = 0;
	uint16_t queue_idx;
	int ret;

	queue_idx = hw->max_queue_pairs * 2;

	hw->vqs = rte_zmalloc(NULL, sizeof(struct virtqueue *) * (queue_idx + 1) , 0);
	if (!hw->vqs) {
		PMD_INIT_LOG(ERR, "failed to allocate vqs");
		return -ENOMEM;
	}

	vq_size = virtio_pci_dev_queue_size_get(vpdev, queue_idx);
	PMD_INIT_LOG(INFO, "control queue idx %u, queue size %u, numa %d",
		queue_idx, vq_size, numa_node);

	snprintf(vq_name, sizeof(vq_name), "vdev%d_cvq%u",
		 vpdev->vfio_dev_fd, queue_idx);

	size = RTE_ALIGN_CEIL(sizeof(*vq) +
				vq_size * sizeof(struct vq_desc_extra),
				RTE_CACHE_LINE_SIZE);
	vq = rte_zmalloc_socket(vq_name, size, RTE_CACHE_LINE_SIZE,
				numa_node);
	if (vq == NULL) {
		PMD_INIT_LOG(ERR, "can not allocate control q %u", queue_idx);
		ret = -ENOMEM;
		goto err_free_vqs;
	}
	hw->vqs[queue_idx] = vq;

	vq->hw = hw;
	vq->vq_queue_index = queue_idx;
	vq->vq_nentries = vq_size;

	/*
	 * Reserve a memzone for vring elements
	 */
	size = vring_size(hw, vq_size, VIRTIO_VRING_ALIGN);
	vq->vq_ring_size = RTE_ALIGN_CEIL(size, VIRTIO_VRING_ALIGN);

	mz = rte_memzone_reserve_aligned(vq_name, vq->vq_ring_size,
			numa_node, RTE_MEMZONE_IOVA_CONTIG,
			VIRTIO_VRING_ALIGN);
	if (mz == NULL) {
		if (rte_errno == EEXIST)
			mz = rte_memzone_lookup(vq_name);
		if (mz == NULL) {
			ret = -ENOMEM;
			goto err_free_vq;
		}
	}

	memset(mz->addr, 0, mz->len);

	vq->vq_ring_mem = mz->iova;
	vq->vq_ring_virt_mem = mz->addr;

	virtio_vdpa_init_vring(vq);

	cvq = &vq->cq;
	cvq->mz = mz;

	/* Allocate a page for ctl vq command */
	sz_hdr_mz = rte_mem_page_size();

	if (sz_hdr_mz) {
		snprintf(vq_hdr_name, sizeof(vq_hdr_name), "vdev%d_cq%u_hdr",
				vpdev->vfio_dev_fd, queue_idx);
		hdr_mz = rte_memzone_reserve_aligned(vq_hdr_name, sz_hdr_mz,
				numa_node, RTE_MEMZONE_IOVA_CONTIG,
				RTE_CACHE_LINE_SIZE);
		if (hdr_mz == NULL) {
			if (rte_errno == EEXIST)
				hdr_mz = rte_memzone_lookup(vq_hdr_name);
			if (hdr_mz == NULL) {
				ret = -ENOMEM;
				goto err_free_mz;
			}
		}
		cvq->virtio_net_hdr_mz = hdr_mz;
		cvq->virtio_net_hdr_mem = hdr_mz->iova;
		memset(cvq->virtio_net_hdr_mz->addr, 0, sz_hdr_mz);
	} else {
		PMD_INIT_LOG(ERR, "rte mem page size is zero");
		ret = -EINVAL;
		goto err_free_mz;
	}

	hw->cvq = cvq;

	vr_info.size  = vq_size;
	vr_info.desc  = (uint64_t)(uintptr_t)vq->vq_split.ring.desc;
	vr_info.avail = (uint64_t)(uintptr_t)vq->vq_split.ring.avail;
	vr_info.used  = (uint64_t)(uintptr_t)vq->vq_split.ring.used;
	ret = virtio_pci_dev_queue_set(vpdev, queue_idx, &vr_info);
	if (ret) {
		PMD_INIT_LOG(ERR, "setup_queue %u failed", queue_idx);
		ret = -EINVAL;
		goto err_clean_cvq;
	}

	return 0;

err_clean_cvq:
	rte_memzone_free(hdr_mz);
	hw->cvq = NULL;
err_free_mz:
	rte_memzone_free(mz);
err_free_vq:
	rte_free(vq);
	hw->vqs[queue_idx] = NULL;
err_free_vqs:
	rte_free(hw->vqs);
	hw->vqs = NULL;
	return ret;
}

static void
virtio_vdpa_destroy_cq_ctl(struct virtnet_ctl *ctl)
{
	rte_memzone_free(ctl->mz);
	rte_memzone_free(ctl->virtio_net_hdr_mz);
}

/* seems no need, in virtio_pci_dev_queues_free */
static void
modern_net_destroy_cvq(struct virtio_hw *hw)
{
#if 0
	uint16_t nr_vq = priv->hw_nr_virtqs;
	struct virtqueue *vq;
	uint16_t i;

	if (hw->vqs == NULL)
		return;

	if (hw->cvq) {
		virtio_vdpa_destroy_cq_ctl(hw->cvq);
		hw->cvq = NULL;
	}

	for (i = 0; i < nr_vq; i++) {
		vq = hw->vqs[i];
		if (vq) {
			rte_free(vq);
			hw->vqs[i] = NULL;
		}
	}
	rte_free(hw->vqs);
	hw->vqs = NULL;
#endif
}

const struct virtio_dev_specific_ops virtio_net_dev_pci_modern_ops = {
	.get_queue_num = modern_net_get_queue_num,
	.get_dev_cfg_size = modern_net_get_dev_cfg_size,
	.get_queue_offset = modern_net_get_queue_offset,
	.get_state_size = modern_net_get_state_size,
	.dev_cfg_dump = modern_net_dev_cfg_dump,
	.dev_state_init = modern_net_dev_state_init,
	.dev_create_cvq = modern_net_create_cvq,
	.dev_destroy_cvq = modern_net_destroy_cvq,
};
