#include <dlfcn.h>
#include <stddef.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "backend.h"

#define VERSION_SYMBOL_NAME "rvgpu_backend_version"

#define GPU_BE_FIND_PLUGIN_VERSION(ver)                                        \
	do {                                                                   \
		uint32_t *ver_ptr =                                            \
			(uint32_t *)dlsym(plugin, VERSION_SYMBOL_NAME);        \
		if (ver_ptr == NULL)                                           \
			(ver) = 1u;                                            \
		else                                                           \
			(ver) = *ver_ptr;                                      \
	} while (0)
#define GPU_BE_OPS_FIELD(field) ((be)->ops.field)
#define GPU_BE_FIND_SYMBOL_OR_FAIL(symbol)                             \
	do {                                                           \
		GPU_BE_OPS_FIELD(symbol) =                             \
			(typeof(GPU_BE_OPS_FIELD(symbol)))(            \
				uintptr_t)dlsym(plugin, #symbol);      \
		if (GPU_BE_OPS_FIELD(symbol) == NULL) {                \
			warnx("failed to find plugin symbol '%s': %s", \
			      #symbol, dlerror());                     \
			goto err_sym;                                  \
		}                                                      \
	} while (0)


#define STRINGIFY(s) STRINGIFY_(s)
#define STRINGIFY_(s) #s

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
	struct rvgpu_backend *be = b;
	void *plugin = b->lib_handle;
	uint32_t version;

	GPU_BE_FIND_PLUGIN_VERSION(version);

	switch (version) {
	case RVGPU_BACKEND_V1:
		GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_ctx_init);
		GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_ctx_destroy);
		GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_frontend_reset_state);
		GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_ctx_wait);
		GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_ctx_wakeup);
		GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_ctx_poll);
		GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_ctx_send);
		GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_ctx_transfer_to_host);
		GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_ctx_res_create);
		GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_ctx_res_find);
		GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_ctx_res_destroy);
		break;
	default:
		err(1, "unsupported backend version: %u", version);
	}

	be->plugin_version = version;
	be->ops.rvgpu_ctx_init(ctx, ctx_args, &backend_reset_state);

	return 0;
err_sym:
	return -1;
}

static int rvgpu_init_backends(struct rvgpu_backend *b,
			       struct rvgpu_scanout_arguments *scanout_args)
{
	struct rvgpu_ctx *ctx = &b->ctx;
	void *plugin = b->lib_handle;
	uint32_t version = b->plugin_version;

	for (unsigned int i = 0; i < ctx->scanout_num; i++) {
		struct rvgpu_scanout *be = &b->scanout[i];

		switch (version) {
		case RVGPU_BACKEND_V1:
			GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_init);
			GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_destroy);
			GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_send);
			GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_recv);
			GPU_BE_FIND_SYMBOL_OR_FAIL(rvgpu_recv_all);
			break;
		default:
			err(1, "unsupported backend version: %u\n", version);
			return -1;
		}

		be->ops.rvgpu_init(ctx, be, scanout_args[i]);
	}

	return 0;
err_sym:
	return -1;
}

struct rvgpu_backend *init_backend_rvgpu(struct host_conn *servers)
{
	struct rvgpu_scanout_arguments scanout_args[MAX_HOSTS] = { 0 };
	struct rvgpu_backend *rvgpu_be;

	rvgpu_be = calloc(1, sizeof(*rvgpu_be));
	if (rvgpu_be == NULL) {
		warnx("failed to allocate backend: %s", strerror(errno));
		goto error;
	}

	char str_lib[] = "librvgpu.so." STRINGIFY(LIBRVGPU_SOVERSION);

	/* Flush the current dl error state */
	dlerror();

	rvgpu_be->lib_handle = dlopen(str_lib, RTLD_NOW);
	if (rvgpu_be->lib_handle == NULL) {
		warnx("failed to open backend library '%s': %s", str_lib,
		      dlerror());
		goto error_free;
	}

	struct rvgpu_ctx_arguments ctx_args = {
		.conn_tmt_s = servers->conn_tmt_s,
		.reconn_intv_ms = servers->reconn_intv_ms,
		.scanout_num = servers->host_cnt,
	};

	if (rvgpu_init_ctx(rvgpu_be, ctx_args)) {
		warnx("failed to init rvgpu ctx");
		goto error_dlclose;
	}

	for (unsigned int i = 0; i < servers->host_cnt; i++) {
		scanout_args[i].tcp.ip = strdup(servers->hosts[i].hostname);
		scanout_args[i].tcp.port = strdup(servers->hosts[i].portnum);
	}

	if (rvgpu_init_backends(rvgpu_be, scanout_args)) {
		warnx("failed to init rvgpu backends");
		goto error_dlclose;
	}

	return rvgpu_be;

error_dlclose:
	dlclose(rvgpu_be->lib_handle);
error_free:
	free(rvgpu_be);
error:
	return NULL;
}

void destroy_backend_rvgpu(struct rvgpu_backend *b)
{
	struct rvgpu_ctx *ctx = &b->ctx;

	for (unsigned int i = 0; i < ctx->scanout_num; i++) {
		struct rvgpu_scanout *s = &b->scanout[i];

		s->ops.rvgpu_destroy(ctx, s);
	}
	b->ops.rvgpu_ctx_destroy(ctx);
	dlclose(b->lib_handle);
}
