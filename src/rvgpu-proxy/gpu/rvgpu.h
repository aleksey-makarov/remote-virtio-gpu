// SPDX-License-Identifier: Apache-2.0
/**
 * Copyright (c) 2022  Panasonic Automotive Systems, Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RVGPU_H
#define RVGPU_H

#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/queue.h>

#define SOCKET_NUM 2
#define TIMERS_CNT 2

#define PIPE_SIZE (8 * 1024 * 1024)
#define PIPE_READ (0)
#define PIPE_WRITE (1)

/* Maximum number of remote rendering targets */
#define MAX_HOSTS 16

/*
 * rvgpu establishes two connections to remote rendering backend.
 * One is used for generic virtio command processing and the another one is
 * used for resource transferring, if resource caching feature is enabled.
 */
enum pipe_type {
	COMMAND,
	RESOURCE,
};

/* Reset states of the GPU Resync feature */
enum reset_state {
	GPU_RESET_NONE,
	GPU_RESET_TRUE,
	GPU_RESET_INITIATED,
};

struct rvgpu_scanout_arguments {
	char *ip;
	char *port;
};

struct rvgpu_ctx_arguments {
	/* Timeout in seconds to wait for all scanouts be connected */
	uint16_t conn_tmt_s;
	/* Scanout reconnection interval */
	uint16_t reconn_intv_ms;
	/* Number of scanouts */
	uint16_t scanout_num;
};

struct rvgpu_scanout;

struct ctx_priv;
struct rvgpu_ctx {
	uint16_t scanout_num;
	struct ctx_priv *priv;
};

/*
 * RVGPU resource information
 */
struct rvgpu_res_info {
	uint32_t target;
	uint32_t format;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t array_size;
	uint32_t last_level;
	uint32_t flags;
	uint32_t bpp;
};

/*
 * RVGPU resource
 */
struct rvgpu_res {
	unsigned int resid;
	struct iovec *backing;
	unsigned int nbacking;
	struct rvgpu_res_info info;

	LIST_ENTRY(rvgpu_res) entry;
};

/*
 * Unified structure to pass the 2d/3d transfer to host info
 */
struct rvgpu_res_transfer {
	uint32_t x, y, z;
	uint32_t w, h, d;
	uint32_t level;
	uint32_t stride;
	uint64_t offset;
};

struct sc_priv;
struct rvgpu_scanout {
	uint32_t scanout_id;
	struct sc_priv *priv;
};

struct rvgpu_backend {
	struct rvgpu_ctx ctx;
	struct rvgpu_scanout scanout[MAX_HOSTS];
};

/** @brief Init a remote virtio gpu context
 *
 *  @param ctx pointer to the rvgpu context
 *  @param args arguments needed to initialize a remote virtio gpu context
 *  @param gpu_reset_cb callback function to process the GPU reset state
 *
 *  @return 0 on success
 *         -1 on error
 */
int rvgpu_ctx_init(struct rvgpu_ctx *ctx,
		   struct rvgpu_ctx_arguments *args);

/** @brief Destroy a remote virtio gpu context
 *
 *  @param ctx pointer to the rvgpu context
 *
 *  @return void
 */
void rvgpu_ctx_destroy(struct rvgpu_ctx *ctx);

/** @brief Lock ctx and wait for appropriate GPU reset state
 *
 *  @param ctx pointer to the rvgpu context
 *  @param state GPU reset state
 *
 *  @return void
 */
void rvgpu_ctx_wait(struct rvgpu_ctx *ctx,
		    enum reset_state state);

/** @brief Wakeup a ctx from rvgpu_ctx_wait function
 *
 *  @param ctx pointer to the rvgpu context
 *
 *  @return void
 */
void rvgpu_ctx_wakeup(struct rvgpu_ctx *ctx);

/** @brief Poll for ctx events
 *
 *  @param ctx pointer to the rvgpu context
 *  @param p type of pipe. virtio or resource pipe
 *  @param timeo poll timeout
 *  @param events poll events
 *  @param revents poll revents
 *
 *  @return number of poll revents
 */
int rvgpu_ctx_poll(struct rvgpu_ctx *ctx,
		   enum pipe_type p,
		   int timeo,
		   short int *events,
		   short int *revents);

/** @brief Transfer the virtio stream to remote targets
 *
 *  @param ctx pointer to the rvgpu context
 *  @param buf pointer to virtio buffer
 *  @param len size of data
 *
 *  @return 0 on success
 *  @return errno on error
 */
int rvgpu_ctx_send(struct rvgpu_ctx *ctx,
                   const void *buf,
		   size_t len);

/** @brief transfer a remote virtio gpu resource to target
 *
 *  @param ctx pointer to the rvgpu context
 *  @param t resource transfer container
 *  @param res remote virtio gpu resource
 *
 *  @return 0 on success
 *  @return -1 on error
 */
int rvgpu_ctx_transfer_to_host(struct rvgpu_ctx *ctx,
			       const struct rvgpu_res_transfer *t,
			       const struct rvgpu_res *res);

/** @brief Get a remote virtio gpu resource
 *
 *  @param ctx pointer to the rvgpu context
 *  @param resource_id resource identificator
 *
 *  @return pointer to resource
 */
struct rvgpu_res *rvgpu_ctx_res_find(struct rvgpu_ctx *ctx,
				     uint32_t resource_id);

/** @brief Destroy a remote virtio gpu resource
 *
 *  @param ctx pointer to the rvgpu context
 *  @param resource_id resource identificator
 *
 *  @return void
 */
void rvgpu_ctx_res_destroy(struct rvgpu_ctx *ctx,
			   uint32_t resource_id);

/** @brief Create a remote virtio gpu resource
 *
 *  @param ctx pointer to the rvgpu context
 *  @param info resource info
 *  @param resource_id resource identificator
 *
 *  @return 0 on success
 *  @return -1 on error
 */
int rvgpu_ctx_res_create(struct rvgpu_ctx *ctx,
			 const struct rvgpu_res_info *info,
			 uint32_t resource_id);

void rvgpu_ctx_set_reset_state_initiated(struct rvgpu_ctx *ctx);
enum reset_state rvgpu_ctx_get_reset_state(struct rvgpu_ctx *ctx);

/** @brief Initialize a remote target
 *
 *  @param ctx pointer to the rvgpu context
 *  @param scanout pointer to remote target
 *  @param args arguments needed to initialize a remote target
 *
 *  @return 0 on success
 *  @return -1 on error
 */
int rvgpu_init(struct rvgpu_ctx *ctx,
	       struct rvgpu_scanout *scanout,
	       struct rvgpu_scanout_arguments *args);

/** @brief Destroy a remote target
 *
 *  @param ctx pointer to the rvgpu context
 *  @param scanout pointer to remote target
 *
 *  @return void
 */
void rvgpu_destroy(struct rvgpu_ctx *ctx,
		   struct rvgpu_scanout *scanout);

/** @brief Receive data of the specified length from a remote target
 *
 *  @param scanout pointer to remote target
 *  @param p type of data. virtio command or resource
 *  @param buf pointer to virtio buffer
 *  @param len size of the virtio buffer
 *
 *  @return size of received data
 *  @return errno on error
 */
int rvgpu_recv_all(struct rvgpu_scanout *scanout,
		   enum pipe_type p,
		   void *buf,
		   size_t len);

/** @brief Receive data from a remote target
 *
 *  @param scanout pointer to remote target
 *  @param p type of data. virtio command or resource
 *  @param buf pointer to virtio buffer
 *  @param len size of the virtio buffer
 *
 *  @return size of received data
 *  @return errno on error
 */
int rvgpu_recv(struct rvgpu_scanout *scanout,
	       enum pipe_type p,
	       void *buf,
	       size_t len);

/** @brief Send data to a remote target
 *
 *  @param scanout pointer to remote target
 *  @param p type of pipe. virtio or resource pipe
 *  @param buf pointer to virtio buffer
 *  @param size of data
 *
 *  @return size of sent data
 *  @return errno on error
 */
int rvgpu_send(struct rvgpu_scanout *scanout,
	       enum pipe_type p,
	       const void *buf,
	       size_t len);

#endif /* RVGPU_H */
