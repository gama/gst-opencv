NULL =

noinst_LTLIBRARIES = libgstmotiontemplate.la

# sources used to compile this plug-in
libgstmotiontemplate_la_SOURCES =						\
	gstmotiontemplate.c									\
	$(NULL)

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstmotiontemplate_la_CFLAGS =						\
	$(GST_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgstmotiontemplate_la_LIBADD =						\
	$(GST_LIBS)											\
	$(OPENCV_LIBS)										\
	$(NULL)

libgstmotiontemplate_la_LDFLAGS =						\
	$(GST_PLUGIN_LDFLAGS)								\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gstmotiontemplate.h									\
	$(NULL)

