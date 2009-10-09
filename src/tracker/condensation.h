#include "cv.h"
#include "highgui.h"
#include "cvaux.h"
#include <stdio.h>
#include <ctype.h>

CvConDensation *initCondensation(int state_vec_dim, int measurement_vec_dim, int sample_size, int max_width, int max_height);
void updateCondensation(IplImage* image, CvConDensation* ConDens, double measurement_x, double measurement_y, double *predicted_x, double *predicted_y );
void  getParticlesBoundary(CvConDensation *ConDens,CvRect *particlesBoundary, int max_width, int max_height);
void centroid(CvPoint2D32f *measurement, int size, double *x, double *y);
