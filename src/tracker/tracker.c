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

#include <cv.h>
#include <math.h>

// private function prototypes
static void     tracker_resample   (Tracker *tracker, gfloat ctr, gfloat po);
static CvPoint  tracker_centroid   (Tracker *tracker);
static CvPoint  rect_centroid      (CvRect rect);
static gboolean rect_is_null       (CvRect rect);
static gfloat   gaussian_function  (gfloat x, gfloat mean, gfloat variance);
static gfloat   euclidian_distance (CvPoint p1, CvPoint p2);
static gfloat   online_classifier  (CvRect detected_obj);

Tracker*
tracker_new(CvRect region, gint state_vec_dim, gint measurement_vec_dim,
            gint num_particles, CvRect image)
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

    lowerBound = cvCreateMat(state_vec_dim, 1, CV_32F);
    upperBound = cvCreateMat(state_vec_dim, 1, CV_32F);

    // coord x
    cvmSet(lowerBound, 0, 0, 0.0);
    cvmSet(upperBound, 0, 0, image.width);

    // coord y
    cvmSet(lowerBound, 1, 0, 0.0);
    cvmSet(upperBound, 1, 0, image.height);

    // x speed (dx/dt)
    cvmSet(lowerBound, 2, 0, 0.0);
    cvmSet(upperBound, 2, 0, 1.0);

    // y speed (dy/dt)
    cvmSet(lowerBound, 3, 0, 0.0);
    cvmSet(upperBound, 3, 0, 1.0);

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
    horizontal_sigma = 3 * (region.width  - region.x) / 2; // 3*sigma ~ 99.7% of particles inside of rectangle
    vertical_sigma   = 3 * (region.height - region.y) / 2;

    cvRandArr(&rng_state, particle_positions, CV_RAND_NORMAL,
              cvScalar(region.x, region.y, 0, 0),
              cvScalar(3 * horizontal_sigma, 3 * vertical_sigma, 0, 0));

    for (i = 0; i < num_particles; ++i) {
        CvPoint *pt = (CvPoint*) cvPtr1D(particle_positions, i, 0);
        tracker->filter->flSamples[i][0] = pt->x; // 0 -> x coord
        tracker->filter->flSamples[i][1] = pt->y; // 1 -> y coord
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
tracker_run(Tracker *tracker, CvRect closer_tracker_with_a_detected_obj)
{
    gfloat ctr, po, mean, variance;
    CvRect detected_obj;

    // sanity checks
    g_assert(tracker != NULL);

    // FIXME: initialize mean and/or variance
    mean = variance = 0.0f;

    detected_obj = tracker->detected_object;
    ctr          = online_classifier(detected_obj);

    // reliability of the detector confidence density
    if (rect_is_null(detected_obj) == FALSE) // if a detection was associated to the tracker
        po = 1.0f;
    else if (rect_is_null(closer_tracker_with_a_detected_obj) == FALSE)
        po = gaussian_function(euclidian_distance(tracker_centroid(tracker), rect_centroid(closer_tracker_with_a_detected_obj)),
                               mean, variance);
    else po = 0.0f;

    tracker_resample(tracker, ctr, po);
}

// private methods

// FIXME: define mean and variance
static void
tracker_resample(Tracker *tracker, gfloat ctr, gfloat po)
{
    CvPoint particle_pos;
    CvRect  d_star;
    gfloat  mean, variance, likelihood, has_detected_obj;
    gint    i;

    // sanity checks
    g_assert(tracker != NULL);

    // sample_x = g_new(double, ConDens->SamplesNum);
    // sample_y = g_new(double, ConDens->SamplesNum);

    d_star           = tracker->detected_object;
    has_detected_obj = rect_is_null(d_star) ? 1.0f: 0.0f;

    for (i = 0; i < tracker->filter->SamplesNum; i++) {
        particle_pos = cvPoint(tracker->filter->flSamples[i][0], tracker->filter->flSamples[i][1]);
        likelihood   = gaussian_function(euclidian_distance(particle_pos,rect_centroid(d_star)), mean, variance);
        tracker->filter->flConfidence[i] = tracker->beta * has_detected_obj*likelihood +
                                           tracker->gama * po +
                                           tracker->mi   * ctr;

    }
    // g_free(sample_x);
    // g_free(sample_y);
}


// FIXME
static CvPoint
tracker_centroid(Tracker *tracker)
{
    return cvPoint(0.0f, 0.0f);
}

// utility functions

// FIXME
static CvPoint
rect_centroid(CvRect rect)
{
    return cvPoint(0.0f, 0.0f);
}

static gboolean
rect_is_null(CvRect rect)
{
    if ((rect.x == 0) && (rect.y == 0) && (rect.width == 0) && (rect.height == 0))
        return 1;
    return 0;
}

static gfloat
gaussian_function(gfloat x, gfloat mean, gfloat variance)
{
    // variance = sigma^2
    return ((1 / sqrt(2 * M_PI * variance)) * exp(-((x - mean) * (x - mean))/(2 * variance)));
}

static gfloat
euclidian_distance(CvPoint p1, CvPoint p2)
{
    return sqrt((p2.x-p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
}

static gfloat
online_classifier(CvRect detected_obj)
{
    // FIXME
    return 0.0f;
}
