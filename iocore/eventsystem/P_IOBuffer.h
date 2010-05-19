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



#if !defined (_P_IOBuffer_h)
#define _P_IOBuffer_h
#include "inktomi++.h"

//////////////////////////////////////////////////////////////
//
// returns 0 for DEFAULT_BUFFER_BASE_SIZE,
// +1 for each power of 2
//
//////////////////////////////////////////////////////////////
TS_INLINE int64
buffer_size_to_index(int64 size, int64 max = max_iobuffer_size)
{
  int r = max;
  while (r && BUFFER_SIZE_FOR_INDEX(r - 1) >= size)
    r--;
  return r;
}

TS_INLINE int64
iobuffer_size_to_index(int64 size, int64 max)
{
  if (size > BUFFER_SIZE_FOR_INDEX(max))
    return BUFFER_SIZE_INDEX_FOR_XMALLOC_SIZE(size);
  return buffer_size_to_index(size, max);
}

TS_INLINE int64
index_to_buffer_size(int64 idx)
{
  if (BUFFER_SIZE_INDEX_IS_FAST_ALLOCATED(idx))
    return BUFFER_SIZE_FOR_INDEX(idx);
  else if (BUFFER_SIZE_INDEX_IS_XMALLOCED(idx))
    return BUFFER_SIZE_FOR_XMALLOC(idx);
  // coverity[dead_error_condition]
  else if (BUFFER_SIZE_INDEX_IS_CONSTANT(idx))
    return BUFFER_SIZE_FOR_CONSTANT(idx);
  // coverity[dead_error_line]
  return 0;
}

TS_INLINE IOBufferBlock *
iobufferblock_clone(IOBufferBlock * b, int64 offset, int64 len)
{

  IOBufferBlock *start_buf = NULL;
  IOBufferBlock *current_buf = NULL;

  while (b && len >= 0) {
    char *start = b->_start;
    char *end = b->_end;
    int64 max_bytes = end - start;
    max_bytes -= offset;
    if (max_bytes <= 0) {
      offset = -max_bytes;
      b = b->next;
      continue;
    }
    int64 bytes = len;
    if (bytes >= max_bytes)
      bytes = max_bytes;
    IOBufferBlock *new_buf = b->clone();
    new_buf->_start += offset;
    new_buf->_buf_end = new_buf->_end = new_buf->_start + bytes;
    if (!start_buf) {
      start_buf = new_buf;
      current_buf = start_buf;
    } else {
      current_buf->next = new_buf;
      current_buf = new_buf;
    }
    len -= bytes;
    b = b->next;
    offset = 0;
  }
  return start_buf;
}

TS_INLINE IOBufferBlock *
iobufferblock_skip(IOBufferBlock * b, int64 *poffset, int64 *plen, int64 write)
{
  int64 offset = *poffset;
  int64 len = write;
  while (b && len >= 0) {
    int64 max_bytes = b->read_avail();
    max_bytes -= offset;
    if (max_bytes <= 0) {
      offset = -max_bytes;
      b = b->next;
      continue;
    }
    if (len >= max_bytes) {
      b = b->next;
      len -= max_bytes;
      offset = 0;
    } else {
      offset = offset + len;
      break;
    }
  }
  *poffset = offset;
  *plen -= write;
  return b;
}

#ifdef TRACK_BUFFER_USER
struct Resource;
extern Resource *res_lookup(const char *path);

TS_INLINE void
iobuffer_mem_inc(const char *_loc, int64 _size_index)
{
  if (!res_track_memory)
    return;

  if (!BUFFER_SIZE_INDEX_IS_FAST_ALLOCATED(_size_index))
    return;

  if (!_loc)
    _loc = "memory/IOBuffer/UNKNOWN-LOCATION";
  Resource *res = res_lookup(_loc);
  ink_debug_assert(strcmp(_loc, res->path) == 0);
  int64 r = ink_atomic_increment64(&res->value,
                                   index_to_buffer_size(_size_index));
  NOWARN_UNUSED(r);
  ink_debug_assert(r >= 0);
}

TS_INLINE void
iobuffer_mem_dec(const char *_loc, int64 _size_index)
{
  if (!res_track_memory)
    return;

  if (!BUFFER_SIZE_INDEX_IS_FAST_ALLOCATED(_size_index))
    return;
  if (!_loc)
    _loc = "memory/IOBuffer/UNKNOWN-LOCATION";
  Resource *res = res_lookup(_loc);
  ink_debug_assert(strcmp(_loc, res->path) == 0);
  int64 r = ink_atomic_increment64(&res->value,
                                   -index_to_buffer_size(_size_index));
  NOWARN_UNUSED(r);
  ink_debug_assert(r >= index_to_buffer_size(_size_index));
}
#endif

//////////////////////////////////////////////////////////////////
//
// inline functions definitions
//
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//
//  class IOBufferData --
//         inline functions definitions
//
//////////////////////////////////////////////////////////////////
TS_INLINE int64
IOBufferData::block_size()
{
  return index_to_buffer_size(_size_index);
}

TS_INLINE IOBufferData *
new_IOBufferData_internal(
#ifdef TRACK_BUFFER_USER
                           const char *location,
#endif
                           void *b, int64 size, int64 asize_index)
{
  (void) size;
  IOBufferData *d = ioDataAllocator.alloc();
  d->_size_index = asize_index;
  ink_assert(BUFFER_SIZE_INDEX_IS_CONSTANT(asize_index)
             || size <= d->block_size());
#ifdef TRACK_BUFFER_USER
  d->_location = location;
#endif
  d->_data = (char *) b;
  return d;
}

TS_INLINE IOBufferData *
new_constant_IOBufferData_internal(
#ifdef TRACK_BUFFER_USER
                                    const char *loc,
#endif
                                    void *b, int64 size)
{
  return new_IOBufferData_internal(
#ifdef TRACK_BUFFER_USER
                                    loc,
#endif
                                    b, size, BUFFER_SIZE_INDEX_FOR_CONSTANT_SIZE(size));
}

TS_INLINE IOBufferData *
new_xmalloc_IOBufferData_internal(
#ifdef TRACK_BUFFER_USER
                                   const char *location,
#endif
                                   void *b, int64 size)
{
  return new_IOBufferData_internal(
#ifdef TRACK_BUFFER_USER
                                    location,
#endif
                                    b, size, BUFFER_SIZE_INDEX_FOR_XMALLOC_SIZE(size));
}

TS_INLINE IOBufferData *
new_IOBufferData_internal(
#ifdef TRACK_BUFFER_USER
                           const char *location,
#endif
                           void *b, int64 size)
{
  return new_IOBufferData_internal(
#ifdef TRACK_BUFFER_USER
                                    location,
#endif
                                    b, size, iobuffer_size_to_index(size));
}

TS_INLINE IOBufferData *
new_IOBufferData_internal(
#ifdef TRACK_BUFFER_USER
                           const char *loc,
#endif
                           int64 size_index, AllocType type)
{
  IOBufferData *d = ioDataAllocator.alloc();
#ifdef TRACK_BUFFER_USER
  d->_location = loc;
#endif
  d->alloc(size_index, type);
  return d;
}

// IRIX has a compiler bug which prevents this function
// from being compiled correctly at -O3
// so it is DUPLICATED in IOBuffer.cc
// ****** IF YOU CHANGE THIS FUNCTION change that one as well.
TS_INLINE void
IOBufferData::alloc(int64 size_index, AllocType type)
{
  if (_data)
    dealloc();
  _size_index = size_index;
  _mem_type = type;
#ifdef TRACK_BUFFER_USER
  iobuffer_mem_inc(_location, size_index);
#endif
  switch (type) {
  case MEMALIGNED:
    if (BUFFER_SIZE_INDEX_IS_FAST_ALLOCATED(size_index))
      _data = (char *) ioBufAllocator[size_index].alloc_void();
    // coverity[dead_error_condition]
    else if (BUFFER_SIZE_INDEX_IS_XMALLOCED(size_index))
      // coverity[dead_error_line]
      _data = (char *) valloc(index_to_buffer_size(size_index));
    break;
  default:
  case DEFAULT_ALLOC:
    if (BUFFER_SIZE_INDEX_IS_FAST_ALLOCATED(size_index))
      _data = (char *) ioBufAllocator[size_index].alloc_void();
    else if (BUFFER_SIZE_INDEX_IS_XMALLOCED(size_index))
      _data = (char *) xmalloc(BUFFER_SIZE_FOR_XMALLOC(size_index));
    break;
  }
}

// ****** IF YOU CHANGE THIS FUNCTION change that one as well.


TS_INLINE void
IOBufferData::dealloc()
{
#ifdef TRACK_BUFFER_USER
  iobuffer_mem_dec(_location, _size_index);
#endif
  switch (_mem_type) {
  case MEMALIGNED:
    if (BUFFER_SIZE_INDEX_IS_FAST_ALLOCATED(_size_index))
      ioBufAllocator[_size_index].free_void(_data);
    else if (BUFFER_SIZE_INDEX_IS_XMALLOCED(_size_index))
      ::free((void *) _data);
    break;
  default:
  case DEFAULT_ALLOC:
    if (BUFFER_SIZE_INDEX_IS_FAST_ALLOCATED(_size_index))
      ioBufAllocator[_size_index].free_void(_data);
    else if (BUFFER_SIZE_INDEX_IS_XMALLOCED(_size_index))
      xfree(_data);
    break;
  }
  _data = 0;
  _size_index = BUFFER_SIZE_NOT_ALLOCATED;
  _mem_type = NO_ALLOC;
}

TS_INLINE void
IOBufferData::free()
{
  dealloc();
  ioDataAllocator.free(this);
}

//////////////////////////////////////////////////////////////////
//
//  class IOBufferBlock --
//         inline functions definitions
//
//////////////////////////////////////////////////////////////////
TS_INLINE IOBufferBlock *
new_IOBufferBlock_internal(
#ifdef TRACK_BUFFER_USER
                            const char *location
#endif
  )
{
  IOBufferBlock *b = ioBlockAllocator.alloc();
#ifdef TRACK_BUFFER_USER
  b->_location = location;
#endif
  return b;
}

TS_INLINE IOBufferBlock *
new_IOBufferBlock_internal(
#ifdef TRACK_BUFFER_USER
                            const char *location,
#endif
                            IOBufferData * d, int64 len, int64 offset)
{
  IOBufferBlock *b = ioBlockAllocator.alloc();
#ifdef TRACK_BUFFER_USER
  b->_location = location;
#endif
  b->set(d, len, offset);
  return b;
}

TS_INLINE
IOBufferBlock::IOBufferBlock():
_start(0),
_end(0),
_buf_end(0)
#ifdef TRACK_BUFFER_USER
,
_location(0)
#endif
{
  return;
}

TS_INLINE void
IOBufferBlock::consume(int64 len)
{
  _start += len;
  ink_assert(_start <= _end);
}

TS_INLINE void
IOBufferBlock::fill(int64 len)
{
  _end += len;
  ink_assert(_end <= _buf_end);
}

TS_INLINE void
IOBufferBlock::reset()
{
  _end = _start = buf();
  _buf_end = buf() + data->block_size();
}

TS_INLINE void
IOBufferBlock::alloc(int64 i)
{
  ink_debug_assert(BUFFER_SIZE_ALLOCATED(i));
#ifdef TRACK_BUFFER_USER
  data = new_IOBufferData_internal(_location, i);
#else
  data = new_IOBufferData_internal(i);
#endif
  reset();
}

TS_INLINE void
IOBufferBlock::clear()
{
  data = NULL;
  IOBufferBlock *p = next;
  while (p) {
    int r = p->refcount_dec();
    if (r)
      break;
    else {
      IOBufferBlock *n = p->next.m_ptr;
      p->next.m_ptr = NULL;
      p->free();
      p = n;
    }
  }
  next.m_ptr = NULL;
  _buf_end = _end = _start = NULL;
}

TS_INLINE IOBufferBlock *
IOBufferBlock::clone()
{
#ifdef TRACK_BUFFER_USER
  IOBufferBlock *b = new_IOBufferBlock_internal(_location);
#else
  IOBufferBlock *b = new_IOBufferBlock_internal();
#endif
  b->data = data;
  b->_start = _start;
  b->_end = _end;
  b->_buf_end = _end;
#ifdef TRACK_BUFFER_USER
  b->_location = _location;
#endif
  return b;
}

TS_INLINE void
IOBufferBlock::dealloc()
{
  clear();
}

TS_INLINE void
IOBufferBlock::free()
{
  dealloc();
  ioBlockAllocator.free(this);
}

TS_INLINE void
IOBufferBlock::set_internal(void *b, int64 len, int64 asize_index)
{
#ifdef TRACK_BUFFER_USER
  data = new_IOBufferData_internal(_location, BUFFER_SIZE_NOT_ALLOCATED);
#else
  data = new_IOBufferData_internal(BUFFER_SIZE_NOT_ALLOCATED);
#endif
  data->_data = (char *) b;
#ifdef TRACK_BUFFER_USER
  iobuffer_mem_inc(_location, asize_index);
#endif
  data->_size_index = asize_index;
  reset();
  _end = _start + len;
}

TS_INLINE void
IOBufferBlock::set(IOBufferData * d, int64 len, int64 offset)
{
  data = d;
  _start = buf() + offset;
  _end = _start + len;
  _buf_end = _start + d->block_size();
}

TS_INLINE void
IOBufferBlock::realloc_set_internal(void *b, int64 buf_size, int64 asize_index)
{
  int64 data_size = size();
  memcpy(b, _start, size());
  dealloc();
  set_internal(b, buf_size, asize_index);
  _end = _start + data_size;
}

TS_INLINE void
IOBufferBlock::realloc(void *b, int64 buf_size)
{
  realloc_set_internal(b, buf_size, BUFFER_SIZE_NOT_ALLOCATED);
}

TS_INLINE void
IOBufferBlock::realloc_xmalloc(void *b, int64 buf_size)
{
  realloc_set_internal(b, buf_size, -buf_size);
}

TS_INLINE void
IOBufferBlock::realloc_xmalloc(int64 buf_size)
{
  realloc_set_internal(xmalloc(buf_size), buf_size, -buf_size);
}

TS_INLINE void
IOBufferBlock::realloc(int64 i)
{
  if (i == data->_size_index)
    return;
  if (i >= (int64) sizeof(ioBufAllocator))
    return;

  ink_release_assert(i > data->_size_index && i != BUFFER_SIZE_NOT_ALLOCATED);
  void *b = ioBufAllocator[i].alloc_void();
  realloc_set_internal(b, BUFFER_SIZE_FOR_INDEX(i), i);
}

//////////////////////////////////////////////////////////////////
//
//  class IOBufferReader --
//         inline functions definitions
//
//////////////////////////////////////////////////////////////////
TS_INLINE void
IOBufferReader::skip_empty_blocks()
{
  while (block->next && block->next->read_avail() && start_offset >= block->size()) {
    start_offset -= block->size();
    block = block->next;
  }
}

TS_INLINE bool
IOBufferReader::low_water()
{
  return mbuf->low_water();
}

TS_INLINE bool
IOBufferReader::high_water()
{
  return read_avail() >= mbuf->water_mark;
}

TS_INLINE bool
IOBufferReader::current_low_water()
{
  return mbuf->current_low_water();
}

TS_INLINE IOBufferBlock *
IOBufferReader::get_current_block()
{
  return (block);
}

TS_INLINE char *
IOBufferReader::start()
{
  if (block == 0)
    return (0);
  skip_empty_blocks();
  return block->start() + start_offset;
}

TS_INLINE char *
IOBufferReader::end()
{
  if (block == 0)
    return (0);
  skip_empty_blocks();
  return block->end();
}

TS_INLINE int64
IOBufferReader::block_read_avail()
{
  if (block == 0)
    return (0);
  skip_empty_blocks();
  return (int64) (block->end() - (block->start() + start_offset));
}

TS_INLINE int
IOBufferReader::block_count()
{
  int count = 0;
  IOBufferBlock *b = block;
  while (b) {
    count++;
    b = b->next;
  }
  return count;
}

TS_INLINE int64
IOBufferReader::read_avail()
{
  int64 t = 0;
  IOBufferBlock *b = block;
  while (b) {
    t += b->read_avail();
    b = b->next;
  }
  t -= start_offset;
  if (size_limit != INT64_MAX && t > size_limit)
    t = size_limit;
  return t;
}

TS_INLINE void
IOBufferReader::consume(int64 n)
{
  start_offset += n;
  if (size_limit != INT64_MAX)
    size_limit -= n;
  ink_assert(size_limit >= 0);
  if (block == 0)
    return;
  int64 r = block->read_avail();
  int64 s = start_offset;
  while (r <= s && block->next && block->next->read_avail()) {
    s -= r;
    start_offset = s;
    block = block->next;
    r = block->read_avail();
  }
  ink_debug_assert(read_avail() >= 0);
}

TS_INLINE char &
IOBufferReader::operator[] (int64 i)
{
  static char
    _error = '\0';

  IOBufferBlock *
    b = block;
  i += start_offset;
  while (b) {
    int64 bytes = b->read_avail();
    if (bytes > i)
      return b->start()[i];
    i -= bytes;
    b = b->next;
  }

  ink_assert(!"out of range");
  if (unlikely(b))
    return *b->start();

  return _error;
}

TS_INLINE void
IOBufferReader::clear()
{
  accessor = NULL;
  block = NULL;
  mbuf = NULL;
  start_offset = 0;
  size_limit = INT64_MAX;
}

TS_INLINE void
IOBufferReader::reset()
{
  block = mbuf->_writer;
  start_offset = 0;
  size_limit = INT64_MAX;
}

////////////////////////////////////////////////////////////////
//
//  class MIOBuffer --
//      inline functions definitions
//
////////////////////////////////////////////////////////////////
inkcoreapi extern ClassAllocator<MIOBuffer> ioAllocator;
////////////////////////////////////////////////////////////////
//
//  MIOBuffer::MIOBuffer()
//
//  This constructor accepts a pre-allocated memory buffer,
//  wraps if in a IOBufferData and IOBufferBlock structures
//  and sets it as the current block.
//  NOTE that in this case the memory buffer will not be freed
//  by the MIOBuffer class. It is the user responsibility to
//  free the memory buffer. The wrappers (MIOBufferBlock and
//  MIOBufferData) will be freed by this class.
//
////////////////////////////////////////////////////////////////
TS_INLINE
MIOBuffer::MIOBuffer(void *b, int64 bufsize, int64 aWater_mark)
{
  set(b, bufsize);
  water_mark = aWater_mark;
  size_index = BUFFER_SIZE_NOT_ALLOCATED;
#ifdef TRACK_BUFFER_USER
  _location = NULL;
#endif
  return;
}

TS_INLINE
MIOBuffer::MIOBuffer(int64 default_size_index)
{
  clear();
  size_index = default_size_index;
#ifdef TRACK_BUFFER_USER
  _location = NULL;
#endif
  return;
}

TS_INLINE
MIOBuffer::MIOBuffer()
{
  clear();
#ifdef TRACK_BUFFER_USER
  _location = NULL;
#endif
  return;
}

TS_INLINE
MIOBuffer::~
MIOBuffer()
{
  _writer = NULL;
  dealloc_all_readers();
}

TS_INLINE MIOBuffer * new_MIOBuffer_internal(
#ifdef TRACK_BUFFER_USER
                                               const char *location,
#endif
                                               int64 size_index)
{
  MIOBuffer *b = ioAllocator.alloc();
#ifdef TRACK_BUFFER_USER
  b->_location = location;
#endif
  b->alloc(size_index);
  return b;
}

TS_INLINE void
free_MIOBuffer(MIOBuffer * mio)
{
  mio->_writer = NULL;
  mio->dealloc_all_readers();
  ioAllocator.free(mio);
}

TS_INLINE MIOBuffer * new_empty_MIOBuffer_internal(
#ifdef TRACK_BUFFER_USER
                                                     const char *location,
#endif
                                                     int64 size_index)
{
  MIOBuffer *b = ioAllocator.alloc();
  b->size_index = size_index;
#ifdef TRACK_BUFFER_USER
  b->_location = location;
#endif
  return b;
}

TS_INLINE void
free_empty_MIOBuffer(MIOBuffer * mio)
{
  ioAllocator.free(mio);
}

TS_INLINE IOBufferReader *
MIOBuffer::alloc_accessor(MIOBufferAccessor * anAccessor)
{
  int i;
  for (i = 0; i < MAX_MIOBUFFER_READERS; i++)
    if (!readers[i].allocated())
      break;

  // TODO refactor code to return NULL at some point
  ink_release_assert(i < MAX_MIOBUFFER_READERS);

  IOBufferReader *e = &readers[i];
  e->mbuf = this;
  e->reset();
  e->accessor = anAccessor;

  return e;
}

TS_INLINE IOBufferReader *
MIOBuffer::alloc_reader()
{
  int i;
  for (i = 0; i < MAX_MIOBUFFER_READERS; i++)
    if (!readers[i].allocated())
      break;

  // TODO refactor code to return NULL at some point
  ink_release_assert(i < MAX_MIOBUFFER_READERS);

  IOBufferReader *e = &readers[i];
  e->mbuf = this;
  e->reset();
  e->accessor = NULL;

  return e;
}

TS_INLINE int64
MIOBuffer::block_size()
{
  return index_to_buffer_size(size_index);
}
TS_INLINE IOBufferReader *
MIOBuffer::clone_reader(IOBufferReader * r)
{
  int i;
  for (i = 0; i < MAX_MIOBUFFER_READERS; i++)
    if (!readers[i].allocated())
      break;

  // TODO refactor code to return NULL at some point
  ink_release_assert(i < MAX_MIOBUFFER_READERS);

  IOBufferReader *e = &readers[i];
  e->mbuf = this;
  e->accessor = NULL;
  e->block = r->block;
  e->start_offset = r->start_offset;
  e->size_limit = r->size_limit;
  ink_assert(e->size_limit >= 0);

  return e;
}

TS_INLINE int64
MIOBuffer::block_write_avail()
{
  IOBufferBlock *b = first_write_block();
  return b ? b->write_avail() : 0;
}

////////////////////////////////////////////////////////////////
//
//  MIOBuffer::append_block()
//
//  Appends a block to writer->next and make it the current
//  block.
//  Note that the block is not appended to the end of the list.
//  That means that if writer->next was not null before this
//  call then the block that writer->next was pointing to will
//  have its reference count decremented and writer->next
//  will have a new value which is the new block.
//  In any case the new appended block becomes the current
//  block.
//
////////////////////////////////////////////////////////////////
TS_INLINE void
MIOBuffer::append_block_internal(IOBufferBlock * b)
{
  // It would be nice to remove an empty buffer at the beginning,
  // but this breaks HTTP.
  // if (!_writer || !_writer->read_avail())
  if (!_writer) {
    _writer = b;
    init_readers();
  } else {
    ink_assert(!_writer->next || !_writer->next->read_avail());
    _writer->next = b;
    while (b->read_avail()) {
      _writer = b;
      b = b->next;
      if (!b)
        break;
    }
  }
  while (_writer->next && !_writer->write_avail() && _writer->next->read_avail())
    _writer = _writer->next;
}

TS_INLINE void
MIOBuffer::append_block(IOBufferBlock * b)
{
  ink_assert(b->read_avail());
  append_block_internal(b);
}

////////////////////////////////////////////////////////////////
//
//  MIOBuffer::append_block()
//
//  Allocate a block, appends it to current->next
//  and make the new block the current block (writer).
//
////////////////////////////////////////////////////////////////
TS_INLINE void
MIOBuffer::append_block(int64 asize_index)
{
  ink_debug_assert(BUFFER_SIZE_ALLOCATED(asize_index));
#ifdef TRACK_BUFFER_USER
  IOBufferBlock *b = new_IOBufferBlock_internal(_location);
#else
  IOBufferBlock *b = new_IOBufferBlock_internal();
#endif
  b->alloc(asize_index);
  append_block_internal(b);
  return;
}

TS_INLINE void
MIOBuffer::add_block()
{
  append_block(size_index);
}

TS_INLINE void
MIOBuffer::check_add_block()
{
  if (!high_water() && current_low_water())
    add_block();
}

TS_INLINE IOBufferBlock *
MIOBuffer::get_current_block()
{
  return first_write_block();
}

//////////////////////////////////////////////////////////////////
//
//  MIOBuffer::current_write_avail()
//
//  returns the total space available in all blocks.
//  This function is different than write_avail() because
//  it will not append a new block if there is no space
//  or below the watermark space available.
//
//////////////////////////////////////////////////////////////////
TS_INLINE int64
MIOBuffer::current_write_avail()
{
  int64 t = 0;
  IOBufferBlock *b = _writer;
  while (b) {
    t += b->write_avail();
    b = b->next;
  }
  return t;
}

//////////////////////////////////////////////////////////////////
//
//  MIOBuffer::write_avail()
//
//  returns the number of bytes available in the current block.
//  If there is no current block or not enough free space in
//  the current block then a new block is appended.
//
//////////////////////////////////////////////////////////////////
TS_INLINE int64
MIOBuffer::write_avail()
{
  check_add_block();
  return current_write_avail();
}

TS_INLINE void
MIOBuffer::fill(int64 len)
{
  int64 f = _writer->write_avail();
  while (f < len) {
    _writer->fill(f);
    len -= f;
    if (len > 0)
      _writer = _writer->next;
    f = _writer->write_avail();
  }
  _writer->fill(len);
}

TS_INLINE int
MIOBuffer::max_block_count()
{
  int maxb = 0;
  for (int i = 0; i < MAX_MIOBUFFER_READERS; i++) {
    if (readers[i].allocated()) {
      int c = readers[i].block_count();
      if (c > maxb) {
        maxb = c;
      }
    }
  }
  return maxb;
}

TS_INLINE int64
MIOBuffer::max_read_avail()
{
  int64 s = 0;
  int found = 0;
  for (int i = 0; i < MAX_MIOBUFFER_READERS; i++) {
    if (readers[i].allocated()) {
      int64 ss = readers[i].read_avail();
      if (ss > s) {
        s = ss;
      }
      found = 1;
    }
  }
  if (!found && _writer)
    return _writer->read_avail();
  return s;
}

TS_INLINE void
MIOBuffer::set(void *b, int64 len)
{
#ifdef TRACK_BUFFER_USER
  _writer = new_IOBufferBlock_internal(_location);
#else
  _writer = new_IOBufferBlock_internal();
#endif
  _writer->set_internal(b, len, BUFFER_SIZE_INDEX_FOR_CONSTANT_SIZE(len));
  init_readers();
}

TS_INLINE void
MIOBuffer::set_xmalloced(void *b, int64 len)
{
#ifdef TRACK_BUFFER_USER
  _writer = new_IOBufferBlock_internal(_location);
#else
  _writer = new_IOBufferBlock_internal();
#endif
  _writer->set_internal(b, len, BUFFER_SIZE_INDEX_FOR_XMALLOC_SIZE(len));
  init_readers();
}

TS_INLINE void
MIOBuffer::append_xmalloced(void *b, int64 len)
{
#ifdef TRACK_BUFFER_USER
  IOBufferBlock *x = new_IOBufferBlock_internal(_location);
#else
  IOBufferBlock *x = new_IOBufferBlock_internal();
#endif
  x->set_internal(b, len, BUFFER_SIZE_INDEX_FOR_XMALLOC_SIZE(len));
  append_block_internal(x);
}

TS_INLINE void
MIOBuffer::append_fast_allocated(void *b, int64 len, int64 fast_size_index)
{
#ifdef TRACK_BUFFER_USER
  IOBufferBlock *x = new_IOBufferBlock_internal(_location);
#else
  IOBufferBlock *x = new_IOBufferBlock_internal();
#endif
  x->set_internal(b, len, fast_size_index);
  append_block_internal(x);
}

TS_INLINE void
MIOBuffer::alloc(int64 i)
{
#ifdef TRACK_BUFFER_USER
  _writer = new_IOBufferBlock_internal(_location);
#else
  _writer = new_IOBufferBlock_internal();
#endif
  _writer->alloc(i);
  size_index = i;
  init_readers();
}

TS_INLINE void
MIOBuffer::alloc_xmalloc(int64 buf_size)
{
  char *b = (char *) xmalloc(buf_size);
  set_xmalloced(b, buf_size);
}

TS_INLINE void
MIOBuffer::dealloc_reader(IOBufferReader * e)
{
  if (e->accessor) {
    ink_assert(e->accessor->mbuf == this);
    ink_assert(e->accessor->entry == e);
    e->accessor->reset();
  }
  e->clear();
}

TS_INLINE IOBufferReader *
IOBufferReader::clone()
{
  return mbuf->clone_reader(this);
}

TS_INLINE void
IOBufferReader::dealloc()
{
  mbuf->dealloc_reader(this);
}

TS_INLINE void
MIOBuffer::dealloc_all_readers()
{
  for (int i = 0; i < MAX_MIOBUFFER_READERS; i++)
    if (readers[i].allocated())
      dealloc_reader(&readers[i]);
}

TS_INLINE void
MIOBuffer::set_size_index(int64 size)
{
  size_index = iobuffer_size_to_index(size);
}

TS_INLINE void
MIOBufferAccessor::reader_for(MIOBuffer * abuf)
{
  mbuf = abuf;
  if (abuf)
    entry = mbuf->alloc_accessor(this);
  else
    entry = NULL;
}

TS_INLINE void
MIOBufferAccessor::reader_for(IOBufferReader * areader)
{
  if (entry == areader)
    return;
  mbuf = areader->mbuf;
  entry = areader;
  ink_assert(mbuf);
}

TS_INLINE void
MIOBufferAccessor::writer_for(MIOBuffer * abuf)
{
  mbuf = abuf;
  entry = NULL;
}

TS_INLINE void
MIOBufferAccessor::clear()
{
  entry = NULL;
  mbuf = NULL;
}

TS_INLINE
MIOBufferAccessor::~
MIOBufferAccessor()
{
}

#endif
