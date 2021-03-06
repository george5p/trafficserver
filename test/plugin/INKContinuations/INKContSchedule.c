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


/**************************************************************************
Verification of INKqa06643

Schedule a continuation that is simply called back with a later timeout
value. Explicitly call INKContSchedule() without a mutex, the mutex should
be created in InkAPI.cc/INKContSchedule.

This plug-in will not complete the client request (request times-out),
since the event routine calls INKContSchedule() in the event handler.
A simple change to the event routine can be made so that INKHttpTxnReenable()
is called in place of INKContSchedule().

Entry points to the core now use either
	FORCE_PLUGIN_MUTEX
or
	new_ProxyMutex()

to create/init a mutex.

**************************************************************************/


#include <sys/types.h>
#include <time.h>

#include <ts/ts.h>

/* Verification code for: INKqa06643 */
static int
EventHandler(INKCont contp, INKEvent event, void *eData)
{
  INKHttpTxn txn = (INKHttpTxn) eData;
  int iVal;
  time_t tVal;

  if (time(&tVal) != (time_t) (-1)) {
    INKDebug("tag_sched6643", "INKContSchedule: EventHandler: called at %s\n", ctime(&tVal));
  }

  iVal = (int) INKContDataGet(contp);

  INKDebug("tag_sched6643", "INKContSchedule: handler called with value %d\n", iVal);

  switch (event) {

  case INK_EVENT_HTTP_OS_DNS:
    INKDebug("tag_sched6643", "INKContSchedule: Seed event %s\n", "INK_EVENT_HTTP_OS_DNS");
    break;

  case INK_EVENT_TIMEOUT:
    INKDebug("tag_sched6643", "INKContSchedule: TIMEOUT event\n");
    break;

  default:
    INKDebug("tag_sched6643", "INKContSchedule: Error: default event\n");
    break;
  }

  iVal += 100;                  /* seed + timeout val */
  INKContDataSet(contp, (void *) iVal);
  INKContSchedule(contp, iVal);

  /* INKHttpTxnReenable(txn, INK_EVENT_HTTP_CONTINUE); */
}

void
INKPluginInit(int argc, const char *argv[])
{
  INKCont contp, contp2;
  int timeOut = 10;

  INKDebug("tag_sched6643", "INKContSchedule: Initial data value for contp is %d\n", timeOut);

  /* contp = INKContCreate(EventHandler, INKMutexCreate() ); */

  contp = INKContCreate(EventHandler, NULL);
  INKContDataSet(contp, (void *) timeOut);

  INKHttpHookAdd(INK_HTTP_OS_DNS_HOOK, contp);
}
