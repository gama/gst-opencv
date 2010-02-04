NULL =

noinst_LTLIBRARIES = libgstbgfgcodebook.la

# sources used to compile this plug-in
libgstbgfgcodebook_la_SOURCES =							\
	gstbgfgcodebook.c									\
	$(NULL)

# flags used to compile this bgfgcodebook
# add other _CFLAGS and _LIBS as needed
libgstbgfgcodebook_la_CFLAGS =							\
	-I$(top_srcdir)/src/common							\
	$(GST_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgstbgfgcodebook_la_LIBADD =							\
	$(GST_LIBS)											\
	$(OPENCV_LIBS)										\
	$(NULL)

libgstbgfgcodebook_la_LDFLAGS =							\
	$(GST_PLUGIN_LDFLAGS)
	$(NULL)

libgstbgfgcodebook_la_DEPENDENCIES =					\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gstbgfgcodebook.h									\
	$(NULL)

