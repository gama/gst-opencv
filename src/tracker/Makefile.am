# plugindir is set in configure

noinst_LTLIBRARIES = libgsttracker.la

# sources used to compile this plug-in
libgsttracker_la_SOURCES = gsttracker.c condensation.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgsttracker_la_CFLAGS  = $(GST_CFLAGS) $(OPENCV_CFLAGS)
libgsttracker_la_LIBADD  = $(GST_LIBS)   $(OPENCV_LIBS)
libgsttracker_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gsttracker.h condensation.h
