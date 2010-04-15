NULL =

noinst_LTLIBRARIES = libgststaticobjects.la

# sources used to compile this plug-in
libgststaticobjects_la_SOURCES =		\
	gststaticobjects.c					\
	$(NULL)

# flags used to compile this staticobjects
# add other _CFLAGS and _LIBS as needed
libgststaticobjects_la_CFLAGS =			\
	-I$(top_srcdir)/src/common			\
	$(GST_CFLAGS)						\
	$(GNET_CFLAGS)						\
	$(OPENCV_CFLAGS)					\
	$(NULL)

libgststaticobjects_la_LIBADD =			\
	$(GST_LIBS)							\
	$(GNET_LIBS)						\
	$(OPENCV_LIBS)						\
	$(NULL)

libgststaticobjects_la_LDFLAGS =		\
	$(GST_PLUGIN_LDFLAGS)
	$(NULL)

libgststaticobjects_la_DEPENDENCIES =	\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =						\
	gststaticobjects.h					\
	$(NULL)
