/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 * Copyright (C) 2009 Erickson Nascimento <erickson@vettalabs.com>
 */

#ifndef __GST_OPENCV_COMMON_IDENTIFIER_MOTION__
#define __GST_OPENCV_COMMON_IDENTIFIER_MOTION__

#include <cv.h>

CvRect motion_detect      (IplImage *img,
                           IplImage *motionHist);

CvSeq* motion_detect_mult (IplImage *img,
                           IplImage *motionHist);

#endif // __GST_OPENCV_COMMON_IDENTIFIER_MOTION__
