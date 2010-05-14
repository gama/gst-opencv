NULL =

SUBDIRS =										\
	$(srcdir)									\
	imageio										\
	trackers									\
	$(NULL)

noinst_LTLIBRARIES = libonlineboost.la

# sources used to compile this directory
libonlineboost_la_SOURCES =						\
	BaseClassifier.cpp							\
	Classifier.cpp                              \
	ClassifierIntermediate.cpp					\
	ClassifierThreshold.cpp						\
	Detector.cpp								\
	EstimatedGaussDistribution.cpp				\
	FeatureHaar.cpp								\
	ImageRepresentation.cpp						\
	Patches.cpp									\
	Regions.cpp									\
	StrongClassifier.cpp						\
	StrongClassifierDirectSelection.cpp			\
	StrongClassifierStandard.cpp				\
	StrongClassifierStandardSemi.cpp			\
	WeakClassifier.cpp							\
	WeakClassifierHaarFeature.cpp				\
	$(NULL)

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libonlineboost_la_CFLAGS  =						\
	$(OPENCV_CFLAGS)							\
	$(NULL)

libonlineboost_la_CXXFLAGS = $(libonlineboost_la_CFLAGS)

libonlineboost_la_LIBADD  =						\
	$(OPENCV_LIBS)								\
	$(NULL)

libonlineboost_la_LDFLAGS =						\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =								\
	BaseClassifier.h							\
	Classifier.h                                \
	ClassifierThreshold.h						\
	Detector.h									\
	EstimatedGaussDistribution.h				\
	FeatureHaar.h								\
	ImageRepresentation.h						\
	Patches.h									\
	OS_specific.h								\
	Regions.h									\
	StrongClassifier.h							\
	StrongClassifierDirectSelection.h			\
	StrongClassifierStandard.h					\
	StrongClassifierStandardSemi.h				\
	WeakClassifier.h							\
	WeakClassifierHaarFeature.h					\
	$(NULL)

