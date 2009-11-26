/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 * Copyright (C) 2009 Erickson Nascimento <erickson@vettalabs.com>
 */

#ifndef __GST_OPENCV_FACEMETRIX_KMEANS__
#define __GST_OPENCV_FACEMETRIX_KMEANS__

#include <cv.h>

CvRect rectBoudingIdx      (CvPoint2D32f *measurement,
                            int           size,
                            int           idx,
                            int          *vetPointsClusterIdx);

CvRect floatRectBoudingIdx (float **measurement,
                            int     size,
                            int     idx,
                            int    *vetPointsClusterIdx);

void   doKmeans            (int           nClusters,
                            int           nPoints,
                            CvPoint2D32f *vetPoints,
                            int          *vetPointsClusterIdx);

void   floatDoKmeans       (int     nClusters,
                            int     nPoints,
                            float **vetPoints,
                            int    *vetPointsClusterIdx);

#endif // __GST_OPENCV_FACEMETRIX_KMEANS__
