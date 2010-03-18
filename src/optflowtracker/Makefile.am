NULL =

noinst_LTLIBRARIES = libgstoptflowtracker.la

# sources used to compile this plug-in
libgstoptflowtracker_la_SOURCES =						\
	gstoptflowtracker.c									\
	$(NULL)

# flags used to compile this optflowtracker
# add other _CFLAGS and _LIBS as needed
libgstoptflowtracker_la_CFLAGS =						\
	-I$(top_srcdir)/src/common							\
	$(GST_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgstoptflowtracker_la_LIBADD =						\
	$(GST_LIBS)											\
	$(OPENCV_LIBS)										\
	$(NULL)

libgstoptflowtracker_la_LDFLAGS =						\
	$(GST_PLUGIN_LDFLAGS)
	$(NULL)

libgstoptflowtracker_la_DEPENDENCIES =					\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gstoptflowtracker.h									\
	$(NULL)

