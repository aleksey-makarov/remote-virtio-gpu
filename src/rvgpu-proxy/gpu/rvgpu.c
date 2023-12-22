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

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <unistd.h>

#include "rvgpu.h"
#include "rvgpu_private.h"
#include "error.h"

struct conn_pipes {
	int rcv_pipe[2];
	int snd_pipe[2];
};

struct sc_priv {
	struct conn_pipes pipes[SOCKET_NUM];
	struct rvgpu_scanout_arguments args;
	bool activated;
};

static void free_communic_pipes(struct rvgpu_scanout *scanout)
{
	struct sc_priv *sc_priv = scanout->priv;

	for (unsigned int i = 0; i < SOCKET_NUM; i++) {
		close(sc_priv->pipes[i].rcv_pipe[PIPE_READ]);
		close(sc_priv->pipes[i].rcv_pipe[PIPE_WRITE]);
		close(sc_priv->pipes[i].snd_pipe[PIPE_READ]);
		close(sc_priv->pipes[i].snd_pipe[PIPE_WRITE]);
	}
}

static int init_communic_pipes(struct rvgpu_scanout *scanout)
{
	struct sc_priv *sc_priv = scanout->priv;

	for (int i = 0; i < SOCKET_NUM; i++) {
		if (pipe2(sc_priv->pipes[i].rcv_pipe, 0) == -1) {
			perror("pipe creation error");
			return -1;
		}
		fcntl(sc_priv->pipes[i].rcv_pipe[0], F_SETPIPE_SZ, PIPE_SIZE);
		if (pipe2(sc_priv->pipes[i].snd_pipe, 0) == -1) {
			perror("pipe creation error");
			return -1;
		}
		fcntl(sc_priv->pipes[i].snd_pipe[0], F_SETPIPE_SZ, PIPE_SIZE);
	}
	return 0;
}

static int init_tcp_scanout(struct rvgpu_ctx *ctx, struct rvgpu_scanout *scanout,
		     struct rvgpu_scanout_arguments *args)
{
	struct ctx_priv *ctx_priv = ctx->priv;
	struct sc_priv *sc_priv = scanout->priv;

	struct vgpu_host *cmd = &ctx_priv->cmd[ctx_priv->cmd_count];
	struct vgpu_host *res = &ctx_priv->res[ctx_priv->res_count];

	cmd->tcp = args;
	cmd->host_p[PIPE_WRITE] = sc_priv->pipes[COMMAND].rcv_pipe[PIPE_WRITE];
	cmd->host_p[PIPE_READ] = sc_priv->pipes[COMMAND].snd_pipe[PIPE_READ];
	cmd->vpgu_p[PIPE_WRITE] = sc_priv->pipes[COMMAND].snd_pipe[PIPE_WRITE];
	cmd->vpgu_p[PIPE_READ] = sc_priv->pipes[COMMAND].rcv_pipe[PIPE_READ];
	ctx_priv->cmd_count++;

	res->tcp = args;
	res->host_p[PIPE_WRITE] = sc_priv->pipes[RESOURCE].rcv_pipe[PIPE_WRITE];
	res->host_p[PIPE_READ] = sc_priv->pipes[RESOURCE].snd_pipe[PIPE_READ];
	res->vpgu_p[PIPE_WRITE] = sc_priv->pipes[RESOURCE].snd_pipe[PIPE_WRITE];
	res->vpgu_p[PIPE_READ] = sc_priv->pipes[RESOURCE].rcv_pipe[PIPE_READ];
	ctx_priv->res_count++;

	return 0;
}

int rvgpu_ctx_send(struct rvgpu_ctx *ctx, const void *buf, size_t len)
{
	struct ctx_priv *ctx_priv = ctx->priv;

	for (unsigned int i = 0; i < ctx_priv->cmd_count; i++) {
		struct sc_priv *sc_priv =
			(struct sc_priv *)ctx_priv->sc[i]->priv;
		size_t offset = 0;

		if (!sc_priv->activated)
			return -EBUSY;

		while (offset < len) {
			ssize_t written = write(
				sc_priv->pipes[COMMAND].snd_pipe[PIPE_WRITE],
				(const char *)buf + offset, len - offset);
			if (written >= 0) {
				offset += (size_t)written;
			} else if (errno != EAGAIN) {
				warn("Error while writing to socket");
				return errno;
			}
		}
	}

	return 0;
}

int rvgpu_recv_all(struct rvgpu_scanout *scanout, enum pipe_type p, void *buf,
		   size_t len)
{
	struct sc_priv *sc_priv = scanout->priv;
	size_t offset = 0;

	if (!sc_priv->activated)
		return -EBUSY;

	while (offset < len) {
		ssize_t r = read(sc_priv->pipes[p].rcv_pipe[PIPE_READ],
				 (char *)buf + offset, len - offset);
		if (r > 0) {
			offset += (size_t)r;
		} else if (r == 0) {
			warnx("Connection was closed");
			return -1;
		} else if (errno != EAGAIN) {
			warn("Error while reading from socket");
			return -1;
		}
	}

	return offset;
}

int rvgpu_recv(struct rvgpu_scanout *scanout, enum pipe_type p, void *buf,
	       size_t len)
{
	struct sc_priv *sc_priv = scanout->priv;

	if (!sc_priv->activated)
		return -EBUSY;

	return read(sc_priv->pipes[p].rcv_pipe[PIPE_READ], buf, len);
}

int rvgpu_send(struct rvgpu_scanout *scanout, enum pipe_type p, const void *buf,
	       size_t len)
{
	struct sc_priv *sc_priv = scanout->priv;

	if (!sc_priv->activated)
		return -EBUSY;

	int rc = write(sc_priv->pipes[p].snd_pipe[PIPE_WRITE], buf, len);
	/*
	 * During reset gpu procedure pipe may be full and since it's in
	 * NONBLOCKING mode, write will return EAGAIN. Backend device
	 * should ignore this.
	 */
	return ((rc == -1) && (errno == EAGAIN)) ? (int)len : rc;
}

int rvgpu_init(struct rvgpu_ctx *ctx,
	       struct rvgpu_scanout *scanout,
	       struct rvgpu_scanout_arguments *args)
{
	assert(ctx);
	assert(scanout);
	assert(args);

	struct ctx_priv *ctx_priv = ctx->priv;
	struct sc_priv *sc_priv;
	int rc;

	sc_priv = (struct sc_priv *)calloc(1, sizeof(*sc_priv));
	if (!sc_priv) {
		error("malloc(struct sc_priv)");
		return -1;
	}

	sc_priv->args.ip = strdup(args->ip);
	sc_priv->args.port = strdup(args->port);
	if (!sc_priv->args.ip || !sc_priv->args.port) {
		error("strdup()");
		goto err_free;
	}

	scanout->priv = sc_priv;
	ctx_priv->sc[ctx_priv->inited_scanout_num] = scanout;

	pthread_mutex_lock(&ctx_priv->lock);

	rc = init_communic_pipes(scanout);
	if (rc) {
		pthread_mutex_unlock(&ctx_priv->lock);
		error("init_communic_pipes()");
		goto err_free;
	}

	rc = init_tcp_scanout(ctx, scanout, &sc_priv->args);
	if (rc) {
		pthread_mutex_unlock(&ctx_priv->lock);
		perror("Failed to init TCP scanout");
		goto err;
	}

	sc_priv->activated = true;
	ctx_priv->inited_scanout_num++;

	pthread_mutex_unlock(&ctx_priv->lock);

	return 0;

err:
	free_communic_pipes(scanout);

err_free:
	free(sc_priv->args.ip);
	free(sc_priv->args.port);
	free(sc_priv);
	return -1;
}

void rvgpu_destroy(struct rvgpu_ctx *ctx, struct rvgpu_scanout *scanout)
{
	assert(ctx);
	assert(scanout);

	(void) ctx;
	struct sc_priv *sc_priv = scanout->priv;

	free_communic_pipes(scanout);

	free(sc_priv->args.ip);
	free(sc_priv->args.port);
	free(sc_priv);

	scanout->priv = NULL;
}

void rvgpu_ctx_set_reset_state_initiated(struct rvgpu_ctx *ctx)
{
	struct ctx_priv *ctx_priv = ctx->priv;

	ctx_priv->reset.state = GPU_RESET_INITIATED;
}

enum reset_state rvgpu_ctx_get_reset_state(struct rvgpu_ctx *ctx)
{
	struct ctx_priv *ctx_priv = ctx->priv;

	return ctx_priv->reset.state;
}

int rvgpu_ctx_poll(struct rvgpu_ctx *ctx, enum pipe_type p, int timeo,
		   short int *events, short int *revents)
{
	struct ctx_priv *ctx_priv = ctx->priv;
	struct pollfd pfd[MAX_HOSTS];
	int ret = 0;

	if (p == COMMAND) {
		for (unsigned int i = 0; i < ctx_priv->cmd_count; i++) {
			struct vgpu_host *cmd = &ctx_priv->cmd[i];

			if (events[i] & POLLIN) {
				pfd[i].fd = cmd->vpgu_p[PIPE_READ];
				pfd[i].events = POLLIN;
			} else if (events[i] & POLLOUT) {
				pfd[i].fd = cmd->vpgu_p[PIPE_WRITE];
				pfd[i].events = POLLOUT;
			}
		}
		ret = poll(pfd, ctx_priv->cmd_count, timeo);
		for (unsigned int i = 0; i < ctx_priv->cmd_count; i++)
			revents[i] = pfd[i].revents;

		return ret;
	}

	if (p == RESOURCE) {
		for (unsigned int i = 0; i < ctx_priv->res_count; i++) {
			struct vgpu_host *res = &ctx_priv->res[i];

			if (events[i] & POLLIN) {
				pfd[i].fd = res->vpgu_p[PIPE_READ];
				pfd[i].events = POLLIN;
			} else if (events[i] & POLLOUT) {
				pfd[i].fd = res->vpgu_p[PIPE_WRITE];
				pfd[i].events = POLLOUT;
			}
		}
		ret = poll(pfd, ctx_priv->res_count, timeo);
		for (unsigned int i = 0; i < ctx_priv->res_count; i++)
			revents[i] = pfd[i].revents;

		return ret;
	}

	return ret;
}

void *thread_conn_tcp(void *arg);

int rvgpu_ctx_init(struct rvgpu_ctx *ctx,
		   struct rvgpu_ctx_arguments *args)
{
	assert(ctx);
	assert(args);

	struct ctx_priv *ctx_priv = calloc(1, sizeof(*ctx_priv));
	if (!ctx_priv) {
		error("calloc()");
		return -1;
	}

	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	if (pthread_mutex_init(&ctx_priv->lock, &attr)) {
		error_errno("pthread_mutex_init()");
		goto err_free;
	}
	if (pthread_cond_init(&ctx_priv->reset.cond, NULL)) {
		error_errno("pthread_cond_init()");
		goto err_free;
	}
	if (pthread_mutex_init(&ctx_priv->reset.lock, NULL)) {
		error_errno("pthread_mutex_init()");
		goto err_free;
	}

	ctx->scanout_num = args->scanout_num;
	ctx_priv->scanout_num = args->scanout_num;
	memcpy(&ctx_priv->args, args, sizeof(*args));

	ctx->priv = ctx_priv;
	if (pthread_create(&ctx_priv->tid, NULL, thread_conn_tcp, ctx)) {
		error_errno("pthread_create(thread_conn_tcp)");
		goto err_free;
	}

	return 0;

err_free:
	free(ctx_priv);
	return -1;
}

void rvgpu_ctx_destroy(struct rvgpu_ctx *ctx)
{
	struct ctx_priv *ctx_priv = (struct ctx_priv *)ctx->priv;

	ctx_priv->interrupted = true;
}
