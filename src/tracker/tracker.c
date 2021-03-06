/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
 * Copyright (C) 2010 Erickson Rangel do Nascimento <erickson@vettalabs.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tracker.h"
#include "util.h"

#include <cv.h>
#include <math.h>

// private function prototypes
static void     tracker_resample   (Tracker *tracker, CvMat *confidence_density, IplImage *image, gfloat po);

Tracker*
tracker_new(const CvRect *region, gint state_vec_dim, gint measurement_vec_dim,
            gint num_particles, IplImage *image,
            gfloat beta, gfloat gamma, gfloat eta, gint id)
{
    Tracker        *tracker;
    CvRNG           rng_state;
    CvMat          *lowerBound;
    CvMat          *upperBound;
    gint            i;
    CvMat          *particle_positions;
    CvPoint2D32f    standard_deviation;
    CvPoint2D32f    mean;
    CvPoint         pos;

    // allocate tracker struct and initialize condensation filter
    tracker         = g_new0(Tracker, 1);
    tracker->filter = cvCreateConDensation(state_vec_dim, measurement_vec_dim, num_particles);
    tracker->beta = beta;
    tracker->gamma = gamma;
    tracker->eta = eta;

    tracker->id = id;

    tracker->image_size = cvSize(image->width, image->height);

    tracker->detected_object = g_new(CvRect,1);
    *tracker->detected_object = *region;
    tracker->frames_to_last_detecting = 0;
    tracker->frames_of_wrong_classifier_to_del = 0;

    tracker->tracker_area = *tracker->detected_object;

    lowerBound = cvCreateMat(state_vec_dim, 1, CV_32F);
    upperBound = cvCreateMat(state_vec_dim, 1, CV_32F);

    // coord x
    cvmSet(lowerBound, 0, 0, tracker->detected_object->x);
    cvmSet(upperBound, 0, 0, tracker->detected_object->x + tracker->detected_object->width);

    // coord y
    cvmSet(lowerBound, 1, 0, tracker->detected_object->y);
    cvmSet(upperBound, 1, 0, tracker->detected_object->y + tracker->detected_object->height);

    // x speed (dx/dt)
    cvmSet(lowerBound, 2, 0, 0.0);
    cvmSet(upperBound, 2, 0, 0.0);

    // y speed (dy/dt)
    cvmSet(lowerBound, 3, 0, 0.0);
    cvmSet(upperBound, 3, 0, 0.0);

    // update model (dynamic model)
    // M = | 1 0 1 0 | |   x   |   | x + dx/dt |
    //     | 0 1 0 1 | |   y   | = | y + dy/dt |
    //     | 0 0 1 0 | | dx/dt |   |   dx/dt   |
    //     | 0 0 0 1 | | dy/dt |   |   dy/dt   |
    tracker->filter->DynamMatr[ 0] = 1; tracker->filter->DynamMatr[ 1] = 0; tracker->filter->DynamMatr[2] =  1; tracker->filter->DynamMatr[ 3] = 0;
    tracker->filter->DynamMatr[ 4] = 0; tracker->filter->DynamMatr[ 5] = 1; tracker->filter->DynamMatr[6] =  0; tracker->filter->DynamMatr[ 7] = 1;
    tracker->filter->DynamMatr[ 8] = 0; tracker->filter->DynamMatr[ 9] = 0; tracker->filter->DynamMatr[10] = 1; tracker->filter->DynamMatr[11] = 0;
    tracker->filter->DynamMatr[12] = 0; tracker->filter->DynamMatr[13] = 0; tracker->filter->DynamMatr[14] = 0; tracker->filter->DynamMatr[15] = 1;

    cvConDensInitSampleSet(tracker->filter, lowerBound, upperBound);

    rng_state = cvRNG(0xffffffff);

    particle_positions = cvCreateMat(num_particles, 1, CV_32SC2);

    // the initial particle positions are get from a normal distribution
    // with center at the detection region
    mean = cvPoint2D32f((float)(tracker->detected_object->width)/2.0 + tracker->detected_object->x,
                        (float)(tracker->detected_object->height)/2.0 + tracker->detected_object->y);

    // 3*sigma ~ 99.7% of particles inside of rectangle
    standard_deviation = cvPoint2D32f(  ((float)(tracker->detected_object->width)/2.0)/3.0,
                                        ((float)(tracker->detected_object->height)/2.0)/3.0);
    #if 1
    // normal distribution
    cvRandArr(&rng_state, particle_positions, CV_RAND_NORMAL,
               cvScalar(mean.x, mean.y, 0, 0), // average intensity
               cvScalar(standard_deviation.x, standard_deviation.y, 0, 0) // deviation of the intensity
              );
    #else
    // uniforme distribution
    cvRandArr(&rng_state, particle_positions, CV_RAND_UNI,
                cvScalar(tracker->detected_object->x, tracker->detected_object->y, 0, 0),
                cvScalar(   tracker->detected_object->x + tracker->detected_object->width,
                            tracker->detected_object->y + tracker->detected_object->height, 0, 0));

    #endif

    //tracker->filter->State[0] = mean.x;
    //tracker->filter->State[1] = mean.y;

    tracker->max_confidence = 0;
    for (i = 0; i < num_particles; ++i) {
        pos = *(CvPoint*)cvPtr1D(particle_positions, i, 0);
        tracker->filter->flSamples[i][0] = pos.x; //0 -> x coord
        tracker->filter->flSamples[i][1] = pos.y; //1 -> y coord


        if (tracker->max_confidence < tracker->filter->flConfidence[i])
           tracker->max_confidence = tracker->filter->flConfidence[i];
    }

    // init learn process
    tracker->classifier = classifier_intermediate_init(image, *tracker->detected_object);

    cvReleaseMat(&particle_positions);
    cvReleaseMat(&lowerBound);
    cvReleaseMat(&upperBound);

    return tracker;
}

void
tracker_free(Tracker *tracker)
{
    cvReleaseConDensation(&tracker->filter);
    g_free(tracker->detected_object);
    g_free(tracker->classifier);
    g_free(tracker);
}

// FIXME: define mean and variance
void
tracker_run(Tracker *tracker, Tracker *closer_tracker_with_a_detected_obj, CvMat *confidence_density, IplImage *image)
{
    gfloat po, mean, variance;
    gfloat new_area, old_area, ratio;

    // sanity checks
    g_assert(tracker != NULL);


    // reliability of the detector confidence density
    if (tracker->detected_object != NULL){ // if a detection was associated to the tracker
        po = 1.0f;

        // FIXME: use the return of function
        classifier_intermediate_train(tracker->classifier, image, *tracker->detected_object);

        new_area = (float)(tracker->detected_object->width * tracker->detected_object->height);
        old_area = (float)(tracker->tracker_area.width * tracker->tracker_area.height);

        ratio = abs(new_area-old_area)/old_area;

        // update the tracker area only it changes less than 60%
        if ( ratio <= 0.6 )
            tracker->tracker_area = *tracker->detected_object;

    }
    else if (closer_tracker_with_a_detected_obj != NULL){
        mean = 1.0f;
        variance = sqrt( pow(closer_tracker_with_a_detected_obj->detected_object->width,2) + // diagonal
                        pow(closer_tracker_with_a_detected_obj->detected_object->height,2));
        po = gaussian_function(euclidian_distance(
                                    cvPoint(tracker->filter->State[0], tracker->filter->State[1]),
                                    cvPoint(closer_tracker_with_a_detected_obj->filter->State[0],
                                            closer_tracker_with_a_detected_obj->filter->State[1])),
                               mean, variance);
    }
    else po = 0.0f;

    tracker_resample(tracker, confidence_density, image, po);
    cvConDensUpdateByTime(tracker->filter);

    tracker->previous_centroid = rect_centroid(&tracker->tracker_area);
    tracker->tracker_area.x = tracker->filter->State[0] - tracker->tracker_area.width/2;
    tracker->tracker_area.y = tracker->filter->State[1] - tracker->tracker_area.height/2;
}

// private methods

static void
tracker_resample(Tracker *tracker, CvMat *confidence_density, IplImage *image, gfloat po)
{
    CvPoint particle_pos;
    gfloat  mean, variance;
    gfloat  likelihood;
    gboolean has_detected_obj;
    gfloat  ctr;
    gint    i;
    double  confidence_density_term;
    gfloat  dist;
    gfloat min_confidence;
    CvRect tr_rect;
    CvPoint tr_rect_origin, tr_rect_original_centroid;

    // sanity checks
    g_assert(tracker != NULL);

    has_detected_obj = tracker->detected_object != NULL ? TRUE: FALSE;

    // Store informations to create the rect centralized in each particle
    tr_rect = tracker->tracker_area;
    tr_rect_origin = cvPoint(tr_rect.x, tr_rect.y);
    tr_rect_original_centroid = rect_centroid(&tracker->tracker_area);

    min_confidence = G_MAXFLOAT;
    tracker->max_confidence = 0;

    for (i = 0; i < tracker->filter->SamplesNum; i++) {
        particle_pos = cvPoint(tracker->filter->flSamples[i][0], tracker->filter->flSamples[i][1]);
        // FIXME: check if some particles can have negative positition?
        if (particle_pos.x < tracker->image_size.width &&
            particle_pos.y < tracker->image_size.height &&
            particle_pos.x >= 0 && particle_pos.y >= 0)
        {
            if (has_detected_obj){
                dist = euclidian_distance(particle_pos, rect_centroid(tracker->detected_object));

                mean = 10;
                //TODO: is it the best variance to use?
                variance =  sqrt(   pow(tracker->detected_object->width,2) + // diagonal
                                pow(tracker->detected_object->height,2));

                likelihood   = gaussian_function(dist, mean, variance);
            }
            else likelihood = 0;

            // Moves the object rect so that it centered on the particle
            tr_rect.x = tr_rect_origin.x + particle_pos.x - tr_rect_original_centroid.x;
            tr_rect.y = tr_rect_origin.y + particle_pos.y - tr_rect_original_centroid.y;
            //FIXME: check if ctr = 0.0f is the best value to paritcles that have rect region outside of image
            if (tr_rect.x + tr_rect.width < image->width && tr_rect.y + tr_rect.height < image->height)
                ctr = classifier_intermediate_classify(tracker->classifier, image, tr_rect);
            else
                ctr = 0.0f;

            if (confidence_density){
                confidence_density_term = cvGetReal2D( confidence_density, particle_pos.y, particle_pos.x );
            }
            else confidence_density_term = 1.0;

            tracker->filter->flConfidence[i] = tracker->beta * likelihood +
                                               tracker->gamma * po * confidence_density_term +
                                               tracker->eta  * ctr;
            if (min_confidence > tracker->filter->flConfidence[i])
               min_confidence = tracker->filter->flConfidence[i];
            if (tracker->max_confidence < tracker->filter->flConfidence[i])
               tracker->max_confidence = tracker->filter->flConfidence[i];


        } else tracker->filter->flConfidence[i] = 0;

    }

    // min confidence of the particles is shift to 0 if it is negativo
    if (min_confidence < 0)
        for (i = 0; i < tracker->filter->SamplesNum; i++)
            tracker->filter->flConfidence[i] = tracker->filter->flConfidence[i] - min_confidence;
}


// utility functions
static gboolean
rect_is_null(CvRect rect)
{
    return ((rect.x == 0) && (rect.y == 0) && (rect.width == 0) && (rect.height == 0));
}

gfloat
gaussian_function(gfloat x, gfloat mean, gfloat standard_deviation)
{
    // variance = sigma^2
    gfloat variance;
    variance = standard_deviation*standard_deviation;

    return ((1 / sqrt(2 * M_PI * variance)) * exp(-((x - mean) * (x - mean))/(2 * variance)));
}

float
dist_point_segment(gfloat x, gfloat y, gfloat x1, gfloat y1, gfloat x2, gfloat y2)
{
    gfloat A = x - x1;
    gfloat B = y - y1;
    gfloat C = x2 - x1;
    gfloat D = y2 - y1;

    gfloat dot = A * C + B * D;
    gfloat len_sq = C * C + D * D;
    gfloat param = dot / len_sq;

    gfloat xx, yy;

    if (param < 0) {
        xx = x1;
        yy = y1;
    } else if (param > 1) {
        xx = x2;
        yy = y2;
    } else {
        xx = x1 + param * C;
        yy = y1 + param * D;
    }

    return sqrt(((x - xx) * (x - xx)) + ((y - yy) * (y - yy)));
}

gfloat
get_inner_angle_b(CvPoint a, CvPoint b, CvPoint c)
{
    gfloat coef_ang_ba, ang_ba, ang_ba_left, ang_ba_right;
    gfloat coef_ang_bc, ang_bc, ang_bc_left, ang_bc_right;
    gfloat temp, result;

    if((a.x == b.x && a.y == b.y) || (a.x == c.x && a.y == c.y) || (c.x == b.x && c.y == b.y))
        return 0;

    coef_ang_ba = (gfloat) (a.y - b.y) / (a.x - b.x);
    ang_ba = (atan(coef_ang_ba)*(180 / M_PI));
    ang_ba_left = (ang_ba > 0) ? ang_ba : ang_ba + 180;
    ang_ba_right = 180 - ang_ba_left;

    coef_ang_bc = (gfloat) (c.y - b.y) / (c.x - b.x);
    ang_bc = (atan(coef_ang_bc)*(180 / M_PI));
    ang_bc_right = (ang_bc > 0) ? ang_bc : ang_bc + 180;
    ang_bc_left = 180 - ang_bc_right;

    if ((b.y > a.y && b.y > c.y) || (b.y < a.y && b.y < c.y)) {
        temp = abs(ang_ba_left - ang_bc_right);
        result = ((180-temp) < temp)? temp = (180-temp) : temp;
    } else if (b.y > c.y) {
        result = (ang_bc_right + ang_ba_left < ang_bc_left + ang_ba_right) ? ang_bc_right + ang_ba_left : ang_bc_left + ang_ba_right;
    } else if (b.y == c.y && b.y == a.y && ((a.x >= b.x && a.x <= c.x) || (c.x >= b.x && c.x <= a.x))) {
        result = (ang_ba_left < ang_ba_right)? ang_ba_left : ang_ba_right;
    } else {
        result = (ang_bc_left + ang_ba_left < ang_bc_right + ang_ba_right) ? ang_bc_left + ang_ba_left : ang_bc_right + ang_ba_right;
    }

    return abs(result);
}
