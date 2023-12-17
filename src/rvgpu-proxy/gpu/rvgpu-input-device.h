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

#ifndef RVGPU_INPUT_DEVICE_H
#define RVGPU_INPUT_DEVICE_H

struct input_device;

/**
 * @brief Initialize input device
 * @param b - pointer to rvgpu backend
 * @return pointer to initialized input device structure
 */
struct input_device *input_device_init(struct rvgpu_backend *b);

/**
 * @brief Free input device resources
 * @param g - pointer to initialized input device structure
 */
void input_device_free(struct input_device *g);

#endif /* RVGPU_INPUT_DEVICE_H */
