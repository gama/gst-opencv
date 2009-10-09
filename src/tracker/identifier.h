#ifndef _IDENTIFIER_H_
#define _IDENTIFIER_H_

#include "cv.h"
#include "highgui.h"
#include "cvaux.h"

int learnBackground(IplImage* image, CvBGCodeBookModel* model, IplImage* background);
IplImage* segObjectBookBGDiff(CvBGCodeBookModel* model, IplImage* rawImage, IplImage* background);

#endif
