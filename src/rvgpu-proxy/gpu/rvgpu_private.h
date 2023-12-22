#ifndef __rvgpu_private_h__
#define __rvgpu_private_h__

#include <stdint.h>
#include <poll.h>
#include <pthread.h>

#include "rvgpu.h"

enum host_state {
	HOST_NONE,
	HOST_CONNECTED,
	HOST_DISCONNECTED,
	HOST_RECONNECTED,
};

struct vgpu_host {
	struct tcp_host *tcp;
	struct pollfd *pfd;
	int host_p[2];
	int vpgu_p[2];
	int sock;
	enum host_state state;
};

struct gpu_reset {
	enum reset_state state;
	pthread_mutex_t lock;
	pthread_cond_t cond;
};

struct ctx_priv {
	pthread_t tid;
	uint16_t inited_scanout_num;
	uint16_t scanout_num;
	bool interrupted;
	struct vgpu_host cmd[MAX_HOSTS];
	struct vgpu_host res[MAX_HOSTS];
	uint16_t cmd_count;
	uint16_t res_count;
	struct gpu_reset reset;
	pthread_mutex_t lock;
	struct rvgpu_scanout *sc[MAX_HOSTS];
	struct rvgpu_ctx_arguments args;
	LIST_HEAD(res_head, rvgpu_res) reslist;
};

#endif
