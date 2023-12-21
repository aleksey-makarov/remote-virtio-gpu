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

#include <librvgpu/rvgpu.h>

struct host_server {
	char *hostname;
	char *portnum;
};

struct host_conn {
	struct host_server hosts[MAX_HOSTS];
	unsigned int host_cnt;
	unsigned int conn_tmt_s;
	unsigned int reconn_intv_ms;
	bool active;
};

/**
 * @brief Initialize rvgpu backend
 *
 * @param servers - pointer to remote targets settings
 *
 * @return pointer to rvgpu backend
 */
struct rvgpu_backend *init_backend_rvgpu(struct host_conn *servers);

/**
 * @brief Destroy rvgpu backend
 *
 * @param b - pointer to rvgpu backend
 *
 * @return void
 */
void destroy_backend_rvgpu(struct rvgpu_backend *b);

enum reset_state backend_get_reset_state(void);
void backend_set_reset_state_initiated(void);

#endif /* RVGPU_PROXY_H */
