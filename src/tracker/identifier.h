#ifndef _IDENTIFIER_H_
#define _IDENTIFIER_H_

#include "cv.h"
#include "highgui.h"
#include "cvaux.h"
#include <stdio.h>
#include <time.h>

CvRect segObjectBookBGDiff(CvBGCodeBookModel* model, IplImage* rawImage, IplImage* yuvImage);
void canny(IplImage *image, CvRect rect, int edge_thresh, int smooth);

#endif
