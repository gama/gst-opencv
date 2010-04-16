NULL =

noinst_LTLIBRARIES = libgstobjectdistances.la

# sources used to compile this plug-in
libgstobjectdistances_la_SOURCES =		\
	gstobjectdistances.c				\
	$(NULL)

# flags used to compile this objectdistances
# add other _CFLAGS and _LIBS as needed
libgstobjectdistances_la_CFLAGS =		\
	-I$(top_srcdir)/src/common			\
	$(GST_CFLAGS)						\
	$(GNET_CFLAGS)						\
	$(OPENCV_CFLAGS)					\
	$(NULL)

libgstobjectdistances_la_LIBADD =		\
	$(GST_LIBS)							\
	$(GNET_LIBS)						\
	$(OPENCV_LIBS)						\
	$(NULL)

libgstobjectdistances_la_LDFLAGS =		\
	$(GST_PLUGIN_LDFLAGS)
	$(NULL)

libgstobjectdistances_la_DEPENDENCIES =	\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =						\
	gstobjectdistances.h				\
	$(NULL)
