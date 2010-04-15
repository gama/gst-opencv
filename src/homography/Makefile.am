NULL =

noinst_LTLIBRARIES = libgsthomography.la

# sources used to compile this plug-in
libgsthomography_la_SOURCES =							\
	gsthomography.c										\
	$(NULL)

# flags used to compile this homography
# add other _CFLAGS and _LIBS as needed
libgsthomography_la_CFLAGS =							\
	-I$(top_srcdir)/src/common							\
	$(GST_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgsthomography_la_LIBADD =							\
	$(GST_LIBS)											\
	$(OPENCV_LIBS)										\
	$(NULL)

libgsthomography_la_LDFLAGS =							\
	$(GST_PLUGIN_LDFLAGS)								\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gsthomography.h										\
	$(NULL)

