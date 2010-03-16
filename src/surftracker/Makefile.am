NULL =

noinst_LTLIBRARIES = libgstsurftracker.la

# sources used to compile this plug-in
libgstsurftracker_la_SOURCES =							\
	gstsurftracker.c									\
	$(NULL)

# flags used to compile this surftracker
# add other _CFLAGS and _LIBS as needed
libgstsurftracker_la_CFLAGS =							\
	-I$(top_srcdir)/src/common							\
	$(GST_CFLAGS)										\
	$(GNET_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgstsurftracker_la_LIBADD =							\
	$(GST_LIBS)											\
	$(GNET_LIBS)										\
	$(OPENCV_LIBS)										\
	$(NULL)

libgstsurftracker_la_LDFLAGS =							\
	$(GST_PLUGIN_LDFLAGS)
	$(NULL)

libgstsurftracker_la_DEPENDENCIES =						\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gstsurftracker.h									\
	$(NULL)

