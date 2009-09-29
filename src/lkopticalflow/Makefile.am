# plugindir is set in configure

noinst_LTLIBRARIES = libgstlkopticalflow.la

# sources used to compile this plug-in
libgstlkopticalflow_la_SOURCES = gstlkopticalflow.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstlkopticalflow_la_CFLAGS  = $(GST_CFLAGS) $(OPENCV_CFLAGS)
libgstlkopticalflow_la_LIBADD  = $(GST_LIBS)   $(OPENCV_LIBS)
libgstlkopticalflow_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gstlkopticalflow.h
