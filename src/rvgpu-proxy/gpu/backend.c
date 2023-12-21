#include <dlfcn.h>
#include <stddef.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <librvgpu/rvgpu.h>

#include "backend.h"

static enum reset_state gpu_reset_state;

static void backend_reset_state(struct rvgpu_ctx *ctx, enum reset_state state)
{
	(void)ctx;
	gpu_reset_state = state;
}

enum reset_state  backend_get_reset_state(void)
{
	return gpu_reset_state;
}

void backend_set_reset_state_initiated(void)
{
	gpu_reset_state = GPU_RESET_INITIATED;
}

static int rvgpu_init_ctx(struct rvgpu_backend *b, struct rvgpu_ctx_arguments ctx_args)
{
	struct rvgpu_ctx *ctx = &b->ctx;

	rvgpu_ctx_init(ctx, ctx_args, &backend_reset_state);

	return 0;
}

static int rvgpu_init_backends(struct rvgpu_backend *b,
			       struct rvgpu_scanout_arguments *scanout_args)
{
	struct rvgpu_ctx *ctx = &b->ctx;

	for (unsigned int i = 0; i < ctx->scanout_num; i++) {
		struct rvgpu_scanout *be = &b->scanout[i];

		rvgpu_init(ctx, be, scanout_args[i]);
	}

	return 0;
}

struct rvgpu_backend *init_backend_rvgpu(struct host_conn *servers)
{
	struct rvgpu_scanout_arguments scanout_args[MAX_HOSTS] = { 0 };
	struct rvgpu_backend *rvgpu_be;

	rvgpu_be = calloc(1, sizeof(*rvgpu_be));
	if (rvgpu_be == NULL) {
		warnx("failed to allocate backend: %s", strerror(errno));
		goto err;
	}

	struct rvgpu_ctx_arguments ctx_args = {
		.conn_tmt_s = servers->conn_tmt_s,
		.reconn_intv_ms = servers->reconn_intv_ms,
		.scanout_num = servers->host_cnt,
	};

	if (rvgpu_init_ctx(rvgpu_be, ctx_args)) {
		warnx("failed to init rvgpu ctx");
		goto err_free;
	}

	for (unsigned int i = 0; i < servers->host_cnt; i++) {
		scanout_args[i].tcp.ip = strdup(servers->hosts[i].hostname);
		scanout_args[i].tcp.port = strdup(servers->hosts[i].portnum);
	}

	if (rvgpu_init_backends(rvgpu_be, scanout_args)) {
		warnx("failed to init rvgpu backends");
		goto err_free;
	}

	return rvgpu_be;

err_free:
	free(rvgpu_be);
err:
	return NULL;
}

void destroy_backend_rvgpu(struct rvgpu_backend *b)
{
	struct rvgpu_ctx *ctx = &b->ctx;

	for (unsigned int i = 0; i < ctx->scanout_num; i++) {
		struct rvgpu_scanout *s = &b->scanout[i];

		rvgpu_destroy(ctx, s);
	}
	rvgpu_ctx_destroy(ctx);
}
