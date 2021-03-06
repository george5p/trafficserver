/** @file

  Private record core declarations

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

#ifndef _P_REC_CORE_H_
#define _P_REC_CORE_H_

#include "ink_thread.h"
#include "ink_hash_table.h"
#include "ink_llqueue.h"
#include "ink_rwlock.h"
#include "TextBuffer.h"

#include "I_RecCore.h"
#include "P_RecDefs.h"
#include "P_RecTree.h"

// records, record hash-table, and hash-table rwlock
extern RecRecord *g_records;
extern InkHashTable *g_records_ht;
extern ink_rwlock g_records_rwlock;
extern int g_num_records;
extern int g_num_update[];
extern RecTree *g_records_tree;

// records.config items
extern const char *g_rec_config_fpath;
extern LLQ *g_rec_config_contents_llq;
extern InkHashTable *g_rec_config_contents_ht;
extern ink_mutex g_rec_config_lock;

// stats.snap items
extern const char *g_stats_snap_fpath;

extern int g_type_records[][REC_MAX_RECORDS];
extern int g_type_num_records[];

//-------------------------------------------------------------------------
// Initialization
//-------------------------------------------------------------------------

int RecCoreInit(RecModeT mode_type, Diags * diags);

//-------------------------------------------------------------------------
// Registration/Insertion
//-------------------------------------------------------------------------

RecRecord *RecRegisterStat(RecT rec_type, const char *name, RecDataT data_type,
                           RecData data_default, RecPersistT persist_type);

RecRecord *RecRegisterConfig(RecT rec_type, const char *name, RecDataT data_type,
                             RecData data_default, RecUpdateT update_type,
                             RecCheckT check_type, const char *check_regex, RecAccessT access_type = RECA_NULL);

RecRecord *RecForceInsert(RecRecord * record);

//-------------------------------------------------------------------------
// Setting/Getting
//-------------------------------------------------------------------------

int RecSetRecord(RecT rec_type, const char *name, RecDataT data_type,
                 RecData *data, RecRawStat *raw_stat, bool lock = true);

int RecGetRecord_Xmalloc(const char *name, RecDataT data_type, RecData * data, bool lock = true);

//-------------------------------------------------------------------------
// Read/Sync to Disk
//-------------------------------------------------------------------------

int RecReadStatsFile();
int RecSyncStatsFile();
int RecReadConfigFile();
int RecSyncConfigToTB(textBuffer * tb);

//-------------------------------------------------------------------------
// Misc
//-------------------------------------------------------------------------

int RecExecConfigUpdateCbs();
int RecExecStatUpdateFuncs();
int RecExecRawStatUpdateFuncs();

void RecDumpRecordsHt(RecT rec_type = RECT_NULL);

#endif
