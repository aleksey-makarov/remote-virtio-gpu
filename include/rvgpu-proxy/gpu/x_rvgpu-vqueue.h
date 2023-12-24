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

#ifndef RVGPU_VQUEUE_H
#define RVGPU_VQUEUE_H

#include <linux/virtio_ring.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/uio.h>

/**
 * @brief Virtqueue structure (device part)
 */
struct vqueue {
	struct vring vr; /**< actual vring in guest memory */
	uint16_t last_avail_idx; /**< index of last read avail entry */
};

/**
 * @brief Virtqueue request (device part)
 */
#define VQUEUE_REQUEST_IOVEC_LEN 1024
struct vqueue_request {
	struct iovec r[VQUEUE_REQUEST_IOVEC_LEN]; /**< iovectors for reading */
	struct iovec w[VQUEUE_REQUEST_IOVEC_LEN]; /**< iovectors for writing */
	size_t nr; /**< number of read iovecs */
	size_t nw; /**< number of write iovecs */
	uint16_t idx; /**< index of first descriptior in the chain */
	struct vqueue *q;

	bool mapped;
};

/**
 * @brief Check if the queue has new requests
 * @retval yes it does
 */
static inline bool vqueue_are_requests_available(struct vqueue *q)
{
	return q->last_avail_idx != q->vr.avail->idx;
}

int vqueue_get_request(int vilo, struct vqueue *q, struct vqueue_request *req);
void vqueue_send_response(struct vqueue_request *req, size_t resp_len);

#endif /* RVGPU_VQUEUE_H */
