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

AM_CPPFLAGS =   -I$(top_srcdir)/libinktomi++ \
                -I$(iocore_include_dirs) \
                -I$(top_srcdir)/librecords \
                -I$(top_srcdir)/proxy \
                -DPREFIX=\"$(prefix)\"
#                -I$(top_srcdir)/proxy/hdrs
#                -I$(top_srcdir)/proxy/logging


DEFS += @IOCORE_MODULARIZED_DEFS@

bin_PROGRAMS = tsapp

tsapp_SOURCES = app-template.cc

tsapp_LDFLAGS = @EXTRA_CXX_LDFLAGS@
tsapp_LDADD =  \
	$(top_builddir)/proxy/libTrafficServerStandalone.a \
	$(top_builddir)/proxy/hdrs/libhdrs.a \
	$(top_builddir)/iocore/cache/libinkcache.a \
	$(top_builddir)/iocore/aio/libinkaio.a \
        $(top_builddir)/iocore/hostdb/libinkhostdb.a \
        $(top_builddir)/iocore/dns/libinkdns.a \
	$(top_builddir)/iocore/net/libinknet.a \
        $(top_builddir)/iocore/utils/libinkutils.a \
        $(top_builddir)/iocore/eventsystem/libinkevent.a \
        $(top_builddir)/librecords/librecprocess.a \
        $(top_builddir)/libinktomi++/libinktomi++.a \
	  @LIBTHREAD@ @LIBSOCKET@ @LIBNSL@ @LIBRESOLV@ @LIBRT@ \
	  @LIBPCRE@ @LIBDB@ @LIBSQLITE3@ @LIBSSL@ @LIBTCL@ @LIBDL@ \
	  @LIBEXPAT@ @LIBDEMANGLE@ @LIBICONV@ \
	  @LIBMLD@ @LIBEXC@ @LIBM@ @LIBEV@ @LIBPROFILER@ @LIBEXECINFO@

EXTRA_DIST = records.config.in
sysconfdir = $(pkgsysconfdir)
nodist_sysconf_DATA = records.config

install-data-local:
	$(INSTALL) -d -o $(pkgsysuser) -g $(pkgsysgroup) $(DESTDIR)$(pkglocalstatedir) $(DESTDIR)$(pkglogdir) $(DESTDIR)$(pkgsysconfdir) $(DESTDIR)$(pkgdatadir)

install-data-hook:
	-chown -R $(pkgsysuser):$(pkgsysgroup) $(DESTDIR)$(pkgsysconfdir) $(DESTDIR)$(pkgdatadir)

install-exec-local:
	$(INSTALL) -d -o $(pkgsysuser) -g $(pkgsysgroup) $(DESTDIR)$(pkglibexecdir)

install-exec-hook:
	-chown -R $(pkgsysuser):$(pkgsysgroup) $(DESTDIR)$(pkglibexecdir)

uninstall-hook:
	-rm -rf $(DESTDIR)$(pkglocalstatedir) $(DESTDIR)$(pkglogdir) $(DESTDIR)$(pkgsysconfdir) $(DESTDIR)$(pkglibexecdir)
