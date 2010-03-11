/*
  *Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 */

#ifndef __GST_OPENCV_COMMON_SURF__
#define __GST_OPENCV_COMMON_SURF__

#include <glib.h>
#include <cv.h>

typedef struct _IntPair IntPair;

struct _IntPair{
    int a;
    int b;
};

double   compareSURFDescriptors (const float  *d1,
                                 const float  *d2,
                                 double        best,
                                 int           length);

int      naiveNearestNeighbor   (const float  *vec,
                                 int           laplacian,
                                 const CvSeq  *model_keypoints,
                                 const CvSeq  *model_descriptors);

void     findPairs              (const CvSeq  *objectKeypoints,
                                 const CvSeq  *objectDescriptors,
                                 const CvSeq  *imageKeypoints,
                                 const CvSeq  *imageDescriptors,
                                 GArray       *pairs);

CvSeq    *getMatchPoints        (const CvSeq  *src,
                                 const GArray *pairs,
                                 const int     PAIR_A_0__PAIR_B_1,
                                 CvMemStorage *mem_storage);

CvPoint  surfCentroid           (const CvSeq  *seq,
                                 CvPoint       pt_displacement);

void     drawSurfPoints         (const CvSeq  *seq,
                                 CvPoint       pt_displacement,
                                 IplImage     *img,
                                 CvScalar      color,
                                 int           NOMARK0_MARK1);

CvRect   rectDisplacement       (const CvSeq  *objectKeypoints,
                                 const CvSeq  *imageKeypoints,
                                 const GArray *pairs,
                                 const CvRect  objectRect,
                                 const float   pairs_perc_considerate);

CvRect   surfPointsBoundingRect (const CvSeq  *seq);

#endif
