NULL =

noinst_LTLIBRARIES = libgsttracker.la

# sources used to compile this plug-in
libgsttracker_la_SOURCES =								\
	gsttracker.c										\
	identifier.c										\
	tracker.c											\
	$(NULL)

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgsttracker_la_CFLAGS =								\
	-I$(top_srcdir)/src/common							\
	-I$(top_srcdir)/src/onlineboost						\
	$(GST_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgsttracker_la_LIBADD =								\
	$(GST_LIBS)											\
	$(OPENCV_LIBS)										\
	$(NULL)

libgsttracker_la_LDFLAGS =								\
	$(GST_PLUGIN_LDFLAGS)								\
	$(NULL)

libgsttracker_la_DEPENDENCIES =							\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gsttracker.h										\
	identifier.h										\
	tracker.h											\
	$(NULL)

