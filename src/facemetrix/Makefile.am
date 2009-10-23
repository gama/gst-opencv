# plugindir is set in configure

noinst_LTLIBRARIES = libgstfacemetrix.la

# sources used to compile this plug-in
libgstfacemetrix_la_SOURCES = gstfacemetrix.c sglclient.c

# flags used to compile this facemetrix
# add other _CFLAGS and _LIBS as needed
libgstfacemetrix_la_CFLAGS = $(GST_CFLAGS) $(GNET_CFLAGS) $(OPENCV_CFLAGS)
libgstfacemetrix_la_LIBADD = $(GST_LIBS) $(GNET_LIBS) $(OPENCV_LIBS)
libgstfacemetrix_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gstfacemetrix.h sglclient.h
