/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 */

#ifndef __GST_OPENCV_COMMON_DRAW__
#define __GST_OPENCV_COMMON_DRAW__

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
#define LABEL_BORDER  4

CvScalar colorIdx           (int i);

void     pr                 (CvRect        ob);

void     pp                 (CvPoint       ob);

void     pi                 (int           i);

void     ps                 (int           i);

void     pt                 (char         *s);

void     draw_face_id       (IplImage     *dst,
                             const gchar  *face_id,
                             const CvRect *face_rect,
                             CvScalar      color,
                             float         font_scale,
                             gboolean      draw_face_box);

int      rectIntercept      (CvRect       *a,
                             CvRect       *b);

float    distRectToPoint    (CvRect        rect,
                             CvPoint       point);

int      pointIntoRect      (CvRect        rect,
                             CvPoint       point);

void     printText          (IplImage     *dst,
                             CvPoint       point,
                             const char   *text,
                             CvScalar      color,
                             float         font_scale,
                             int           box1_nobox0);

void     monitorImage       (IplImage     *src_bin,
                             IplImage     *dst,
                             int           scale,
                             int           quadrant);

int      contourIsFull      (CvSeq        *contour,
                             IplImage     *src);

double   euclid_dist_points (CvPoint2D32f  p1,
                             CvPoint2D32f  p2);

#endif
