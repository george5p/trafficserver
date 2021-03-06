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


/*
TODO tests of stoping an event at the session and transaction level


#include "ts.h"
#include <sys/types.h>
#include <stdio.h>


#if 0
*/
/**************************************************************************
 * HTTP Sessions
 *************************************************************************/

/*
1. inkapi void INKHttpHookAdd(INKHttpHookID id, INKCont contp);
   Covered in INKHttpHookAdd.c
2. inkapi void INKHttpSsnHookAdd(INKHttpSsn ssnp, INKHttpHookID id, INKCont contp);
   Called for all events except INK_HTTP_SESSION_START, INK_EVENT_MGMT_UPDATE.
3. inkapi void INKHttpSsnReenable(INKHttpSsn ssnp, INKEvent event);
   INK_EVENT_HTTP_CONTINUE
TODO build a test case for event INK_EVENT_HTTP_ERROR HTTP Transactions
4. void INKHttpTxnReenable(INKHttpTxn txnp, INKEvent INK_EVENT_HTTP_ERROR)
   INK_EVENT_HTTP_CONTINUE
TODO build a test case for event INK_EVENT_HTTP_ERROR
*/

/**************************************************************************
 * HTTP sessions
 *************************************************************************/
