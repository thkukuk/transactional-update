AUTOMAKE_OPTIONS = subdir-objects
lib_LTLIBRARIES = libtukit.la
libtukit_la_SOURCES=Transaction.cpp \
        Snapshot.cpp Snapshot/Snapper.cpp \
        Mount.cpp Overlay.cpp Configuration.cpp \
        Util.cpp Supplement.cpp
publicheadersdir=$(includedir)/tukit
publicheaders_HEADERS=Transaction.hpp
noinst_HEADERS=Snapshot.hpp Snapshot/Snapper.hpp \
        Mount.hpp Overlay.hpp Log.hpp Configuration.hpp \
        Util.hpp Supplement.hpp Exceptions.hpp
libtukit_la_CPPFLAGS=$(ECONF_CFLAGS) $(LIBMOUNT_CFLAGS) $(SELINUX_CFLAGS)
libtukit_la_LDFLAGS=$(ECONF_LIBS) $(LIBMOUNT_LIBS) $(SELINUX_LIBS) \
	-version-info $(LIBTOOL_CURRENT):$(LIBTOOL_REVISION):$(LIBTOOL_AGE)
