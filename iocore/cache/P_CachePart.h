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


#ifndef _P_CACHE_PART_H__
#define _P_CACHE_PART_H__

#define INK_BLOCK_SHIFT			9
#define INK_BLOCK_SIZE			(1<<INK_BLOCK_SHIFT)
#define ROUND_TO_BLOCK(_x)	        (((_x)+(INK_BLOCK_SIZE-1))&~(INK_BLOCK_SIZE-1))
#define ROUND_TO(_x, _y)	        (((_x)+((_y)-1))&~((_y)-1))

// Part

#define PART_MAGIC			0xF1D0F00D
#define START_BLOCKS                    32      // 8k
#define START_POS			((off_t)START_BLOCKS * INK_BLOCK_SIZE)
#define AGG_HEADER_SIZE                 INK_BLOCK_SIZE
#define AGG_SIZE                        (4 * 1024 * 1024) // 4MB
#define AGG_HIGH_WATER                  (AGG_SIZE / 2) // 2MB
#define EVACUATION_SIZE                 (2 * AGG_SIZE)  // 8MB
#define MAX_PART_SIZE                   ((off_t)512 * 1024 * 1024 * 1024 * 1024)
#define STORE_BLOCKS_PER_DISK_BLOCK     (STORE_BLOCK_SIZE / INK_BLOCK_SIZE)
#define MAX_PART_BLOCKS                 (MAX_PART_SIZE / INK_BLOCK_SIZE)
#define TARGET_FRAG_SIZE                (DEFAULT_MAX_BUFFER_SIZE - sizeofDoc)
#define SHRINK_TARGET_FRAG_SIZE         (DEFAULT_MAX_BUFFER_SIZE + (DEFAULT_MAX_BUFFER_SIZE/4))
#define MAX_FRAG_SIZE                   ((256 * 1024) - sizeofDoc)
#define LEAVE_FREE                      DEFAULT_MAX_BUFFER_SIZE
#define PIN_SCAN_EVERY                  16      // scan every 1/16 of disk
#define PART_HASH_TABLE_SIZE            32707
#define PART_HASH_EMPTY                 0xFFFF
#define LOOKASIDE_SIZE                  256
#define EVACUATION_BUCKET_SIZE          (2 * EVACUATION_SIZE) // 16MB
#define RECOVERY_SIZE                   EVACUATION_SIZE // 8MB
#define AIO_NOT_IN_PROGRESS             0
#define AIO_AGG_WRITE_IN_PROGRESS       -1
#define AUTO_SIZE_RAM_CACHE             -1      // 1-1 with directory size


#define dir_offset_evac_bucket(_o) \
  (_o / (EVACUATION_BUCKET_SIZE / INK_BLOCK_SIZE))
#define dir_evac_bucket(_e) dir_offset_evac_bucket(dir_offset(_e))
#define offset_evac_bucket(_d, _o) \
  dir_offset_evac_bucket((offset_to_part_offset(_d, _o)

// Documents

#define DOC_MAGIC                       ((uint32)0x5F129B13)
#define DOC_CORRUPT                     ((uint32)0xDEADBABE)
#define DOC_NO_CHECKSUM                 ((uint32)0xA0B0C0D0)

#define sizeofDoc (((uint32)(uintptr_t)&((Doc*)0)->checksum)+(uint32)sizeof(uint32))

struct Cache;
struct Part;
struct CacheDisk;
struct PartInitInfo;
struct DiskPart;
struct CachePart;

struct PartHeaderFooter
{
  unsigned int magic;
  VersionNumber version;
  time_t create_time;
  off_t write_pos;
  off_t last_write_pos;
  off_t agg_pos;
  uint32 generation;            // token generation (vary), this cannot be 0
  uint32 phase;
  uint32 cycle;
  uint32 sync_serial;
  uint32 write_serial;
  uint32 dirty;
  uint16 freelist[1];
};

// Key and Earliest key for each fragment that needs to be evacuated
struct EvacuationKey
{
  SLink<EvacuationKey> link;
  INK_MD5 key;
  INK_MD5 earliest_key;
};

struct EvacuationBlock
{
  union
  {
    unsigned int init;
    struct
    {
      unsigned int done:1;              // has been evacuated
      unsigned int pinned:1;            // check pinning timeout
      unsigned int evacuate_head:1;     // check pinning timeout
      unsigned int unused:29;
    } f;
  };
  int readers;
  Dir dir;
  Dir new_dir;
  // we need to have a list of evacuationkeys because of collision.
  EvacuationKey evac_frags;
  CacheVC *earliest_evacuator;
  LINK(EvacuationBlock, link);
};

struct Part:public Continuation
{
  char *path;
  char *hash_id;
  INK_MD5 hash_id_md5;
  int fd;

  char *raw_dir;
  Dir *dir;
  PartHeaderFooter *header;
  PartHeaderFooter *footer;
  int segments;
  off_t buckets;
  off_t recover_pos;
  off_t prev_recover_pos;
  off_t scan_pos;
  off_t skip;               // start of headers
  off_t start;              // start of data
  off_t len;
  off_t data_blocks;
  int hit_evacuate_window;
  AIOCallbackInternal io;

  Queue<CacheVC, Continuation::Link_link> agg;
  Queue<CacheVC, Continuation::Link_link> stat_cache_vcs;
  Queue<CacheVC, Continuation::Link_link> sync;
  char *agg_buffer;
  int agg_todo_size;
  int agg_buf_pos;

  Event *trigger;

  OpenDir open_dir;
  RamCache *ram_cache;
  int evacuate_size;
  DLL<EvacuationBlock> *evacuate;
  DLL<EvacuationBlock> lookaside[LOOKASIDE_SIZE];
  CacheVC *doc_evacuator;

  PartInitInfo *init_info;

  CacheDisk *disk;
  Cache *cache;
  CachePart *cache_part;
  uint32 last_sync_serial;
  uint32 last_write_serial;
  bool recover_wrapped;
  bool dir_sync_waiting;
  bool dir_sync_in_progress;
  bool writing_end_marker;

  CacheKey first_fragment_key;
  int64 first_fragment_offset;
  Ptr<IOBufferData> first_fragment_data;

  void cancel_trigger();

  int open_write(CacheVC *cont, int allow_if_writers, int max_writers);
  int open_write_lock(CacheVC *cont, int allow_if_writers, int max_writers);
  int close_write(CacheVC *cont);
  int close_write_lock(CacheVC *cont);
  int begin_read(CacheVC *cont);
  int begin_read_lock(CacheVC *cont);
  // unused read-write interlock code
  // currently http handles a write-lock failure by retrying the read
  OpenDirEntry *open_read(INK_MD5 * key);
  OpenDirEntry *open_read_lock(INK_MD5 * key, EThread * t);
  int close_read(CacheVC * cont);
  int close_read_lock(CacheVC * cont);

  int clear_dir();

  int init(char *s, off_t blocks, off_t dir_skip, bool clear);

  int handle_dir_clear(int event, void *data);
  int handle_dir_read(int event, void *data);
  int handle_recover_from_data(int event, void *data);
  int handle_recover_write_dir(int event, void *data);
  int handle_header_read(int event, void *data);

  int dir_init_done(int event, void *data);

  int dir_check(bool fix);
  int db_check(bool fix);

  int is_io_in_progress()
  {
    return io.aiocb.aio_fildes != AIO_NOT_IN_PROGRESS;
  }
  int increment_generation()
  {
    // this is stored in the offset field of the directory (!=0)
    ink_debug_assert(mutex->thread_holding == this_ethread());
    header->generation++;
    if (!header->generation)
      header->generation++;
    return header->generation;
  }
  void set_io_not_in_progress()
  {
    io.aiocb.aio_fildes = AIO_NOT_IN_PROGRESS;
  }

  int aggWriteDone(int event, Event *e);
  int aggWrite(int event, void *e);
  void agg_wrap();

  int evacuateWrite(CacheVC *evacuator, int event, Event *e);
  int evacuateDocReadDone(int event, Event *e);
  int evacuateDoc(int event, Event *e);

  int evac_range(off_t start, off_t end, int evac_phase);
  void periodic_scan();
  void scan_for_pinned_documents();
  void evacuate_cleanup_blocks(int i);
  void evacuate_cleanup();
  EvacuationBlock *force_evacuate_head(Dir *dir, int pinned);
  int within_hit_evacuate_window(Dir *dir);

  Part():Continuation(new_ProxyMutex()), path(NULL), fd(-1),
         dir(0), buckets(0), recover_pos(0), prev_recover_pos(0), scan_pos(0), skip(0), start(0),
         len(0), data_blocks(0), hit_evacuate_window(0), agg_todo_size(0), agg_buf_pos(0), trigger(0),
         evacuate_size(0), disk(NULL), last_sync_serial(0), last_write_serial(0), recover_wrapped(false),
         dir_sync_waiting(0), dir_sync_in_progress(0), writing_end_marker(0) {
    open_dir.mutex = mutex;
#if defined(_WIN32)
    agg_buffer = (char *) malloc(AGG_SIZE);
#else
    agg_buffer = (char *) ink_memalign(sysconf(_SC_PAGESIZE), AGG_SIZE);
#endif
    memset(agg_buffer, 0, AGG_SIZE);
    SET_HANDLER(&Part::aggWrite);
  }

  ~Part() {
    ink_memalign_free(agg_buffer);
  }
};

struct AIO_Callback_handler:public Continuation
{
  int handle_disk_failure(int event, void *data);

  AIO_Callback_handler():Continuation(new_ProxyMutex()) {
    SET_HANDLER(&AIO_Callback_handler::handle_disk_failure);
  }
};

struct CachePart
{
  int part_number;
  int scheme;
  int size;
  int num_parts;
  Part **parts;
  DiskPart **disk_parts;
  LINK(CachePart, link);
  // per partition stats
  RecRawStatBlock *part_rsb;

  CachePart():part_number(-1), scheme(0), size(0), num_parts(0), parts(NULL), disk_parts(0), part_rsb(0) { }
};

// element of the fragment table in the head of a multi-fragment document
struct Frag {
  uint64 offset; // start offset of data stored in this fragment
};

// Note : hdr() needs to be 8 byte aligned.
// If you change this, change sizeofDoc above
struct Doc
{
  uint32 magic;                 // DOC_MAGIC
  uint32 len;                   // length of this segment
  uint64 total_len;             // total length of document
  INK_MD5 first_key;            // first key in document (http: vector)
  INK_MD5 key;
  uint32 hlen;                  // header length
  uint32 ftype:8;               // fragment type CACHE_FRAG_TYPE_XX
  uint32 flen:24;               // fragment table length
  uint32 sync_serial;
  uint32 write_serial;
  uint32 pinned;                // pinned until
  uint32 checksum;

  uint32 data_len();
  uint32 prefix_len();
  int single_fragment();
  int no_data_in_fragment();
  uint32 nfrags();
  char *hdr();
  Frag *frags();
  char *data();
};

// Global Data

extern Part **gpart;
extern volatile int gnpart;
extern ClassAllocator<OpenDirEntry> openDirEntryAllocator;
extern ClassAllocator<EvacuationBlock> evacuationBlockAllocator;
extern ClassAllocator<EvacuationKey> evacuationKeyAllocator;
extern unsigned short *part_hash_table;

// inline Functions

TS_INLINE int
part_headerlen(Part *d) {
  return ROUND_TO_BLOCK(sizeof(PartHeaderFooter) + sizeof(uint16) * (d->segments-1));
}
TS_INLINE int
part_dirlen(Part * d)
{
  return ROUND_TO_BLOCK(d->buckets * DIR_DEPTH * d->segments * SIZEOF_DIR) +
    part_headerlen(d) + ROUND_TO_BLOCK(sizeof(PartHeaderFooter));
}
TS_INLINE int
part_direntries(Part * d)
{
  return d->buckets * DIR_DEPTH * d->segments;
}
TS_INLINE int
part_out_of_phase_valid(Part * d, Dir * e)
{
  return (dir_offset(e) - 1 >= ((d->header->agg_pos - d->start) / INK_BLOCK_SIZE));
}
TS_INLINE int
part_out_of_phase_agg_valid(Part * d, Dir * e)
{
  return (dir_offset(e) - 1 >= ((d->header->agg_pos - d->start + AGG_SIZE) / INK_BLOCK_SIZE));
}
TS_INLINE int
part_out_of_phase_write_valid(Part * d, Dir * e)
{
  return (dir_offset(e) - 1 >= ((d->header->write_pos - d->start) / INK_BLOCK_SIZE));
}
TS_INLINE int
part_in_phase_valid(Part * d, Dir * e)
{
  return (dir_offset(e) - 1 < ((d->header->write_pos + d->agg_buf_pos - d->start) / INK_BLOCK_SIZE));
}
TS_INLINE off_t
part_offset(Part * d, Dir * e)
{
  return d->start + (off_t) dir_offset(e) * INK_BLOCK_SIZE - INK_BLOCK_SIZE;
}
TS_INLINE off_t
offset_to_part_offset(Part * d, off_t pos)
{
  return ((pos - d->start + INK_BLOCK_SIZE) / INK_BLOCK_SIZE);
}
TS_INLINE off_t
part_offset_to_offset(Part * d, off_t pos)
{
  return d->start + pos * INK_BLOCK_SIZE - INK_BLOCK_SIZE;
}
TS_INLINE Dir *
part_dir_segment(Part * d, int s)
{
  return (Dir *) (((char *) d->dir) + (s * d->buckets) * DIR_DEPTH * SIZEOF_DIR);
}
TS_INLINE int
part_in_phase_agg_buf_valid(Part * d, Dir * e)
{
  return (part_offset(d, e) >= d->header->write_pos && part_offset(d, e) < (d->header->write_pos + d->agg_buf_pos));
}
TS_INLINE uint32
Doc::prefix_len()
{
  return sizeofDoc + hlen + flen;
}
TS_INLINE uint32
Doc::data_len()
{
  return len - sizeofDoc - hlen - flen;
}
TS_INLINE int
Doc::single_fragment()
{
  return (total_len && (data_len() == total_len));
}
TS_INLINE uint32
Doc::nfrags() {
  return flen / sizeof(Frag);
}
TS_INLINE Frag *
Doc::frags()
{
  return (Frag*)(((char *) this) + sizeofDoc);
}
TS_INLINE char *
Doc::hdr()
{
  return ((char *) this) + sizeofDoc + flen;
}
TS_INLINE char *
Doc::data()
{
  return ((char *) this) + sizeofDoc + flen + hlen;
}

int part_dir_clear(Part * d);
int part_init(Part * d, char *s, off_t blocks, off_t skip, bool clear);

// inline Functions

TS_INLINE EvacuationBlock *
evacuation_block_exists(Dir * dir, Part * p)
{
  EvacuationBlock *b = p->evacuate[dir_evac_bucket(dir)].head;
  for (; b; b = b->link.next)
    if (dir_offset(&b->dir) == dir_offset(dir))
      return b;
  return 0;
}

TS_INLINE void
Part::cancel_trigger()
{
  if (trigger) {
    trigger->cancel_action();
    trigger = NULL;
  }
}

TS_INLINE EvacuationBlock *
new_EvacuationBlock(EThread * t)
{
  EvacuationBlock *b = THREAD_ALLOC(evacuationBlockAllocator, t);
  b->init = 0;
  b->readers = 0;
  b->earliest_evacuator = 0;
  b->evac_frags.link.next = 0;
  return b;
}

TS_INLINE void
free_EvacuationBlock(EvacuationBlock * b, EThread * t)
{
  EvacuationKey *e = b->evac_frags.link.next;
  while (e) {
    EvacuationKey *n = e->link.next;
    evacuationKeyAllocator.free(e);
    e = n;
  }
  THREAD_FREE(b, evacuationBlockAllocator, t);
}

TS_INLINE OpenDirEntry *
Part::open_read(INK_MD5 * key)
{
  return open_dir.open_read(key);
}

TS_INLINE int
Part::within_hit_evacuate_window(Dir * xdir)
{
  off_t oft = dir_offset(xdir) - 1;
  off_t write_off = (header->write_pos + AGG_SIZE - start) / INK_BLOCK_SIZE;
  off_t delta = oft - write_off;
  if (delta >= 0)
    return delta < hit_evacuate_window;
  else
    return -delta > (data_blocks - hit_evacuate_window) && -delta < data_blocks;
}

#endif /* _P_CACHE_PART_H__ */
