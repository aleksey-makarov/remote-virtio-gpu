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

#ifndef RVGPU_PROXY_H
#define RVGPU_PROXY_H

#include <linux/version.h>

#define CAPSET_PATH "/etc/virgl.capset"

/*
 * The commit 34268c9dde4 from linux kernel changes virtio_gpu_ctrl_hdr
 * that is used by UHMI to implement vsync.
 * Check if we are compiled with old enough kernel.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,15,0)
# define VSYNC_ENABLE
#endif

#endif
