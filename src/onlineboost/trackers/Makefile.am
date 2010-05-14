NULL =

noinst_PROGRAMS =						\
	Classifier                          \
	BoostingTracker						\
	SemiBoostingTracker					\
	BeyondSemiBoostingTracker			\
	$(NULL)

Classifier_SOURCES =                    \
	ClassifierApplication.cpp			\
	$(NULL)

Classifier_CFLAGS =                     \
	-I..								\
	-I../imageio						\
	$(OPENCV_CFLAGS)					\
	$(NULL)

Classifier_CXXFLAGS = $(Classifier_CFLAGS)

Classifier_LDADD =                      \
	../libonlineboost.la				\
	../imageio/libonlineboostimageio.la	\
	$(OPENCV_LIBS)						\
	$(NULL)

BoostingTracker_SOURCES =				\
	BoostingTracker.cpp					\
	BoostingApplication.cpp				\
	$(NULL)

BoostingTracker_CFLAGS =				\
	-I..								\
	-I../imageio						\
	$(OPENCV_CFLAGS)					\
	$(NULL)

BoostingTracker_CXXFLAGS = $(BoostingTracker_CFLAGS)

BoostingTracker_LDADD =					\
	../libonlineboost.la				\
	../imageio/libonlineboostimageio.la	\
	$(OPENCV_LIBS)						\
	$(NULL)

SemiBoostingTracker_SOURCES =			\
	SemiBoostingTracker.cpp				\
	SemiBoostingApplication.cpp			\
	$(NULL)

SemiBoostingTracker_CFLAGS =			\
	-I..								\
	-I../imageio						\
	$(OPENCV_CFLAGS)					\
	$(NULL)

SemiBoostingTracker_CXXFLAGS = $(SemiBoostingTracker_CFLAGS)

SemiBoostingTracker_LDADD =				\
	../libonlineboost.la				\
	../imageio/libonlineboostimageio.la	\
	$(OPENCV_LIBS)						\
	$(NULL)

BeyondSemiBoostingTracker_SOURCES =		\
	BeyondSemiBoostingTracker.cpp		\
	BeyondSemiBoostingApplication.cpp	\
	$(NULL)

BeyondSemiBoostingTracker_CFLAGS =		\
	-I..								\
	-I../imageio						\
	$(OPENCV_CFLAGS)					\
	$(NULL)

BeyondSemiBoostingTracker_CXXFLAGS = $(BeyondSemiBoostingTracker_CFLAGS)

BeyondSemiBoostingTracker_LDADD =		\
	../libonlineboost.la				\
	../imageio/libonlineboostimageio.la	\
	$(OPENCV_LIBS)						\
	$(NULL)

noinst_HEADERS =						\
	BoostingTracker.h					\
	SemiBoostingTracker.h				\
	BeyondSemiBoostingTracker.h			\
	$(NULL)

