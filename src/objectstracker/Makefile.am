NULL =

noinst_LTLIBRARIES = libgstobjectstracker.la

# sources used to compile this plug-in
libgstobjectstracker_la_SOURCES =						\
	gstobjectstracker.c									\
	$(NULL)

# flags used to compile this objectstracker
# add other _CFLAGS and _LIBS as needed
libgstobjectstracker_la_CFLAGS =						\
	-I$(top_srcdir)/src/common							\
	$(GST_CFLAGS)										\
	$(GNET_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgstobjectstracker_la_LIBADD =						\
	$(GST_LIBS)											\
	$(GNET_LIBS)										\
	$(OPENCV_LIBS)										\
	$(NULL)

libgstobjectstracker_la_LDFLAGS =						\
	$(GST_PLUGIN_LDFLAGS)
	$(NULL)

libgstobjectstracker_la_DEPENDENCIES =					\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gstobjectstracker.h									\
	$(NULL)

