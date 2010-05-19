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

// Clocked Least Frequently Used by Size (CLFUS) replacement policy
// See https://cwiki.apache.org/confluence/display/TS/RamCache

#include "P_Cache.h"
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif
#ifdef HAVE_LZMA
#include <lzma.h>
#endif

#define REQUIRED_COMPRESSION 0.9 // must get to this size or declared incompressible
#define REQUIRED_SHRINK 0.8 // must get to this size or keep orignal buffer (with padding)
#define HISTORY_HYSTERIA 10 // extra temporary history
#define ENTRY_OVERHEAD 256 // per-entry overhead to consider when computing cache value/size
#define LZMA_BASE_MEMLIMIT (64 * 1024 * 1024)
//#define CHECK_ACOUNTING 1 // very expensive double checking of all sizes

#define REQUEUE_HITS(_h) ((_h) ? 1 : 0)
#define CACHE_VALUE_HITS_SIZE(_h, _s) ((float)((_h)+1) / ((_s) + ENTRY_OVERHEAD))
#define CACHE_VALUE(_x) CACHE_VALUE_HITS_SIZE((_x)->hits, (_x)->size)

struct RamCacheCLFUSEntry {
  INK_MD5 key;
  uint32 auxkey1;
  uint32 auxkey2;
  uint64 hits;
  uint32 size; // memory used including paddding in buffer
  uint32 len;  // actual data length
  uint32 compressed_len;
  union {
    struct {
      uint32 compressed:3; // compression type
      uint32 incompressible:1;
      uint32 lru:1;
      uint32 copy:1; // copy-in-copy-out
    };
    uint32 flags;
  };
  LINK(RamCacheCLFUSEntry, lru_link);
  LINK(RamCacheCLFUSEntry, hash_link);
  Ptr<IOBufferData> data;
};

struct RamCacheCLFUS : RamCache {
  int64 max_bytes;
  int64 bytes;
  int64 objects;

  // returns 1 on found/stored, 0 on not found/stored, if provided auxkey1 and auxkey2 must match
  int get(INK_MD5 *key, Ptr<IOBufferData> *ret_data, uint32 auxkey1 = 0, uint32 auxkey2 = 0);
  int put(INK_MD5 *key, IOBufferData *data, uint32 len, bool copy = false, uint32 auxkey1 = 0, uint32 auxkey2 = 0);
  int fixup(INK_MD5 *key, uint32 old_auxkey1, uint32 old_auxkey2, uint32 new_auxkey1, uint32 new_auxkey2);

  void init(int64 max_bytes, Part *part);

  // private
  Part *part; // for stats
  int64 history;
  int ibuckets;
  int nbuckets;
  DList(RamCacheCLFUSEntry, hash_link) *bucket;
  Que(RamCacheCLFUSEntry, lru_link) lru[2];
  uint16 *seen;
  int ncompressed;
  RamCacheCLFUSEntry *compressed; // first uncompressed lru[0] entry
  void compress_entries(int do_at_most = INT_MAX);
  void resize_hashtable();
  void victimize(RamCacheCLFUSEntry *e);
  void move_compressed(RamCacheCLFUSEntry *e);
  RamCacheCLFUSEntry *destroy(RamCacheCLFUSEntry *e);
  void requeue_victims(RamCacheCLFUS *c, Que(RamCacheCLFUSEntry, lru_link) &victims);
  void tick(); // move CLOCK on history
  RamCacheCLFUS(): max_bytes(0), bytes(0), objects(0), part(0), history(0), ibuckets(0), nbuckets(0), bucket(0),
              seen(0), ncompressed(0), compressed(0) { }
};

ClassAllocator<RamCacheCLFUSEntry> ramCacheCLFUSEntryAllocator("RamCacheCLFUSEntry");

const static int bucket_sizes[] = {
  127, 251, 509, 1021, 2039, 4093, 8191, 16381, 32749, 65521, 131071, 262139,
  524287, 1048573, 2097143, 4194301, 8388593, 16777213, 33554393, 67108859,
  134217689, 268435399, 536870909, 1073741789, 2147483647
};

void RamCacheCLFUS::resize_hashtable() {
  int anbuckets = bucket_sizes[ibuckets];
  DDebug("ram_cache", "resize hashtable %d", anbuckets);
  int64 s = anbuckets * sizeof(DList(RamCacheCLFUSEntry, hash_link));
  DList(RamCacheCLFUSEntry, hash_link) *new_bucket = (DList(RamCacheCLFUSEntry, hash_link) *)xmalloc(s);
  memset(new_bucket, 0, s);
  if (bucket) {
    for (int64 i = 0; i < nbuckets; i++) {
      RamCacheCLFUSEntry *e = 0;
      while ((e = bucket[i].pop()))
        new_bucket[e->key.word(3) % anbuckets].push(e);
    }
    xfree(bucket);
  }
  bucket = new_bucket;
  nbuckets = anbuckets;
  if (seen) xfree(seen);
  int size = bucket_sizes[ibuckets] * sizeof(uint16);
  seen = (uint16*)xmalloc(size);
  memset(seen, 0, size);
}

void RamCacheCLFUS::init(int64 abytes, Part *apart) {
  part = apart;
  max_bytes = abytes;
  DDebug("ram_cache", "initializing ram_cache %lld bytes", abytes);
  if (!max_bytes)
    return;
  resize_hashtable();
}

#ifdef CHECK_ACOUNTING
static void check_accounting(RamCacheCLFUS *c) {
  int64 x = 0, xsize = 0, h = 0;
  RamCacheCLFUSEntry *y = c->lru[0].head;
  while (y) { x++; xsize += y->size + ENTRY_OVERHEAD; y = y->lru_link.next; }
  y = c->lru[1].head;
  while (y) { h++; y = y->lru_link.next; }
  ink_assert(x == c->objects);
  ink_assert(xsize == c->bytes);
  ink_assert(h == c->history);
}
#else
#define check_accounting(_c)
#endif

int RamCacheCLFUS::get(INK_MD5 *key, Ptr<IOBufferData> *ret_data, uint32 auxkey1, uint32 auxkey2) {
  if (!max_bytes)
    return 0;
  int64 i = key->word(3) % nbuckets;
  RamCacheCLFUSEntry *e = bucket[i].head;
  char *b = 0;
  while (e) {
    if (e->key == *key && e->auxkey1 == auxkey1 && e->auxkey2 == auxkey2) {
      move_compressed(e);
      lru[e->lru].remove(e);
      lru[e->lru].enqueue(e);
      if (!e->lru) { // in memory
        e->hits++;
        if (e->compressed) {
          b = (char*)xmalloc(e->len);
          switch (e->compressed) {
            default: goto Lfailed;
            case CACHE_COMPRESSION_FASTLZ: {
              int l = (int)e->len;
              if ((l != (int)fastlz_decompress(e->data->data(), e->compressed_len, b, l)))
                goto Lfailed;
              break;
            }
#if HAVE_LIBZ
            case CACHE_COMPRESSION_LIBZ: {
              uLongf l = e->len;
              if (Z_OK != uncompress((Bytef*)b, &l, (Bytef*)e->data->data(), e->compressed_len))
                goto Lfailed;
              break;
            }
#endif
#ifdef HAVE_LZMA
            case CACHE_COMPRESSION_LIBLZMA: {
              size_t l = (size_t)e->len, ipos = 0, opos = 0;
              uint64_t memlimit = e->len * 2 + LZMA_BASE_MEMLIMIT;
              if (LZMA_OK != lzma_stream_buffer_decode(
                    &memlimit, 0, NULL, (uint8_t*)e->data->data(), &ipos, e->compressed_len, (uint8_t*)b, &opos, l))
                goto Lfailed;
              break;
            }
#endif
          }
          IOBufferData *data = new_xmalloc_IOBufferData(b, e->len);
          data->_mem_type = DEFAULT_ALLOC;
          if (!e->copy) { // don't bother if we have to copy anyway
            int64 delta = ((int64)e->compressed_len) - (int64)e->size;
            bytes += delta;
            CACHE_SUM_DYN_STAT_THREAD(cache_ram_cache_bytes_stat, delta);
            e->size = e->compressed_len;
            check_accounting(this);
            e->compressed = 0;
            e->data = data;
          }
          (*ret_data) = data;
        } else {
          IOBufferData *data = e->data;
          if (e->copy) {
            data = new_IOBufferData(iobuffer_size_to_index(e->len, MAX_BUFFER_SIZE_INDEX), MEMALIGNED);
            memcpy(data->data(), e->data->data(), e->len);
          }
          (*ret_data) = data;
        }
        CACHE_SUM_DYN_STAT_THREAD(cache_ram_cache_hits_stat, 1);
        DDebug("ram_cache", "get %X %d %d size %d HIT", key->word(3), auxkey1, auxkey2, e->size);
        return 1;
      } else {
        CACHE_SUM_DYN_STAT_THREAD(cache_ram_cache_misses_stat, 1);
        DDebug("ram_cache", "get %X %d %d HISTORY", key->word(3), auxkey1, auxkey2);
        return 0;
      }
    }
    assert(e != e->hash_link.next);
    e = e->hash_link.next;
  }
  DDebug("ram_cache", "get %X %d %d MISS", key->word(3), auxkey1, auxkey2);
Lerror:
  CACHE_SUM_DYN_STAT_THREAD(cache_ram_cache_misses_stat, 1);
  return 0;
Lfailed:
  xfree(b);
  e = destroy(e);
  DDebug("ram_cache", "get %X %d %d Z_ERR", key->word(3), auxkey1, auxkey2);
  goto Lerror;
}

void RamCacheCLFUS::tick() {
  RamCacheCLFUSEntry *e = lru[1].dequeue();
  if (!e)
    return;
  e->hits <<= 1;
  if (e->hits) {
    e->hits = REQUEUE_HITS(e->hits);
    lru[1].enqueue(e);
  } else
    goto Lfree;
  if (history <= objects + HISTORY_HYSTERIA)
    return;
  e = lru[1].dequeue();
Lfree:
  e->lru = 0;
  history--;
  uint32 b = e->key.word(3) % nbuckets;
  bucket[b].remove(e);
  DDebug("ram_cache", "put %X %d %d size %d FREED", e->key.word(3), e->auxkey1, e->auxkey2, e->size);
  THREAD_FREE(e, ramCacheCLFUSEntryAllocator, this_ethread());
}

void RamCacheCLFUS::victimize(RamCacheCLFUSEntry *e) {
  objects--;
  DDebug("ram_cache", "put %X %d %d size %d VICTIMIZED", e->key.word(3), e->auxkey1, e->auxkey2, e->size);
  e->data = NULL;
  e->lru = 1;
  lru[1].enqueue(e);
  history++;
}

void RamCacheCLFUS::move_compressed(RamCacheCLFUSEntry *e) {
  if (e == compressed) {
    ncompressed--;
    if (compressed->lru_link.next)
      compressed = compressed->lru_link.next;
    else {
      ncompressed--;
      compressed = compressed->lru_link.prev;
    }
  }
}

RamCacheCLFUSEntry *RamCacheCLFUS::destroy(RamCacheCLFUSEntry *e) {
  RamCacheCLFUSEntry *ret = e->hash_link.next;
  move_compressed(e);
  lru[e->lru].remove(e);
  if (!e->lru) {
    objects--;
    bytes -= e->size + ENTRY_OVERHEAD;
    CACHE_SUM_DYN_STAT_THREAD(cache_ram_cache_bytes_stat, -e->size);
    e->data = NULL;
  } else
    history--;
  uint32 b = e->key.word(3) % nbuckets;
  bucket[b].remove(e);
  DDebug("ram_cache", "put %X %d %d DESTROYED", e->key.word(3), e->auxkey1, e->auxkey2);
  THREAD_FREE(e, ramCacheCLFUSEntryAllocator, this_ethread());
  return ret;
}

void RamCacheCLFUS::compress_entries(int do_at_most) {
  if (!cache_config_ram_cache_compress)
    return;
  if (!compressed) {
    compressed = lru[0].head;
    ncompressed = 0;
  }
  float target = (cache_config_ram_cache_compress_percent / 100.0) * objects;
  int n = 0;
  char *b = 0, *bb = 0;
  while (compressed && target > ncompressed) {
    RamCacheCLFUSEntry *e = compressed;
    if (e->incompressible || e->compressed)
      goto Lcontinue;
    n++;
    if (do_at_most < n)
      break;
    {
      e->compressed_len = e->size;
      uLongf l = 0;
      int ctype = cache_config_ram_cache_compress;
      switch (ctype) {
        default: goto Lcontinue;
        case CACHE_COMPRESSION_FASTLZ: l = (uLongf)((double)e->len * 1.05 + 66); break;
#ifdef HAVE_LIBZ
        case CACHE_COMPRESSION_LIBZ: l = compressBound(e->len); break;
#endif
#ifdef HAVE_LZMA
        case CACHE_COMPRESSION_LIBLZMA: l = e->len; break;
#endif
      }
      b = (char*)xmalloc(l);
      switch (ctype) {
        default: goto Lfailed;
        case CACHE_COMPRESSION_FASTLZ:
          if (e->len < 16) goto Lfailed;
          if ((l = fastlz_compress(e->data->data(), e->len, b)) <= 0)
            goto Lfailed;
          break;
#if HAVE_LIBZ
        case CACHE_COMPRESSION_LIBZ: {
          uLongf ll = l;
          if ((Z_OK != compress((Bytef*)b, &ll, (Bytef*)e->data->data(), e->len)))
            goto Lfailed;
          l = (int)ll;
          break;
        }
#endif
#ifdef HAVE_LZMA
        case CACHE_COMPRESSION_LIBLZMA: {
          size_t pos = 0, ll = l;
          if (LZMA_OK != lzma_easy_buffer_encode(LZMA_PRESET_DEFAULT, LZMA_CHECK_NONE, NULL,
                                                 (uint8_t*)e->data->data(), e->len, (uint8_t*)b, &pos, ll))
            goto Lfailed;
          l = (int)pos;
          break;
        }
#endif
      }
      if (l > REQUIRED_COMPRESSION * e->len)
        e->incompressible = ctype;
      if (l > REQUIRED_SHRINK * e->size)
        goto Lfailed;
      if (l < e->len) {
        e->compressed = cache_config_ram_cache_compress;
        bb = (char*)xmalloc(l);
        memcpy(bb, b, l);
        xfree(b);
        e->compressed_len = l;
        int64 delta = ((int64)l) - (int64)e->size;
        bytes += delta;
        CACHE_SUM_DYN_STAT_THREAD(cache_ram_cache_bytes_stat, delta);
        e->size = l;
      } else {
        xfree(b);
        e->compressed = 0;
        bb = (char*)xmalloc(e->len);
        memcpy(bb, e->data->data(), e->len);
        int64 delta = ((int64)e->len) - (int64)e->size;
        bytes += delta;
        CACHE_SUM_DYN_STAT_THREAD(cache_ram_cache_bytes_stat, delta);
        e->size = e->len;
        l = e->len;
      }
      e->data = new_xmalloc_IOBufferData(bb, l);
      e->data->_mem_type = DEFAULT_ALLOC;
      check_accounting(this);
    }
    goto Lcontinue;
  Lfailed:
    xfree(b);
    e->incompressible = 1;
  Lcontinue:;
    DDebug("ram_cache", "compress %X %d %d %d %d %d %d",
           e->key.word(3), e->auxkey1, e->auxkey2, e->incompressible, e->compressed,
           e->len, e->compressed_len);
    if (!e->lru_link.next)
      break;
    compressed = e->lru_link.next;
    ncompressed++;
  }
}

void RamCacheCLFUS::requeue_victims(RamCacheCLFUS *c, Que(RamCacheCLFUSEntry, lru_link) &victims) {
  RamCacheCLFUSEntry *victim = 0;
  while ((victim = victims.dequeue())) {
    c->bytes += victim->size + ENTRY_OVERHEAD;
    CACHE_SUM_DYN_STAT_THREAD(cache_ram_cache_bytes_stat, victim->size);
    victim->hits = REQUEUE_HITS(victim->hits);
    c->lru[0].enqueue(victim);
  }
}

int RamCacheCLFUS::put(INK_MD5 *key, IOBufferData *data, uint32 len, bool copy, uint32 auxkey1, uint32 auxkey2) {
  if (!max_bytes)
    return 0;
  uint32 i = key->word(3) % nbuckets;
  RamCacheCLFUSEntry *e = bucket[i].head;
  uint32 size = copy ? len : data->block_size();
  while (e) {
    if (e->key == *key) {
      if (e->auxkey1 == auxkey1 && e->auxkey2 == auxkey2)
        break;
      else {
        e = destroy(e); // discard when aux keys conflict
        continue;
      }
    }
    e = e->hash_link.next;
  }
  if (e) {
    e->hits++;
    if (!e->lru) { // already in cache
      move_compressed(e);
      lru[e->lru].remove(e);
      lru[e->lru].enqueue(e);
      int64 delta = ((int64)size) - (int64)e->size;
      bytes += delta;
      CACHE_SUM_DYN_STAT_THREAD(cache_ram_cache_bytes_stat, delta);
      if (!copy) {
        e->size = size;
        e->data = data;
      } else {
        char *b = (char*)xmalloc(len);
        memcpy(b, data->data(), len);
        e->data = new_xmalloc_IOBufferData(b, len);
        e->data->_mem_type = DEFAULT_ALLOC;
        e->size = size;
      }
      check_accounting(this);
      e->copy = copy;
      e->compressed = 0;
      DDebug("ram_cache", "put %X %d %d size %d HIT", key->word(3), auxkey1, auxkey2, e->size);
      return 1;
    } else
      lru[1].remove(e);
  }
  Que(RamCacheCLFUSEntry, lru_link) victims;
  RamCacheCLFUSEntry *victim = 0;
  if (!lru[1].head) // initial fill
    if (bytes + size <= max_bytes)
      goto Linsert;
  if (!e) {
    uint32 s = key->word(3) % bucket_sizes[ibuckets];
    uint16 k = key->word(3) >> 16;
    uint16 kk = seen[s];
    seen[s] = k;
    if (history >= objects && kk != k) {
      DDebug("ram_cache", "put %X %d %d size %d UNSEEN", key->word(3), auxkey1, auxkey2, size);
      return 0;
    }
  }
  while (1) {
    victim = lru[0].dequeue();
    if (!victim) {
      if (bytes + size <= max_bytes)
        goto Linsert;
      if (e)
        lru[1].enqueue(e);
      requeue_victims(this, victims);
      DDebug("ram_cache", "put %X %d %d NO VICTIM", key->word(3), auxkey1, auxkey2);
      return 0;
    }
    bytes -= victim->size + ENTRY_OVERHEAD;
    CACHE_SUM_DYN_STAT_THREAD(cache_ram_cache_bytes_stat, -victim->size);
    victims.enqueue(victim);
    if (victim == compressed)
      compressed = 0;
    else
      ncompressed--;
    victim->hits <<= 1;
    tick();
    if (!e)
      goto Lhistory;
    else { // e from history
      DDebug("ram_cache_compare", "put %f %f", CACHE_VALUE(victim), CACHE_VALUE(e));
      if (bytes + victim->size + size > max_bytes && CACHE_VALUE(victim) > CACHE_VALUE(e)) {
        requeue_victims(this, victims);
        lru[1].enqueue(e);
        DDebug("ram_cache", "put %X %d %d size %d INC %d HISTORY",
               key->word(3), auxkey1, auxkey2, e->size, e->hits);
        return 0;
      }
    }
    if (bytes + size <= max_bytes)
      goto Linsert;
  }
Linsert:
  while ((victim = victims.dequeue())) {
    if (bytes + size + victim->size <= max_bytes) {
      bytes += victim->size + ENTRY_OVERHEAD;
      CACHE_SUM_DYN_STAT_THREAD(cache_ram_cache_bytes_stat, victim->size);
      victim->hits = REQUEUE_HITS(victim->hits);
      lru[0].enqueue(victim);
    } else
      victimize(victim);
  }
  if (e) {
    history--; // move from history
  } else {
    e = THREAD_ALLOC(ramCacheCLFUSEntryAllocator, this_ethread());
    e->key = *key;
    e->auxkey1 = auxkey1;
    e->auxkey2 = auxkey2;
    e->hits = 1;
    bucket[i].push(e);
    if (objects > nbuckets) {
      ++ibuckets;
      resize_hashtable();
    }
  }
  check_accounting(this);
  e->flags = 0;
  if (!copy)
    e->data = data;
  else {
    char *b = (char*)xmalloc(len);
    memcpy(b, data->data(), len);
    e->data = new_xmalloc_IOBufferData(b, len);
    e->data->_mem_type = DEFAULT_ALLOC;
  }
  e->copy = copy;
  bytes += size + ENTRY_OVERHEAD;
  CACHE_SUM_DYN_STAT_THREAD(cache_ram_cache_bytes_stat, size);
  e->size = size;
  objects++;
  lru[0].enqueue(e);
  e->len = len;
  check_accounting(this);
  DDebug("ram_cache", "put %X %d %d size %d INSERTED", key->word(3), auxkey1, auxkey2, e->size);
  if (cache_config_ram_cache_compress_percent)
    compress_entries();
  return 1;
Lhistory:
  requeue_victims(this, victims);
  check_accounting(this);
  e = THREAD_ALLOC(ramCacheCLFUSEntryAllocator, this_ethread());
  e->key = *key;
  e->auxkey1 = auxkey1;
  e->auxkey2 = auxkey2;
  e->hits = 1;
  e->size = data->block_size();
  e->flags = 0;
  bucket[i].push(e);
  e->lru = 1;
  lru[1].enqueue(e);
  history++;
  DDebug("ram_cache", "put %X %d %d HISTORY", key->word(3), auxkey1, auxkey2);
  return 0;
}

int RamCacheCLFUS::fixup(INK_MD5 * key, uint32 old_auxkey1, uint32 old_auxkey2, uint32 new_auxkey1, uint32 new_auxkey2) {
  if (!max_bytes)
    return 0;
  uint32 i = key->word(3) % nbuckets;
  RamCacheCLFUSEntry *e = bucket[i].head;
  while (e) {
    if (e->key == *key && e->auxkey1 == old_auxkey1 && e->auxkey2 == old_auxkey2) {
      e->auxkey1 = new_auxkey1;
      e->auxkey2 = new_auxkey2;
      return 1;
    }
    e = e->hash_link.next;
  }
  return 0;
}

RamCache *new_RamCacheCLFUS() {
  return new RamCacheCLFUS;
}
