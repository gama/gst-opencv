/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 * Copyright (C) 2009 Erickson Nascimento <erickson@vettalabs.com>
 */

#ifndef __GST_OPENCV_COMMON_CONDENSATION__
#define __GST_OPENCV_COMMON_CONDENSATION__

#include <cv.h>
#include <cvaux.h>

CvConDensation* initCondensation       (int state_vec_dim,
                                        int measurement_vec_dim,
                                        int sample_size,
                                        int max_width,
                                        int max_height);
 
void            updateCondensation     (IplImage       *image,
                                        CvConDensation *ConDens,
                                        CvPoint        *vetCentroids,
                                        int             nCentroid,
                                        int             show_particles);

void            getParticlesBoundary   (CvConDensation *ConDens,
                                        CvRect         *particlesBoundary,
                                        int             max_width,
                                        int             max_height);

void            centroid               (CvPoint2D32f *measurement,
                                        int           size,
                                        double       *x,
                                        double       *y);

/*
void            updateCondensation_old (IplImage* image,
                                        CvConDensation* ConDens,
                                        double measurement_x,
                                        double measurement_y,
                                        int show_particles,
                                        double *predicted_x,
                                        double *predicted_y);
*/

#endif // __GST_OPENCV_COMMON_CONDENSATION__
