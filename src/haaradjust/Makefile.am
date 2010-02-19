NULL =

noinst_LTLIBRARIES = libgsthaaradjust.la

# sources used to compile this plug-in
libgsthaaradjust_la_SOURCES =						\
	gsthaaradjust.c									\
	$(NULL)

# flags used to compile this haaradjust
# add other _CFLAGS and _LIBS as needed
libgsthaaradjust_la_CFLAGS =						\
	-I$(top_srcdir)/src/common							\
	$(GST_CFLAGS)										\
	$(GNET_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgsthaaradjust_la_LIBADD =						\
	$(GST_LIBS)											\
	$(GNET_LIBS)										\
	$(OPENCV_LIBS)										\
	$(NULL)

libgsthaaradjust_la_LDFLAGS =						\
	$(GST_PLUGIN_LDFLAGS)
	$(NULL)

libgsthaaradjust_la_DEPENDENCIES =					\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gsthaaradjust.h									\
	$(NULL)

