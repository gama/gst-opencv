NULL =

SUBDIRS =																			\
	bgfgacmmm2003																	\
	bgfgcodebook																	\
	common																			\
	edgedetect																		\
	faceblur																		\
	facemetrix																		\
	haaradjust																		\
	haardetect																		\
	motiontemplate																	\
	lkopticalflow																	\
	objectsareainteraction															\
	interpreterinteraction															\
	objectsinteraction																\
	optflowtracker																	\
	pyramidsegment																	\
	surftracker																		\
	templatematch																	\
	tracker																			\
	$(NULL)

plugin_LTLIBRARIES = libgstopencv.la

# sources used to compile this plug-in
libgstopencv_la_SOURCES = gstopencv.c

# flags used to compile this plug-in
# add other _CFLAGS and _LIBS as needed
libgstopencv_la_CFLAGS =															\
	-I${top_srcdir}/src/bgfgacmmm2003												\
	-I${top_srcdir}/src/bgfgcodebook												\
	-I${top_srcdir}/src/common														\
	-I${top_srcdir}/src/edgedetect													\
	-I${top_srcdir}/src/faceblur													\
	-I${top_srcdir}/src/facemetrix													\
	-I${top_srcdir}/src/haaradjust													\
	-I${top_srcdir}/src/haardetect													\
	-I${top_srcdir}/src/motiontemplate												\
	-I${top_srcdir}/src/lkopticalflow												\
	-I${top_srcdir}/src/objectsareainteraction										\
	-I${top_srcdir}/src/interpreterinteraction										\
	-I${top_srcdir}/src/objectsinteraction											\
	-I${top_srcdir}/src/optflowtracker												\
	-I${top_srcdir}/src/pyramidsegment												\
	-I${top_srcdir}/src/surftracker													\
	-I${top_srcdir}/src/templatematch												\
	-I${top_srcdir}/src/tracker														\
	$(GST_CFLAGS)																	\
	$(OPENCV_CFLAGS)																\
	$(NULL)


libgstopencv_la_LIBADD =															\
	$(top_builddir)/src/bgfgacmmm2003/libgstbgfgacmmm2003.la						\
	$(top_builddir)/src/bgfgcodebook/libgstbgfgcodebook.la							\
	$(top_builddir)/src/common/libgstcommon.la										\
	$(top_builddir)/src/edgedetect/libgstedgedetect.la								\
	$(top_builddir)/src/faceblur/libgstfaceblur.la									\
	$(top_builddir)/src/facemetrix/libgstfacemetrix.la								\
	$(top_builddir)/src/haaradjust/libgsthaaradjust.la								\
	$(top_builddir)/src/haardetect/libgsthaardetect.la								\
	$(top_builddir)/src/motiontemplate/libgstmotiontemplate.la						\
	$(top_builddir)/src/lkopticalflow/libgstlkopticalflow.la						\
	$(top_builddir)/src/objectsareainteraction/libgstobjectsareainteraction.la		\
	$(top_builddir)/src/interpreterinteraction/libgstinterpreterinteraction.la		\
	$(top_builddir)/src/objectsinteraction/libgstobjectsinteraction.la				\
	$(top_builddir)/src/optflowtracker/libgstoptflowtracker.la						\
	$(top_builddir)/src/pyramidsegment/libgstpyramidsegment.la						\
	$(top_builddir)/src/surftracker/libgstsurftracker.la							\
	$(top_builddir)/src/templatematch/libgsttemplatematch.la						\
	$(top_builddir)/src/tracker/libgsttracker.la									\
	$(GST_LIBS)																		\
	$(OPENCV_LIBS)																	\
	$(NULL)

libgstopencv_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

libgstopencv_la_DEPENDENCIES =														\
	$(top_builddir)/src/bgfgacmmm2003/libgstbgfgacmmm2003.la						\
	$(top_builddir)/src/bgfgcodebook/libgstbgfgcodebook.la							\
	$(top_builddir)/src/common/libgstcommon.la										\
	$(top_builddir)/src/edgedetect/libgstedgedetect.la								\
	$(top_builddir)/src/faceblur/libgstfaceblur.la									\
	$(top_builddir)/src/facemetrix/libgstfacemetrix.la								\
	$(top_builddir)/src/haaradjust/libgsthaaradjust.la								\
	$(top_builddir)/src/haardetect/libgsthaardetect.la								\
	$(top_builddir)/src/motiontemplate/libgstmotiontemplate.la						\
	$(top_builddir)/src/lkopticalflow/libgstlkopticalflow.la						\
	$(top_builddir)/src/objectsareainteraction/libgstobjectsareainteraction.la		\
	$(top_builddir)/src/interpreterinteraction/libgstinterpreterinteraction.la		\
	$(top_builddir)/src/objectsinteraction/libgstobjectsinteraction.la				\
	$(top_builddir)/src/optflowtracker/libgstoptflowtracker.la						\
	$(top_builddir)/src/pyramidsegment/libgstpyramidsegment.la						\
	$(top_builddir)/src/surftracker/libgstsurftracker.la							\
	$(top_builddir)/src/templatematch/libgsttemplatematch.la						\
	$(top_builddir)/src/tracker/libgsttracker.la									\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS = 
