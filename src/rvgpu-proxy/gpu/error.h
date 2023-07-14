// SPDX-License-Identifier: Apache-2.0
/**
 * Copyright (c) 2023  Panasonic Automotive Systems, Co., Ltd.
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

#ifndef __error_h__
#define __error_h__

#include <stdio.h>

#ifndef SRC_FILE
# define SRC_FILE __FILE__
#endif

#define error(_format, ...) ({                            \
	fprintf(stdout, "*** %s:%d %s() : " _format "\n", \
		SRC_FILE, __LINE__, __func__, ##__VA_ARGS__); })

#define error_errno(_format, ...) ({                         \
	fprintf(stdout, "*** %s:%d %s() %m : " _format "\n", \
		SRC_FILE, __LINE__, __func__, ##__VA_ARGS__); })

#endif /* TRACE_H */
