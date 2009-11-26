NULL =

noinst_LTLIBRARIES = libgstfacemetrix.la

# sources used to compile this plug-in
libgstfacemetrix_la_SOURCES =							\
	gstfacemetrix.c										\
	sglclient.c											\
	draw.c												\
	kmeans.c											\
	$(NULL)

# flags used to compile this facemetrix
# add other _CFLAGS and _LIBS as needed
libgstfacemetrix_la_CFLAGS =							\
	-I$(top_srcdir)/src/common							\
	$(GST_CFLAGS)										\
	$(GNET_CFLAGS)										\
	$(OPENCV_CFLAGS)									\
	$(NULL)

libgstfacemetrix_la_LIBADD =							\
	$(GST_LIBS)											\
	$(GNET_LIBS)										\
	$(OPENCV_LIBS)										\
	$(NULL)

libgstfacemetrix_la_LDFLAGS =							\
	$(GST_PLUGIN_LDFLAGS)
	$(NULL)

libgstfacemetrix_la_DEPENDENCIES =						\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =										\
	gstfacemetrix.h										\
	sglclient.h											\
	draw.h												\
	kmeans.h											\
	$(NULL)

