#ifndef _IDENTIFIER_MOTION_H
#define	_IDENTIFIER_MOTION_H

#include "cv.h"
#include <time.h>

#define DIFF_THRESHOLD 30

CvRect motion_detect( IplImage* img, IplImage* motionHist);
CvSeq* motion_detect_mult( IplImage* img, IplImage* motionHist);

#endif	/* _IDENTIFIER_MOTION_H */

