/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024, NVIDIA CORPORATION & AFFILIATES.
 */

#ifndef __VDPA_CONF_H_
#define __VDPA_CONF_H_


struct vf_conf {
	char *slot;
	char *vhost_socket;
	char *uuid;
};

struct vf_list {
	int len;
	struct vf_conf *vfs;
};

struct pf_conf {
	char *slot;
	struct vf_list vf_list;
	int wait_in_ms;
};

struct pf_list {
	int len;
	struct pf_conf *pfs;
};

struct service_conf {
	struct pf_list pf_list;
	int wait_in_ms;
	bool valid;
};

int service_load_conf(struct service_conf *conf);
void service_free_conf(struct service_conf *conf);
int service_apply_conf(struct service_conf *conf);

#endif
