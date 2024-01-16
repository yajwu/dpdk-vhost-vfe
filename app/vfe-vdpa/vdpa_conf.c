/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024, NVIDIA CORPORATION & AFFILIATES.
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <rte_ethdev.h>
#include <virtio_api.h>
#include <virtio_lm.h>
#include <rte_vf_rpc.h>

#include "vdpa_rpc.h"
#include "vdpa_conf.h"

#define RTE_LOGTYPE_VDPA RTE_LOGTYPE_USER1
#define DEFAULT_CONF_FILE_PATH "/etc/vfe-vhostd/vfe-vhostd.conf"

#define DEFAULT_WAIT_TIME_IN_MS 3000

static void set_default_conf(struct service_conf *conf)
{
	conf->valid = false;
	conf->pf_list.len = 0;
	conf->pf_list.pfs = NULL;
	conf->wait_in_ms = DEFAULT_WAIT_TIME_IN_MS;
}


void service_free_conf(struct service_conf *conf)
{
	struct pf_conf *pf;
	struct vf_conf *vf;
	int pf_idx;
	int vf_idx;

	for (pf_idx = 0; pf_idx < conf->pf_list.len; pf_idx++) {
		pf = &conf->pf_list.pfs[pf_idx];
		free(pf->slot);
		for (vf_idx = 0; vf_idx < pf->vf_list.len; vf_idx++) {
			vf = &pf->vf_list.vfs[vf_idx];
			if (vf->slot)
				free(vf->slot);
			if (vf->vhost_socket)
				free(vf->vhost_socket);
			if (vf->uuid)
				free(vf->uuid);
		}
		if (pf->vf_list.vfs)
			free(pf->vf_list.vfs);
	}
	if (conf->pf_list.pfs)
		free(conf->pf_list.pfs);

	set_default_conf(conf);
}

static int get_vf_list(cJSON *vfs, struct vf_list *list)
{
	int size = cJSON_GetArraySize(vfs);
	struct vf_conf *vf_conf;
	cJSON *vf_json;
	cJSON *slot;
	cJSON *vhost_socket;
	cJSON *vm_uuid;
	int i;

	list->vfs = calloc(sizeof(struct vf_conf), size);
	if (!list->vfs)
		return -ENOMEM;
	list->len = 0;
	for (i = 0; i < size; i++) {
		vf_json = cJSON_GetArrayItem(vfs, i);
		vf_conf = &list->vfs[list->len];
		slot = cJSON_GetObjectItem(vf_json, "slot");
		vhost_socket = cJSON_GetObjectItem(vf_json, "vhost_socket");
		vm_uuid = cJSON_GetObjectItem(vf_json, "vm_uuid");
		if (slot && vhost_socket) {
			vf_conf->slot = strdup(slot->valuestring);
			vf_conf->vhost_socket = strdup(vhost_socket->valuestring);
			vf_conf->uuid = vm_uuid ? strdup(vm_uuid->valuestring) : NULL;
			list->len++;
		} else {
			RTE_LOG(ERR, VDPA, "VF slot is missing in conf, skip.\n");
		}
	}

	return 0;
}

static int get_pf_list(cJSON *devices, struct pf_list *list, int default_wait)
{
	int size = cJSON_GetArraySize(devices);
	struct pf_conf *pf_conf;
	cJSON *pf_json;
	cJSON *slot;
	cJSON *wait_in_ms;
	cJSON *vfs;
	int ret = 0, i;

	list->pfs = calloc(sizeof(struct pf_conf), size);
	if (!list->pfs)
		return -ENOMEM;
	list->len = 0;

	for (i = 0; i < size; i++) {
		pf_json = cJSON_GetArrayItem(devices, i);
		slot = cJSON_GetObjectItem(pf_json, "pf");
		wait_in_ms = cJSON_GetObjectItem(pf_json, "wait_in_ms");
		vfs =  cJSON_GetObjectItem(pf_json, "vfs");
		pf_conf = &list->pfs[list->len];
		if (slot) {
			ret = get_vf_list(vfs, &pf_conf->vf_list);
			if (ret)
				goto out_free;
			pf_conf->slot = strdup(slot->valuestring);
			pf_conf->wait_in_ms = wait_in_ms ?
				atoi(wait_in_ms->valuestring) : default_wait;
			list->len++;
		} else {
			RTE_LOG(ERR, VDPA, "PF slot is missing in conf, skip.\n");
		}
	}
	return ret;

out_free:
	free(list->pfs);
	list->pfs = NULL;
	return ret;
}

int service_load_conf(struct service_conf *conf)
{
	cJSON *devices;
	cJSON *wait_in_ms;
	cJSON *conf_json;
	char *buffer = NULL;
	FILE *fp = NULL;
	long lSize = 0;
	int ret = 0;

	if (!conf) {
		ret = -EINVAL;
		goto out;
	}

	set_default_conf(conf);
	fp = fopen(DEFAULT_CONF_FILE_PATH, "rb");
	if (!fp) {
		RTE_LOG(WARNING, VDPA, "No %s found.\n", DEFAULT_CONF_FILE_PATH);
		goto out;
	}

	fseek(fp, 0L, SEEK_END);
	lSize = ftell(fp);
	rewind(fp);

	/* allocate memory for entire content */
	buffer = calloc(1, lSize + 1);
	if (!buffer) {
		ret = -ENOMEM;
		RTE_LOG(ERR, VDPA, "Memory allocation failed");
		goto out_close;
	}

	/* copy the file into the buffer */
	if (1 != fread(buffer, lSize, 1, fp)) {
		ret = errno;
		RTE_LOG(ERR, VDPA, "Read config file failed");
		goto out_free;
	}

	cJSON *jsoncfg = cJSON_Parse(buffer);
	if (!jsoncfg) {
		ret = -EINVAL;
		RTE_LOG(ERR, VDPA, "Wrong config format");
		goto out_free;
	}

	conf_json = cJSON_GetObjectItem(jsoncfg, "conf");
	if (conf_json) {
		wait_in_ms = cJSON_GetObjectItem(conf_json, "wait_in_ms");
		if (wait_in_ms)
			conf->wait_in_ms = atoi(wait_in_ms->valuestring);
	}

	devices = cJSON_GetObjectItem(jsoncfg, "devices");
	if (devices) {
		ret = get_pf_list(devices, &conf->pf_list, conf->wait_in_ms);
		if (ret) {
			RTE_LOG(ERR, VDPA, "Get PF list failed.\n");
			goto out_free_json;
		}
	}
	conf->valid = true;

	cJSON_Delete(jsoncfg);
	return ret;

out_free_json:
	cJSON_Delete(jsoncfg);
out_free:
	free(buffer);
out_close:
	fclose(fp);
out:
	return ret;
}

int service_apply_conf(struct service_conf *conf)
{
	struct pf_conf *pf;
	struct vf_conf *vf;
	int pf_idx;
	int vf_idx;

	if (!conf->valid)
		return -EINVAL;

	RTE_LOG(INFO, VDPA, "Apply initial devices configuration.\n");
	for (pf_idx = 0; pf_idx < conf->pf_list.len; pf_idx++) {
		pf = &conf->pf_list.pfs[pf_idx];
		RTE_LOG(INFO, VDPA, "Add PF: %s.\n", pf->slot);
		rte_vdpa_pf_dev_add(pf->slot);
        RTE_LOG(INFO, VDPA, "Waiting %d ms before add VF...\n", pf->wait_in_ms);
		usleep(pf->wait_in_ms * 1000);
		for (vf_idx = 0; vf_idx < pf->vf_list.len; vf_idx++) {
			vf = &pf->vf_list.vfs[vf_idx];
			RTE_LOG(INFO, VDPA,"Add VF: %s, socket:%s, uuid %s.\n", vf->slot,
					vf->vhost_socket, vf->uuid);
			if (!rte_vdpa_vf_dev_add(vf->slot, vf->uuid, NULL))
				vdpa_with_socket_path_start(vf->slot, vf->vhost_socket);
		}
	}
	return 0;
}
