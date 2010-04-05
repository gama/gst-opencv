NULL =

noinst_LTLIBRARIES = libgstobjectsareainteraction.la

# sources used to compile this plug-in
libgstobjectsareainteraction_la_SOURCES =               \
	gstobjectsareainteraction.c							\
	$(NULL)

# flags used to compile this objectsareainteraction
# add other _CFLAGS and _LIBS as needed
libgstobjectsareainteraction_la_CFLAGS =				\
	-I$(top_srcdir)/src/common							\
	$(GST_CFLAGS)										\
	$(GNET_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgstobjectsareainteraction_la_LIBADD =				\
	$(GST_LIBS)											\
	$(GNET_LIBS)										\
	$(OPENCV_LIBS)										\
	$(NULL)

libgstobjectsareainteraction_la_LDFLAGS =				\
	$(GST_PLUGIN_LDFLAGS)
	$(NULL)

libgstobjectsareainteraction_la_DEPENDENCIES =			\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gstobjectsareainteraction.h							\
	$(NULL)
