# plugindir is set in configure

noinst_LTLIBRARIES = libgsthaardetect.la

# sources used to compile this plug-in
libgsthaardetect_la_SOURCES = gsthaardetect.c

# flags used to compile this haardetect
# add other _CFLAGS and _LIBS as needed
libgsthaardetect_la_CFLAGS = $(GST_CFLAGS) $(OPENCV_CFLAGS)
libgsthaardetect_la_LIBADD = $(GST_LIBS) $(OPENCV_LIBS)
libgsthaardetect_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gsthaardetect.h
