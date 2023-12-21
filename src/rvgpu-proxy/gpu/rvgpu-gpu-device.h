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

#ifndef RVGPU_GPU_DEVICE_H
#define RVGPU_GPU_DEVICE_H

#include <linux/virtio_gpu.h>

struct gpu_device_params {
	char *capset_path;
	bool split_resources;
	int card_index;
	unsigned int num_scanouts;
	unsigned int mem_limit;
	unsigned long framerate;
	struct virtio_gpu_display_one dpys[VIRTIO_GPU_MAX_SCANOUTS];
};

struct rvgpu_backend;
int gpu_device_main(struct gpu_device_params *param, struct rvgpu_backend *rvgpu_be);

#endif /* RVGPU_GPU_DEVICE_H */
