# Makefile.am for the traffic/iocore/cache hierarchy
#
#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

if STANDALONE_IOCORE
AM_CPPFLAGS = \
  $(iocore_include_dirs) \
  -I$(top_srcdir)/librecords \
  -I$(top_srcdir)/libinktomi++ \
  -DSYSCONFDIR=\"$(sysconfdir)\"

ADD_SRC = 
else
AM_CPPFLAGS = \
  $(iocore_include_dirs) \
  -I$(top_srcdir)/librecords \
  -I$(top_srcdir)/libinktomi++ \
  -I$(top_srcdir)/proxy \
  -I$(top_srcdir)/proxy/hdrs \
  -I$(top_srcdir)/proxy/http2 \
  -I$(top_srcdir)/proxy/http2/remap \
  -I$(top_srcdir)/proxy/mgmt2 \
  -I$(top_srcdir)/proxy/mgmt2/preparse \
  -I$(top_srcdir)/proxy/mgmt2/utils \
  -I$(top_srcdir)/proxy/mgmt2/web2 \
  -DSYSCONFDIR=\"$(sysconfdir)\" 

ADD_SRC = CacheTest.cc P_CacheTest.h NewCacheVC.cc NewCacheVC.h
endif

DEFS += @IOCORE_MODULARIZED_DEFS@

noinst_LIBRARIES = libinkcache.a

libinkcache_a_SOURCES = \
  Cache.cc \
  CacheDir.cc \
  CacheDisk.cc \
  CacheHosting.cc \
  CacheHttp.cc \
  CacheLink.cc \
  CachePages.cc \
  CachePagesInternal.cc \
  CachePart.cc \
  CacheRead.cc \
  CacheWrite.cc \
  I_Cache.h \
  I_CacheDefs.h \
  I_Store.h \
  P_Cache.h \
  P_CacheArray.h \
  P_CacheDir.h \
  P_CacheDisk.h \
  P_CacheHosting.h \
  P_CacheHttp.h \
  P_CacheInternal.h \
  P_CachePart.h \
  P_RamCache.h \
  RamCacheLRU.cc \
  RamCacheCLFUS.cc \
  Store.cc \
  Inline.cc $(ADD_SRC)


