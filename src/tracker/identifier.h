#ifndef _IDENTIFIER_H_
#define _IDENTIFIER_H_

#include "cv.h"
#include "highgui.h"
#include "cvaux.h"
#include <stdio.h>
#include <time.h>

CvRect segObjectBookBGDiff(CvBGCodeBookModel* model, IplImage* rawImage, IplImage* yuvImage);
void showBorder( IplImage *srcGray, IplImage *dst, CvScalar borderColor, int edge_thresh, int smooth, int borderIncreaseSize );

#endif
