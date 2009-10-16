#ifndef _IDENTIFIER_H_
#define _IDENTIFIER_H_

#include "cv.h"
#include "highgui.h"
#include "cvaux.h"
#include <stdio.h>
#include <time.h>

//int learnBackground(IplImage* image, CvBGCodeBookModel* model, IplImage* background);
//IplImage*  segObjectBookBGDiff(CvBGCodeBookModel* model, IplImage* rawImage, IplImage* background);
CvRect segObjectBookBGDiff(CvBGCodeBookModel* model, IplImage* rawImage, IplImage* yuvImage);
void canny(IplImage *image);
CvRect  getRoiMotion( IplImage* img, IplImage* motionHist, int diff_threshold );
//float onlyBiggerObject(IplImage* frameBW);

#endif
