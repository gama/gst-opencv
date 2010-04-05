/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 */

#ifndef __GST_OPENCV_COMMON_DRAW__
#define __GST_OPENCV_COMMON_DRAW__

#include <glib.h>
#include <cv.h>

#include <cxcore.h>
#include <cv.h>
#include <highgui.h>
#include <cvaux.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define COLOR_WHITE   CV_RGB(255, 255, 255)
#define COLOR_GREEN   CV_RGB(  0, 255,   0)
#define COLOR_MAGENTA CV_RGB(255,   0, 255)
#define COLOR_BLACK   CV_RGB(  0,   0,   0)
#define COLOR_BLUE    CV_RGB(255,   0,   0)
#define COLOR_RED     CV_RGB(  0,   0, 255)
#define COLOR_YELLOW  CV_RGB(  0, 255, 255)
#define COLOR_CIAN    CV_RGB(255, 255,   0)
#define LABEL_BORDER  4

CvScalar colorIdx                   (int            i);

void     pr                         (CvRect         ob);

void     pp                         (CvPoint        ob);

void     pi                         (int            i);

void     ps                         (int            i);

void     pt                         (char          *s);

void     draw_face_id               (IplImage      *dst,
                                     const gchar   *face_id,
                                     const CvRect  *face_rect,
                                     CvScalar       color,
                                     float          font_scale,
                                     gboolean       draw_face_box);

float    rectIntercept              (const CvRect  *a,
                                     const CvRect  *b);

CvRect   rectIntersection           (const CvRect  *a,
                                     const CvRect  *b);

float    distRectToPoint            (CvRect         rect,
                                     CvPoint        point);

int      pointIntoRect              (CvRect         rect,
                                     CvPoint        point);

void     printText                  (IplImage      *dst,
                                     CvPoint        point,
                                     const char    *text,
                                     CvScalar       color,
                                     float          font_scale,
                                     int            box1_nobox0);

void     monitorImage               (IplImage      *src_bin,
                                     IplImage      *dst,
                                     int            scale,
                                     int            quadrant);

int      contourIsFull              (CvSeq         *contour,
                                     IplImage      *src);

double   euclid_dist_points         (CvPoint2D32f   p1,
                                     CvPoint2D32f   p2);


void     draw_mesh                  (IplImage      *dst,
                                     const CvPoint  q1,
                                     const CvPoint  q2,
                                     const CvPoint  q3,
                                     const CvPoint  q4,
                                     const int      w_lines,
                                     const int      h_lines,
                                     CvScalar       color);

void     set_proportion_and_border  (CvRect        *rect,
                                     float          scale,
                                     int            border_x,
                                     int            border_y);

CvPoint  medium_point               (CvPoint        a,
                                     CvPoint        b);

int      inner_circle               (int            r,
                                     CvPoint        c,
                                     CvPoint        p);

float    euclid_dist_cvpoints       (CvPoint        p1,
                                     CvPoint        p2);

double   linear_projection          (const double   min_real,
                                     const double   min_projected,
                                     const double   max_real,
                                     const double   max_projected,
                                     const double   known_projected_value);

#endif
