/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 * Copyright (C) 2009 Erickson Nascimento <erickson@vettalabs.com>
 */

#ifndef __GST_OPENCV_FACEMETRIX_DRAW__
#define __GST_OPENCV_FACEMETRIX_DRAW__

#include <glib.h>
#include <cv.h>

#define COLOR_WHITE   CV_RGB(255, 255, 255)
#define COLOR_GREEN   CV_RGB(  0, 255,   0)
#define COLOR_MAGENTA CV_RGB(255,   0, 255)
#define COLOR_BLACK   CV_RGB(  0,   0,   0)
#define COLOR_BLUE    CV_RGB(255,   0,   0)
#define COLOR_RED     CV_RGB(  0,   0, 255)
#define COLOR_YELLOW  CV_RGB(  0, 255, 255)
#define COLOR_CIAN    CV_RGB(255, 255,   0)

#define PIXELSIZECHAR 8


CvScalar colorIdx         (int i);

void     pr               (CvRect ob);

void     pp               (CvPoint ob);

void     pi               (int i);

void     ps               (int i);

void     pt               (char *s);

void     drawFaceIdentify (IplImage *dst,
                           gchar    *idFace,
                           CvRect    rectFace,
                           CvScalar  color,
                           int       drawRect);

int      rectIntercept    (CvRect *a,
                           CvRect *b);

float    distRectToPoint  (CvRect  rect,
                           CvPoint point);

int      pointIntoRect    (CvRect  rect,
                           CvPoint point);


#endif // __GST_OPENCV_FACEMETRIX_DRAW__
