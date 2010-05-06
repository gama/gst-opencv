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
static void     tracker_resample   (Tracker *tracker, CvMat *confidence_density, gfloat ctr, gfloat po);
static gfloat   gaussian_function  (gfloat x, gfloat mean, gfloat variance);
static gfloat   online_classify    (CvRect *detected_obj);
static void     online_train       (CvRect *detected_obj);

Tracker*
tracker_new(const CvRect *region, gint state_vec_dim, gint measurement_vec_dim,
            gint num_particles, IplImage *image,
            gfloat beta, gfloat gama, gfloat mi)
{
    Tracker        *tracker;
    CvRNG           rng_state;
    CvMat          *lowerBound;
    CvMat          *upperBound;
    gint            i;
    CvMat          *particle_positions;
    gfloat          horizontal_sigma, vertical_sigma;

    // allocate tracker struct and initialize condensation filter
    tracker         = g_new0(Tracker, 1);
    tracker->filter = cvCreateConDensation(state_vec_dim, measurement_vec_dim, num_particles);
    tracker->beta = beta;
    tracker->gama = gama;
    tracker->mi = mi;

    tracker->detected_object = g_new(CvRect,1);
    tracker->detected_object->x = region->x;
    tracker->detected_object->y = region->x;
    tracker->detected_object->width = region->width;
    tracker->detected_object->height = region->height;

    lowerBound = cvCreateMat(state_vec_dim, 1, CV_32F);
    upperBound = cvCreateMat(state_vec_dim, 1, CV_32F);

    // coord x
    cvmSet(lowerBound, 0, 0, 0.0);
    cvmSet(upperBound, 0, 0, image->width);

    // coord y
    cvmSet(lowerBound, 1, 0, 0.0);
    cvmSet(upperBound, 1, 0, image->height);

    // x speed (dx/dt)
    cvmSet(lowerBound, 2, 0, 0.0);
    cvmSet(upperBound, 2, 0, 1.0);

    // y speed (dy/dt)
    cvmSet(lowerBound, 3, 0, 0.0);
    cvmSet(upperBound, 3, 0, 1.0);

    // update model (dynamic model)
    // M = [1,0,1,0;
    //      0,1,0,1;
    //      0,0,1,0;
    //      0,0,0,1]
    tracker->filter->DynamMatr[ 0] = 1; tracker->filter->DynamMatr[ 1] = 0; tracker->filter->DynamMatr[2] =  1; tracker->filter->DynamMatr[ 3] = 0;
    tracker->filter->DynamMatr[ 4] = 0; tracker->filter->DynamMatr[ 5] = 1; tracker->filter->DynamMatr[6] =  0; tracker->filter->DynamMatr[ 7] = 1;
    tracker->filter->DynamMatr[ 8] = 0; tracker->filter->DynamMatr[ 9] = 0; tracker->filter->DynamMatr[10] = 1; tracker->filter->DynamMatr[11] = 0;
    tracker->filter->DynamMatr[12] = 0; tracker->filter->DynamMatr[13] = 0; tracker->filter->DynamMatr[14] = 0; tracker->filter->DynamMatr[15] = 1;

    cvConDensInitSampleSet(tracker->filter, lowerBound, upperBound);

    rng_state = cvRNG(0xffffffff);

    particle_positions = cvCreateMat(num_particles, 1, CV_32FC2);

    // the initial particle positions are get from a normal distribution
    // centered at the detection region
    horizontal_sigma = 3 * (region->width  - region->x) / 2; // 3*sigma ~ 99->7% of particles inside of rectangle
    vertical_sigma   = 3 * (region->height - region->y) / 2;

    //TODO: review this distribution
    /*
    cvRandArr(&rng_state, particle_positions, CV_RAND_NORMAL,
              cvScalar(region->x+region->width/2, region->y+region->height/2, 0, 0),
              cvScalar(3 * horizontal_sigma, 3 * vertical_sigma, 0, 0));
    */
    for (i = 0; i < num_particles; ++i) {
    //    CvPoint *pt = (CvPoint*) cvPtr1D(particle_positions, i, 0);
    //FIXME: change this uniforme distribution to the pdf above
        tracker->filter->flSamples[i][0] = (cvRandInt(&rng_state) + tracker->detected_object->x) %
                                            (tracker->detected_object->x+tracker->detected_object->width);  //0 -> x coord
        tracker->filter->flSamples[i][1] =  (cvRandInt(&rng_state) + tracker->detected_object->y) %
                                            (tracker->detected_object->y+tracker->detected_object->height);  //1 -> y coord
    }

    cvReleaseMat(&particle_positions);
    cvReleaseMat(&lowerBound);
    cvReleaseMat(&upperBound);


    return tracker;
}

void
tracker_free(Tracker *tracker)
{
    cvReleaseConDensation(&tracker->filter);
    g_free(tracker);
}

// FIXME: define mean and variance
void
tracker_run(Tracker *tracker, Tracker *closer_tracker_with_a_detected_obj, CvMat *confidence_density)
{
    gfloat ctr, po, mean, variance;

    // sanity checks
    g_assert(tracker != NULL);

    // FIXME: initialize mean and/or variance
    mean = variance = 0.0f;

    ctr  = online_classify(tracker->detected_object);

    // reliability of the detector confidence density
    if (tracker->detected_object != NULL) // if a detection was associated to the tracker
        po = 1.0f;
    else if (closer_tracker_with_a_detected_obj != NULL)
        po = gaussian_function(euclidian_distance(tracker->centroid, closer_tracker_with_a_detected_obj->centroid),
                               mean, variance);
    else po = 0.0f;

    tracker_resample(tracker, confidence_density, ctr, po);
    cvConDensUpdateByTime(tracker->filter);
}

// private methods

// FIXME: define mean and variance
static void
tracker_resample(Tracker *tracker, CvMat *confidence_density, gfloat ctr, gfloat po)
{
    CvPoint particle_pos;
    gfloat  mean, variance;
    gfloat  likelihood, has_detected_obj;
    gint    i;
    double  confidence_density_term;
    gfloat  dist;
    CvPoint top_left, bottom_right;

    // sanity checks
    g_assert(tracker != NULL);

    has_detected_obj = tracker->detected_object != NULL ? 1.0f: 0.0f;

    top_left = cvPoint(tracker->filter->flSamples[0][0], tracker->filter->flSamples[0][1]);
    bottom_right = cvPoint(tracker->filter->flSamples[0][0], tracker->filter->flSamples[0][1]);


    for (i = 0; i < tracker->filter->SamplesNum; i++) {
        particle_pos = cvPoint(tracker->filter->flSamples[i][0], tracker->filter->flSamples[i][1]);

        if (particle_pos.x < top_left.x)
            top_left.x = particle_pos.x;

        if (particle_pos.y < top_left.y)
            top_left.y = particle_pos.y;

        if (particle_pos.x > bottom_right.x)
            bottom_right.x = particle_pos.x;

        if (particle_pos.y > bottom_right.y)
            bottom_right.y = particle_pos.y;

        dist = euclidian_distance(particle_pos, rect_centroid(tracker->detected_object));
        likelihood   = gaussian_function( dist, mean, variance);

//  FIXME: get confidence term from matrix
//        confidence_density_term = cvGetReal2D( confidence_density, particle_pos.y, particle_pos.x );
        confidence_density_term = 1.0;
        tracker->filter->flConfidence[i] = tracker->beta * has_detected_obj*likelihood +
                                           tracker->gama * po * confidence_density_term +
                                           tracker->mi   * ctr;

    }

    tracker->centroid.x = top_left.x + bottom_right.x/2;
    tracker->centroid.y = top_left.y + bottom_right.y/2;
}


// utility functions
static gboolean
rect_is_null(CvRect rect)
{
    return ((rect.x == 0) && (rect.y == 0) && (rect.width == 0) && (rect.height == 0));
}

static gfloat
gaussian_function(gfloat x, gfloat mean, gfloat variance)
{
    // variance = sigma^2
    return ((1 / sqrt(2 * M_PI * variance)) * exp(-((x - mean) * (x - mean))/(2 * variance)));
}

static gfloat
online_classify(CvRect *detected_obj)
{
    // FIXME
    return 0.0f;
}

static void
online_train(CvRect *detected_obj)
{
    return;
}
