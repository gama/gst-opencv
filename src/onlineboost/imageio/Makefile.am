NULL =

noinst_LTLIBRARIES = libonlineboostimageio.la

# sources used to compile this directory
libonlineboostimageio_la_SOURCES =				\
	ImageHandler.cpp							\
	ImageSource.cpp								\
	ImageSourceAVIFile.cpp						\
	ImageSourceDir.cpp							\
	ImageSourceUSBCam.cpp						\
	$(NULL)

libonlineboostimageio_la_CFLAGS  =				\
	-I..										\
	$(OPENCV_CFLAGS)							\
	$(NULL)

libonlineboostimageio_la_CXXFLAGS = $(libonlineboostimageio_la_CFLAGS)

libonlineboostimageio_la_LIBADD  =				\
	$(OPENCV_LIBS)								\
	$(NULL)

libonlineboostimageio_la_LDFLAGS =				\
	$(NULL)

# headers we need but don't want installed
noinst_HEADERS =								\
	ImageHandler.h								\
	ImageSource.h								\
	ImageSourceAny.h							\
	ImageSourceAVIFile.h						\
	ImageSourceDir.h							\
	ImageSourceUSBCam.h							\
	$(NULL)

