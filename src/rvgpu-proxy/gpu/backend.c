#include <dlfcn.h>
#include <stddef.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "rvgpu.h"
#include "backend.h"
#include "error.h"

struct rvgpu_backend *init_backend_rvgpu(struct host_conn *servers)
{
	struct rvgpu_scanout_arguments scanout_args[MAX_HOSTS] = { 0 };
	struct rvgpu_backend *b;
	unsigned int i, j;
	int err;

	b = calloc(1, sizeof(*b));
	if (b == NULL) {
		error("calloc()");
		return NULL;
	}

	struct rvgpu_ctx *ctx = &b->ctx;

	struct rvgpu_ctx_arguments ctx_args = {
		.conn_tmt_s = servers->conn_tmt_s,
		.reconn_intv_ms = servers->reconn_intv_ms,
		.scanout_num = servers->host_cnt,
	};

	err = rvgpu_ctx_init(ctx, ctx_args);
	if (err < 0) {
		error("rvgpu_init_ctx()");
		goto err_free;
	}

	for (i = 0; i < servers->host_cnt; i++) {
		scanout_args[i].tcp.ip = strdup(servers->hosts[i].hostname);
		scanout_args[i].tcp.port = strdup(servers->hosts[i].portnum);
	}

	for (i = 0; i < ctx->scanout_num; i++) {
		struct rvgpu_scanout *be = &b->scanout[i];

		err = rvgpu_init(ctx, be, scanout_args[i]);
		if (err) {
			error("rvgpu_init(%u)", i);
			goto err_rvgpu_destroy;
		}

	}

	return b;

err_rvgpu_destroy:
	for (j = 0; j < i; j++) {
		struct rvgpu_scanout *s = &b->scanout[j];
		rvgpu_destroy(ctx, s);
	}
err_free:
	free(b);
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
