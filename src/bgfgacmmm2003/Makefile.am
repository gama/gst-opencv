NULL =

noinst_LTLIBRARIES = libgstbgfgacmmm2003.la

# sources used to compile this plug-in
libgstbgfgacmmm2003_la_SOURCES =							\
	gstbgfgacmmm2003.c									\
	$(NULL)

# flags used to compile this bgfgacmmm2003
# add other _CFLAGS and _LIBS as needed
libgstbgfgacmmm2003_la_CFLAGS =							\
	-I$(top_srcdir)/src/common							\
	$(GST_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgstbgfgacmmm2003_la_LIBADD =							\
	$(GST_LIBS)											\
	$(OPENCV_LIBS)										\
	$(NULL)

libgstbgfgacmmm2003_la_LDFLAGS =							\
	$(GST_PLUGIN_LDFLAGS)
	$(NULL)

libgstbgfgacmmm2003_la_DEPENDENCIES =					\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gstbgfgacmmm2003.h									\
	$(NULL)

