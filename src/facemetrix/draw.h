#ifndef _DRAW_H
#define	_DRAW_H

#define COLOR_WHITE CV_RGB(255,255,255)
#define COLOR_GREEN CV_RGB(0,255,0)
#define COLOR_MAGENTA CV_RGB(255,0,255)
#define COLOR_BLACK CV_RGB(0,0,0)

//RGB
//#define COLOR_BLUE CV_RGB(0,0,255)
//#define COLOR_RED CV_RGB(255,0,0)
//#define COLOR_YELLOW CV_RGB(255,255,0)
//#define COLOR_CIAN CV_RGB(0,255,255)

//BGR
#define COLOR_BLUE CV_RGB(255,0,0)
#define COLOR_RED CV_RGB(0,0,255)
#define COLOR_YELLOW CV_RGB(0,255,255)
#define COLOR_CIAN CV_RGB(255,255,0)


#include <gst/gst.h>
#include "cv.h"

CvScalar colorIdx(int i);
void drawFaceIdentify(IplImage *dst, gchar *idFace, CvRect rectFace, CvScalar color);

void pr(CvRect ob);
void pp(CvPoint ob);
void pi(int i);

#endif
