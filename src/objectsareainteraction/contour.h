/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 */

#ifndef __GST_OPENCV_OBJECTSAREAINTERACTION_CONTOUR___
#define __GST_OPENCV_OBJECTSAREAINTERACTION_CONTOUR___

#include <cv.h>
#include <cvaux.h>
#include <draw.h>

typedef struct _InstanceObjectAreaContour InstanceObjectAreaContour;
typedef struct _InstanceObjectAreaContourResult InstanceObjectAreaContourResult;

struct _InstanceObjectAreaContour
{
    gint             id;
    CvSeq           *contour;
    CvMemStorage    *mem_storage;
};

struct _InstanceObjectAreaContourResult
{
    gint             id_a;
    gint             id_b;
    gdouble          perc_a;
    gdouble          perc_b;
    CvSeq           *contour;
    CvMemStorage    *mem_storage;
};

void    makeContour         (const gchar *str,
                             CvSeq **seq,
                             CvMemStorage* storage);

void    calcInterception    (const InstanceObjectAreaContour *a,
                             const InstanceObjectAreaContour *b,
                             InstanceObjectAreaContourResult *dst);

#endif // __GST_OPENCV_OBJECTSAREAINTERACTION_CONTOUR___
