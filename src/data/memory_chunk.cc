// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2005-2011, Jari Sundell <jaris@ifi.uio.no>

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "data/memory_chunk.h"
#include "torrent/exceptions.h"
#include "torrent/utils/error_number.h"

#ifdef __sun__
extern "C" int
madvise(void*, size_t, int);
//#include <sys/mman.h>
// Should be the include line instead, but Solaris
// has an annoying bug wherein it doesn't declare
// madvise for C++.
#endif

namespace torrent {

uint32_t MemoryChunk::m_pagesize = getpagesize();

const int MemoryChunk::sync_sync;
const int MemoryChunk::sync_async;
const int MemoryChunk::sync_invalidate;

const int MemoryChunk::prot_exec;
const int MemoryChunk::prot_read;
const int MemoryChunk::prot_write;
const int MemoryChunk::prot_none;
const int MemoryChunk::map_shared;

const int MemoryChunk::advice_normal;
const int MemoryChunk::advice_random;
const int MemoryChunk::advice_sequential;
const int MemoryChunk::advice_willneed;
const int MemoryChunk::advice_dontneed;

inline void
MemoryChunk::align_pair(uint32_t* offset, uint32_t* length) const {
  *offset += page_align();

  *length += *offset % m_pagesize;
  *offset -= *offset % m_pagesize;
}

MemoryChunk::MemoryChunk(char* ptr, char* begin, char* end, int prot, int flags)
  : m_ptr(ptr)
  , m_begin(begin)
  , m_end(end)
  , m_prot(prot)
  , m_flags(flags) {

  if (ptr == nullptr)
    throw internal_error(
      "MemoryChunk::MemoryChunk(...) received ptr == nullptr");

  if (page_align() >= m_pagesize)
    throw internal_error("MemoryChunk::MemoryChunk(...) received an page "
                         "alignment >= page size");

  if ((std::ptrdiff_t)ptr % m_pagesize)
    throw internal_error(
      "MemoryChunk::MemoryChunk(...) is not aligned to a page");
}

void
MemoryChunk::unmap() {
  if (!is_valid())
    throw internal_error("MemoryChunk::unmap() called on an invalid object");

  if (munmap(m_ptr, m_end - m_ptr) != 0)
    throw internal_error("MemoryChunk::unmap() system call failed: " +
                         utils::error_number::current().message());
}

void
MemoryChunk::incore(char* buf, uint32_t offset, uint32_t length) {
  if (!is_valid())
    throw internal_error(
      "Called MemoryChunk::incore(...) on an invalid object");

  if (!is_valid_range(offset, length))
    throw internal_error(
      "MemoryChunk::incore(...) received out-of-range input");

  align_pair(&offset, &length);

#if LT_USE_MINCORE

#if LT_USE_MINCORE_UNSIGNED
  if (mincore(m_ptr + offset, length, (unsigned char*)buf))
#else
  if (mincore(m_ptr + offset, length, (char*)buf))
#endif
    throw storage_error("System call mincore failed: " +
                        utils::error_number::current().message());

#else // !LT_USE_MINCORE
  // Pretend all pages are in memory.
  memset(buf, 1, pages_touched(offset, length));

#endif
}

bool
MemoryChunk::advise(uint32_t offset, uint32_t length, int advice) {
  if (!is_valid())
    throw internal_error("Called MemoryChunk::advise() on an invalid object");

  if (!is_valid_range(offset, length))
    throw internal_error(
      "MemoryChunk::advise(...) received out-of-range input");

#if LT_USE_MADVISE
  align_pair(&offset, &length);

  if (madvise(m_ptr + offset, length, advice) == 0)
    return true;

  else if (errno == EINVAL || (errno == ENOMEM && advice != advice_willneed) ||
           errno == EBADF)
    throw internal_error("MemoryChunk::advise(...) " +
                         utils::error_number::current().message());

  else
    return false;

#else
  return true;

#endif
}

bool
MemoryChunk::sync(uint32_t offset, uint32_t length, int flags) {
  if (!is_valid())
    throw internal_error("Called MemoryChunk::sync() on an invalid object");

  if (!is_valid_range(offset, length))
    throw internal_error("MemoryChunk::sync(...) received out-of-range input");

  align_pair(&offset, &length);

  return msync(m_ptr + offset, length, flags) == 0;
}

} // namespace torrent
