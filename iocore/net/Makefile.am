# Makefile.am for the traffic/iocore/net hierarchy
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
  -I$(top_srcdir)/libinktomi++ \
  -I$(top_srcdir)/librecords 
else
AM_CPPFLAGS = \
  $(iocore_include_dirs) \
  -I$(top_srcdir)/libinktomi++ \
  -I$(top_srcdir)/librecords \
  -I$(top_srcdir)/proxy \
  -I$(top_srcdir)/proxy/mgmt2/preparse \
  -I$(top_srcdir)/proxy/hdrs \
  -I$(top_srcdir)/proxy/mgmt2 \
  -I$(top_srcdir)/proxy/mgmt2/utils
endif


DEFS += @IOCORE_MODULARIZED_DEFS@

noinst_LIBRARIES = libinknet.a

libinknet_a_SOURCES = \
  Connection.cc \
  DetailedLog.cc \
  DetailedLog.h \
  I_NetConfig.h \
  I_NetErrno.h \
  I_Net.h \
  I_NetProcessor.h \
  I_NetVConnection.h \
  I_Socks.h \
  I_UDPConnection.h \
  I_UDPNet.h \
  I_UDPPacket.h \
  Net.cc \
  NetConfig.cc \
  NetVConnection.cc \
  NetVCTest.cc \
  P_CompletionUtil.h \
  P_Connection.h \
  P_InkBulkIO.h \
  P_LibBulkIO.h \
  P_NetAccept.h \
  P_Net.h \
  P_NetVConnection.h \
  P_NetVCTest.h \
  P_Socks.h \
  P_SSLCertLookup.h \
  P_SSLConfig.h \
  P_SSLNetAccept.h \
  P_SSLNetProcessor.h \
  P_SSLNetVConnection.h \
  P_UDPConnection.h \
  P_UDPIOEvent.h \
  P_UDPNet.h \
  P_UDPPacket.h \
  P_UnixCompletionUtil.h \
  P_UnixNet.h \
  P_UnixNetProcessor.h \
  P_UnixNetState.h \
  P_UnixNetVConnection.h \
  P_UnixPollDescriptor.h \
  P_UnixUDPConnection.h \
  Socks.cc \
  SSLCertLookup.cc \
  SSLConfig.cc \
  SSLNet.cc \
  SSLNetVConnection.cc \
  SSLUnixNet.cc \
  UDPIOEvent.cc \
  UnixConnection.cc \
  UnixNetAccept.cc \
  UnixNet.cc \
  UnixNetPages.cc \
  UnixNetProcessor.cc \
  UnixNetVConnection.cc \
  UnixUDPConnection.cc \
  UnixUDPNet.cc \
  Inline.cc

