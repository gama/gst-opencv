NULL =

noinst_LTLIBRARIES = libgstcommon.la

# sources used to compile this plug-in
libgstcommon_la_SOURCES =								\
	condensation.c										\
	identifier_motion.c									\
	$(NULL)

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstcommon_la_CFLAGS  = 								\
	$(GST_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgstcommon_la_LIBADD  = 								\
	$(GST_LIBS)											\
	$(OPENCV_LIBS)										\
	$(NULL)

libgstcommon_la_LDFLAGS =								\
	$(GST_PLUGIN_LDFLAGS)								\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS = 										\
	condensation.h										\
	identifier_motion.h									\
	$(NULL)
