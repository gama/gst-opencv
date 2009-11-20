#ifndef _KMEANS_H
#define	_KMEANS_H

#include "cv.h"
#include "cvaux.h"
#include "highgui.h"
#include "cvtypes.h"
#include "gstfacemetrix.h"

CvRect rectBoudingIdx(CvPoint2D32f *measurement, int size, int idx, int *vetPointsClusterIdx);
void doKmeans(int nClusters, int nPoints, CvPoint2D32f *vetPoints, int *vetPointsClusterIdx);

#endif	/* _KMEANS_H */

