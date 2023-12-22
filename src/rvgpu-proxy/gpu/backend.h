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

#ifndef __backend_h__
#define __backend_h__

#include "rvgpu.h"

struct rvgpu_backend *init_backend_rvgpu(struct rvgpu_ctx_arguments *ctx_args,
					 struct rvgpu_scanout_arguments *hosts);

void destroy_backend_rvgpu(struct rvgpu_backend *b);

#endif /* RVGPU_PROXY_H */
