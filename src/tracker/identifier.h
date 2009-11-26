/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 * Copyright (C) 2009 Erickson Nascimento <erickson@vettalabs.com>
 */

#ifndef __GST_OPENCV_TRACKER_IDENTIFIER___
#define __GST_OPENCV_TRACKER_IDENTIFIER___

#include <cv.h>
#include <cvaux.h>

CvRect segObjectBookBGDiff (CvBGCodeBookModel *model,
                            IplImage          *rawImage,
                            IplImage          *yuvImage);

void   showBorder          (IplImage *srcGray,
                            IplImage *dst,
                            CvScalar  borderColor,
                            int       edge_thresh,
                            int       smooth,
                            int       borderIncreaseSize);

#endif // __GST_OPENCV_TRACKER_IDENTIFIER___
