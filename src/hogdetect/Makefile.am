NULL =

noinst_LTLIBRARIES = libgsthogdetect.la

# sources used to compile this plug-in
libgsthogdetect_la_SOURCES =				\
	gsthogdetect.c							\
	$(NULL)

# add other _CFLAGS and _LIBS as needed
libgsthogdetect_la_CFLAGS =					\
	-I$(top_srcdir)/src/common				\
	$(GST_CFLAGS)							\
	$(OPENCV_CFLAGS)						\
	$(NULL)

libgsthogdetect_la_LIBADD =					\
	$(GST_LIBS)								\
	$(OPENCV_LIBS)							\
	$(NULL)

libgsthogdetect_la_LDFLAGS =				\
	$(GST_PLUGIN_LDFLAGS)					\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =							\
	gsthogdetect.h							\
	$(NULL)
