NULL =

noinst_LTLIBRARIES = libgstobjectsinteraction.la

# sources used to compile this plug-in
libgstobjectsinteraction_la_SOURCES =                   \
	gstobjectsinteraction.c								\
	$(NULL)

# flags used to compile this objectsinteraction
# add other _CFLAGS and _LIBS as needed
libgstobjectsinteraction_la_CFLAGS =					\
	-I$(top_srcdir)/src/common							\
	$(GST_CFLAGS)										\
	$(GNET_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgstobjectsinteraction_la_LIBADD =					\
	$(GST_LIBS)											\
	$(GNET_LIBS)										\
	$(OPENCV_LIBS)										\
	$(NULL)

libgstobjectsinteraction_la_LDFLAGS =					\
	$(GST_PLUGIN_LDFLAGS)
	$(NULL)

libgstobjectsinteraction_la_DEPENDENCIES =				\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gstobjectsinteraction.h								\
	$(NULL)
