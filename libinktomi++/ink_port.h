/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

/****************************************************************************

  Ink_port.h

  Definitions & declarations to faciliate inter-architecture portability.

 ****************************************************************************/

#if !defined (_ink_port_h_)
#define	_ink_port_h_

#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>

typedef char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;

#ifndef INT64_MIN
#define INTU64_MAX (18446744073709551615ULL)
#define INT64_MAX (9223372036854775807LL)
#define INT64_MIN (-INT64_MAX -1LL)
#define INTU32_MAX (4294967295U)
#define INT32_MAX (2147483647)
#define INT32_MIN (-2147483647-1)
#endif

#define POSIX_THREAD
#define POSIX_THREAD_10031c

#ifndef ETIME
#ifdef ETIMEDOUT
#define ETIME ETIMEDOUT
#endif
#endif

#ifndef ENOTSUP
#ifdef EOPNOTSUPP
#define ENOTSUP EOPNOTSUPP
#endif
#endif

#if (HOST_OS == freebsd)
#define NO_MEMALIGN
#endif

#if (HOST_OS == darwin)
#define NO_MEMALIGN
#define RENTRENT_GETHOSTBYNAME
#define RENTRENT_GETHOSTBYADDR
#endif

#define NUL '\0'

#endif
