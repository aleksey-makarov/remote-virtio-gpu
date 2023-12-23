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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <librvgpu/rvgpu-protocol.h>

#include <rvgpu-generic/rvgpu-sanity.h>
#include <rvgpu-generic/rvgpu-utils.h>

#include "rvgpu-proxy.h"
#include "gpu/backend.h"
#include "gpu/rvgpu-gpu-device.h"
#include "gpu/rvgpu-input-device.h"
#include "gpu/error.h"

#define DEFAULT_WIDTH 800u
#define DEFAULT_HEIGHT 600u

#define RVGPU_DEFAULT_HOSTNAME "127.0.0.1"
#define RVGPU_DEFAULT_PORT "55667"

#define VMEM_DEFAULT_MB 0

#define RVGPU_DEFAULT_CONN_TMT_S 100u
#define RVGPU_RECONN_INVL_MS 500u

#define CARD_INDEX_MIN 0
#define CARD_INDEX_MAX 64

#define VMEM_MIN_MB 0
#define VMEM_MAX_MB 4096u

#define FRAMERATE_MIN 1u
#define FRAMERATE_MAX 120u

#define RVGPU_MIN_CONN_TMT_S 1u
#define RVGPU_MAX_CONN_TMT_S 100u

static void usage(void)
{
	fprintf(stderr,
		"Usage: rvgpu-proxy [options]\n"
		"	-c capset	specify capset file (default: %s)\n"
		"	-s scanout	specify scanout in form WxH@X,Y (default: %ux%u@0,0), max %u scanouts\n"
		"			X >= 0, Y >= 0, W > 0, H > 0\n"
#ifdef VSYNC_ENABLE
		"	-f rate		specify virtual framerate (default: disabled), should be [%u..%u]\n"
#endif
		"	-i index	specify index 'n' for device /dev/dri/card<n>, should be [%u..%u]\n"
		"	-n server:port  renderer for connecting (default: %s:%s), max %d hosts\n"
		"	-h		show this message\n"
		"	-M lim		memory limit in MB, should be [%u..%u]\n"
		"	-R t		connection timeout, should be [%u..%u]\n"
		"\n"
		"compiled with linux headers version %d.%d.%d (%d)\n"
		,
		CAPSET_PATH,
		DEFAULT_WIDTH, DEFAULT_HEIGHT, VIRTIO_GPU_MAX_SCANOUTS,
#ifdef VSYNC_ENABLE
		FRAMERATE_MIN, FRAMERATE_MAX,
#endif
		CARD_INDEX_MIN, CARD_INDEX_MAX - 1,
		RVGPU_DEFAULT_HOSTNAME, RVGPU_DEFAULT_PORT, MAX_HOSTS,
		VMEM_MIN_MB, VMEM_MAX_MB,
		RVGPU_MIN_CONN_TMT_S, RVGPU_MAX_CONN_TMT_S,
		(LINUX_VERSION_CODE >> 16) & 0xff,
		(LINUX_VERSION_CODE >>  8) & 0xff,
		(LINUX_VERSION_CODE >>  0) & 0xff,
		LINUX_VERSION_CODE);
}

int main(int argc, char **argv)
{
	struct input_device *inpdev;
	struct rvgpu_backend *b;
	int w, h, x , y;

	static struct gpu_device_params params = {
		.capset_path = NULL,
		.framerate = 0u,
		.mem_limit = VMEM_DEFAULT_MB,
		.card_index = -1,
		.num_scanouts = 0u,
		.dpys = { { .r = { .x = 0,
				   .y = 0,
				   .width = DEFAULT_WIDTH,
				   .height = DEFAULT_HEIGHT },
			    .flags = 1,
			    .enabled = 1 } },
	};

	struct rvgpu_ctx_arguments ctx_args = {
		.conn_tmt_s = RVGPU_DEFAULT_CONN_TMT_S,
		.reconn_intv_ms = RVGPU_RECONN_INVL_MS,
		.scanout_num = 0,
	};

	struct rvgpu_scanout_arguments sc_args[MAX_HOSTS];

	char path[64];
	int res, opt;
	char *ip, *port, *errstr = NULL;

	while ((opt = getopt(argc, argv, "hi:n:M:c:R:f:s:")) != -1) {
		switch (opt) {
		case 'c':
			params.capset_path = strdup(optarg);
			if (!params.capset_path) {
				error("strdup()");
				goto err;
			}
			break;
		case 'i':
			params.card_index =
				(int)sanity_strtonum(optarg, CARD_INDEX_MIN,
						     CARD_INDEX_MAX - 1,
						     &errstr);
			if (errstr != NULL) {
				error("card index: \'%s\': %s", optarg, errstr);
				goto err;
			}

			snprintf(path, sizeof(path), "/dev/dri/card%d", params.card_index);
			res = access(path, F_OK);
			if (res == 0) {
				error("device %s exists", path);
				goto err;
			} else if (errno != ENOENT) {
				error_errno("error while checking device %s", path);
				goto err;
			}
			break;
		case 'M':
			params.mem_limit = (unsigned int)sanity_strtonum(
				optarg, VMEM_MIN_MB, VMEM_MAX_MB, &errstr);
			if (errstr != NULL) {
				error("memory limit: \'%s\': %s", optarg, errstr);
				goto err;
			}
			break;
		case 'f':
#ifdef VSYNC_ENABLE
			params.framerate = (unsigned long)sanity_strtonum(
				optarg, FRAMERATE_MIN, FRAMERATE_MAX, &errstr);
			if (errstr != NULL) {
				error("frame rate: \'%s\': %s", optarg, errstr);
				goto err;
			}
			break;
#else
			error("VSYNC is not supported");
			goto err;
#endif
		case 's':
			if (params.num_scanouts >= VIRTIO_GPU_MAX_SCANOUTS) {
				error("too many scanouts");
				goto err;
			}
			res = sscanf(optarg, "%dx%d@%d,%d", &w, &h, &x, &y);
			if (res != 4u || !(w > 0 && h > 0 && x >= 0 && y >= 0)) {
					error( "invalid scanout configuration \'%s\'", optarg);
					goto err;
			} else {
					params.dpys[params.num_scanouts].r.width = (uint32_t)w;
					params.dpys[params.num_scanouts].r.height = (uint32_t)h;
					params.dpys[params.num_scanouts].r.x = (uint32_t)x;
					params.dpys[params.num_scanouts].r.y = (uint32_t)y;				
					params.dpys[params.num_scanouts].enabled = 1;
					params.dpys[params.num_scanouts].flags = 1;
					params.num_scanouts++;
			}
			break;
		case 'n':
			ip = strtok(optarg, ":");
			if (ip == NULL) {
				error("invalid server:port \'%s\'", optarg);
				goto err;
			}
			port = strtok(NULL, "");
			if (port == NULL)
				port = RVGPU_DEFAULT_PORT;

			if (ctx_args.scanout_num >= MAX_HOSTS) {
				error("too many hosts");
				goto err;
			}

			sc_args[ctx_args.scanout_num].ip = ip;
			sc_args[ctx_args.scanout_num].port = port;
			ctx_args.scanout_num++;
			break;
		case 'R':
			ctx_args.conn_tmt_s = (unsigned int)sanity_strtonum(
				optarg, RVGPU_MIN_CONN_TMT_S,
				RVGPU_MAX_CONN_TMT_S, &errstr);
			if (errstr != NULL) {
				error("invalid conn timeout \'%s\': %s", optarg, errstr);
				goto err;
			}
			break;
		case 'h':
			usage();
			goto ok;
		default:
			usage();
			goto err;
		}
	}

	if (ctx_args.scanout_num == 0) {
		sc_args[0].ip = RVGPU_DEFAULT_HOSTNAME;
		sc_args[0].port = RVGPU_DEFAULT_PORT;
		ctx_args.scanout_num = 1;
	}

	if (params.num_scanouts == 0)
		params.num_scanouts = 1;

	trace("num_scanouts: %u", params.num_scanouts);
	for (unsigned int s = 0; s < params.num_scanouts; s++) {
		trace("- %ux%u@%u,%u",
		      params.dpys[s].r.width,
		      params.dpys[s].r.height,
		      params.dpys[s].r.x,
		      params.dpys[s].r.y);
	}
	trace("num renderers: %u", ctx_args.scanout_num);
	for (unsigned int r = 0; r < ctx_args.scanout_num; r++) {
		trace("- %s:%s", sc_args[r].ip, sc_args[r].port);
	}

	/* change oom_score_adj to be very less likely killed */
#define OOM_SCORE_ADJ "/proc/self/oom_score_adj"
	{
		FILE *oom_file = fopen(OOM_SCORE_ADJ, "w");
		if (oom_file == NULL) {
			error_errno("fopen(%s)", OOM_SCORE_ADJ);
			goto err;
		}
		fprintf(oom_file, "%d", -1000);
		fclose(oom_file);
	}

	b = init_backend_rvgpu(&ctx_args, sc_args);
	if (!b) {
		error("init_backend_rvgpu()");
		goto err;
	}

	inpdev = input_device_init(b);
	if (!inpdev) {
		error("input_device_init()");
		goto err_destroy_backend;
	}

	if (gpu_device_main(&params, b) < 0) {
		error("gpu_device_main()");
		goto err_input_device_free;
	}

	input_device_free(inpdev);
	destroy_backend_rvgpu(b);
ok:
	exit(EXIT_SUCCESS);

err_input_device_free:
	input_device_free(inpdev);
err_destroy_backend:
	destroy_backend_rvgpu(b);
err:
	exit(EXIT_FAILURE);
}
