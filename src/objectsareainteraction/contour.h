/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 */

#ifndef __GST_OPENCV_OBJECTS_AREA_INTERACTION_CONTOUR__
#define __GST_OPENCV_OBJECTS_AREA_INTERACTION_CONTOUR__

#include <cv.h>
#include <cvaux.h>
#include <draw.h>

typedef struct _InstanceObjectAreaContour InstanceObjectAreaContour;

struct _InstanceObjectAreaContour
{
    gint          id;
    gchar        *name;
    CvSeq        *contour;
    CvMemStorage *mem_storage;
};

void    makeContour      (const gchar *str,
                          CvSeq **seq,
                          CvMemStorage* storage);

void    calcInterception (const InstanceObjectAreaContour *a,
                          const InstanceObjectAreaContour *b,
                          InstanceObjectAreaContour       *dst);

#endif // __GST_OPENCV_OBJECTS_AREA_INTERACTION_CONTOUR__
