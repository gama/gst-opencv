NULL =

noinst_LTLIBRARIES = libgstinterpreterinteraction.la

# sources used to compile this plug-in
libgstinterpreterinteraction_la_SOURCES =						\
	gstinterpreterinteraction.c									\
	$(NULL)

# flags used to compile this haaradjust
# add other _CFLAGS and _LIBS as needed
libgstinterpreterinteraction_la_CFLAGS =						\
	-I$(top_srcdir)/src/common							\
	$(GST_CFLAGS)										\
	$(GNET_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgstinterpreterinteraction_la_LIBADD =						\
	$(GST_LIBS)											\
	$(GNET_LIBS)										\
	$(OPENCV_LIBS)										\
	$(NULL)

libgstinterpreterinteraction_la_LDFLAGS =						\
	$(GST_PLUGIN_LDFLAGS)
	$(NULL)

libgstinterpreterinteraction_la_DEPENDENCIES =					\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gstinterpreterinteraction.h									\
	$(NULL)

