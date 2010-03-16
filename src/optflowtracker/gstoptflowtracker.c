/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
 * Copyright (C) 2010 Lucas Pantuza Amorim <lucas@vettalabs.com>
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

/**
 * SECTION:element-optflowtracker
 * 
 * Add 'optical flow' (pyramid + lucas kanade) based tracker
 *
 * This tracker element uses the ROIs supplied by an upstream haaradjust
 * element and the foreground mask & ROIs (also supplied by a foreground
 * detection element) to extract features and later track them using the
 * optical flow algorithm. The exact algorithm used by this version is the
 * pyramid+lucas-kanade one.
 *
 * The rule to match objects is very simplistic: we just compare the
 * percentage of overlap between the haar ROI and the known objects. It's
 * rather silly but it's the best we could come up with for the time being
 * -- plus, it works reasonably well for the simpler cases.
 *
 * Object prunning is done using two simple rules: (1) if the percentage of
 * matching features between two frames drops below a given threshold or
 * (2), if a object is tracked for more than a given number of frames
 * without an event from the upstream haaradjust element -- which is used
 * as confirmation that the tracked object stil preserves the same
 * properties.
 * 
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace      !
 *                 bgfgcodebook mask=true roi=true                  !
 *                 haardetect profile=haarcascade_mcs_upperbody.xml !
 *                 haaradjust                                       !
 *                 optflowtracker verbose=true display=true         !
 *                 ffmpegcolorspace ! xvimagesink sync=false
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gstoptflowtracker.h>

#include <gst/gst.h>
#include <gst/gststructure.h>
#include <cvaux.h>
#include <highgui.h>

#include "draw.h"

GST_DEBUG_CATEGORY_STATIC(gst_optical_flow_tracker_debug);
#define GST_CAT_DEFAULT gst_optical_flow_tracker_debug

#define DEFAULT_FEATURES_MAX_SIZE        500
#define DEFAULT_FEATURES_QUALITY_LEVEL     0.01
#define DEFAULT_FEATURES_MIN_DISTANCE      5
#define DEFAULT_CORNER_SUBPIX_WIN_SIZE     5
#define DEFAULT_TERM_CRITERIA_ITERATIONS  20
#define DEFAULT_TERM_CRITERIA_ACCURACY     0.03
#define DEFAULT_PYRAMID_LEVELS             3
#define DEFAULT_MIN_OVERLAP_AREA_PERC      0.6f
#define DEFAULT_MIN_MATCH_FEATURES_PERC    0.5f
#define DEFAULT_NUM_NOISE_FRAMES           2
#define DEFAULT_MAX_NOISE_TRACKED_FRAMES   5
#define DEFAULT_MAX_TRACKED_FRAMES        10

#define HAAR_COLOR      cvScalar(31, 31, 159, 0)
#define TRACKED_COLOR   cvScalar(95, 95, 255, 0)
#define DEBUG_BOX_COLOR cvScalar(47, 47,  47, 0)

enum {
    PROP_0,
    PROP_VERBOSE,
    PROP_DISPLAY,
    PROP_FEATURES_MAX_SIZE,
    PROP_FEATURES_QUALITY_LEVEL,
    PROP_FEATURES_MIN_DISTANCE,
    PROP_CORNER_SUBPIX_WIN_SIZE,
    PROP_TERM_CRITERIA_ITERATIONS,
    PROP_TERM_CRITERIA_ACCURACY,
    PROP_PYRAMID_LEVELS,
    PROP_MIN_OVERLAP_AREA_PERC,
    PROP_MIN_MATCH_FEATURES_PERC,
    PROP_NUM_NOISE_FRAMES,
    PROP_MAX_NOISE_TRACKED_FRAMES,
    PROP_MAX_TRACKED_FRAMES
};

struct _InstanceObject
{
    guint         id;
    CvPoint2D32f *features;
    CvPoint2D32f *prev_features;
    guint         n_features;
    guint         haar_n_features;
    guint         n_haar_frames;
    guint         last_frame;
    guint         last_haar_frame;
    CvRect        rect;
    GstClockTime  timestamp;
};
typedef struct _InstanceObject InstanceObject;

// the capabilities of the inputs and outputs.
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE("sink",
        GST_PAD_SINK,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-raw-rgb")
        );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE("src",
        GST_PAD_SRC,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-raw-rgb")
        );

GST_BOILERPLATE(GstOpticalFlowTracker, gst_optical_flow_tracker, GstElement, GST_TYPE_ELEMENT);

static void          gst_optical_flow_tracker_set_property    (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_optical_flow_tracker_get_property    (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean      gst_optical_flow_tracker_set_caps        (GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_optical_flow_tracker_chain           (GstPad *pad, GstBuffer *buf);
static IplImage*     gst_optical_flow_tracker_print_fg_mask   (GstOpticalFlowTracker *filter);
static IplImage*     gst_optical_flow_tracker_print_haar_mask (GstOpticalFlowTracker *filter, CvRect *rect);
static gboolean      events_cb                                (GstPad *pad, GstEvent *event, gpointer user_data);
static CvRect        rect_intersection                        (const CvRect *a, const CvRect *b);
static float         rect_area_overlap_perc                   (const CvRect *a, const CvRect *b);

// gobject vmethod implementations
static void
gst_optical_flow_tracker_finalize(GObject *obj)
{
    GstOpticalFlowTracker *filter = GST_OPTICAL_FLOW_TRACKER(obj);

    if (filter->image) cvReleaseImage(&filter->image);
    if (filter->gray)  cvReleaseImage(&filter->gray);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
gst_optical_flow_tracker_base_init(gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "optflowtracker",
                                         "Filter/Video",
                                         "Tracks objects using the 'Pyramid/Lucas Kanade' optical flow algorithm",
                                         "Lucas Pantuza Amorim <lucas@vettalabs.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the 'optical flow tracker' class
static void
gst_optical_flow_tracker_class_init(GstOpticalFlowTrackerClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_optical_flow_tracker_finalize);
    gobject_class->set_property = gst_optical_flow_tracker_set_property;
    gobject_class->get_property = gst_optical_flow_tracker_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose",
                                                         "Sets whether the movement direction should be printed to the standard output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display",
                                                         "Highligh the metrixed faces in the video output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_FEATURES_MAX_SIZE,
                                    g_param_spec_uint("features-max-size", "Features max size",
                                                      "",
                                                      0, UINT_MAX, DEFAULT_FEATURES_MAX_SIZE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_FEATURES_QUALITY_LEVEL,
                                    g_param_spec_float("features-quality-level", "Features quality level",
                                                      "",
                                                      0.0, FLT_MAX, DEFAULT_FEATURES_QUALITY_LEVEL, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_FEATURES_MIN_DISTANCE,
                                    g_param_spec_uint("features-min-distance", "Features min distance",
                                                      "",
                                                      0, UINT_MAX, DEFAULT_FEATURES_MIN_DISTANCE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CORNER_SUBPIX_WIN_SIZE,
                                    g_param_spec_uint("corner-subpix-win-size", "Corner subpix win size",
                                                      "",
                                                      0, UINT_MAX, DEFAULT_CORNER_SUBPIX_WIN_SIZE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_TERM_CRITERIA_ITERATIONS,
                                    g_param_spec_uint("term-criteria-iterations", "Term Criteria Iterations",
                                                      "",
                                                      0, UINT_MAX, DEFAULT_TERM_CRITERIA_ITERATIONS, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_TERM_CRITERIA_ACCURACY,
                                    g_param_spec_float("term-criteria-accuracy", "Term Criteria Accuracy",
                                                      "",
                                                      0.0, FLT_MAX, DEFAULT_TERM_CRITERIA_ACCURACY, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_PYRAMID_LEVELS,
                                    g_param_spec_uint("pyramid-levels", "Pyramid Levels",
                                                      "",
                                                      0, UINT_MAX, DEFAULT_PYRAMID_LEVELS, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MIN_OVERLAP_AREA_PERC,
                                    g_param_spec_float("min-overlap-area-perc", "Min Overlap Area Percentage",
                                                      "",
                                                      0.0, 1.0, DEFAULT_MIN_OVERLAP_AREA_PERC, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MIN_MATCH_FEATURES_PERC,
                                    g_param_spec_float("min-match-features-perc", "Min Matching Features Percentage",
                                                      "",
                                                      0.0, 1.0, DEFAULT_MIN_MATCH_FEATURES_PERC, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NUM_NOISE_FRAMES,
                                    g_param_spec_uint("num-noise-frames", "Number of Haar noise frames",
                                                      "",
                                                      0, UINT_MAX, DEFAULT_NUM_NOISE_FRAMES, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MAX_NOISE_TRACKED_FRAMES,
                                    g_param_spec_uint("max-noise-tracked-frames", "Max Noise Tracked Frames",
                                                      "",
                                                      0, UINT_MAX, DEFAULT_MAX_NOISE_TRACKED_FRAMES, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MAX_TRACKED_FRAMES,
                                    g_param_spec_uint("max-tracked-frames", "Max Tracked Frames",
                                                      "",
                                                      0, UINT_MAX, DEFAULT_MAX_TRACKED_FRAMES, G_PARAM_READWRITE));
}

// initialize the new element
// instantiate pads and add them to element
// set pad calback functions
// initialize instance structure
static void
gst_optical_flow_tracker_init(GstOpticalFlowTracker *filter, GstOpticalFlowTrackerClass *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_optical_flow_tracker_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_optical_flow_tracker_chain));

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    // parameters
    filter->verbose                   = FALSE;
    filter->display                   = FALSE;
    filter->features_max_size         = DEFAULT_FEATURES_MAX_SIZE;
    filter->features_quality_level    = DEFAULT_FEATURES_QUALITY_LEVEL;
    filter->features_min_distance     = DEFAULT_FEATURES_MIN_DISTANCE;
    filter->corner_subpix_win_size    = DEFAULT_CORNER_SUBPIX_WIN_SIZE;
    filter->term_criteria_iterations  = DEFAULT_TERM_CRITERIA_ITERATIONS;
    filter->term_criteria_accuracy    = DEFAULT_TERM_CRITERIA_ACCURACY;
    filter->pyramid_levels            = DEFAULT_PYRAMID_LEVELS;
    filter->min_overlap_area_perc     = DEFAULT_MIN_OVERLAP_AREA_PERC;
    filter->min_match_features_perc   = DEFAULT_MIN_MATCH_FEATURES_PERC;
    filter->n_noise_frames            = DEFAULT_NUM_NOISE_FRAMES;
    filter->max_noise_tracked_frames  = DEFAULT_MAX_NOISE_TRACKED_FRAMES;
    filter->max_tracked_frames        = DEFAULT_MAX_TRACKED_FRAMES;

    filter->n_frames                  = 0;
    filter->n_objects                 = 0;
    filter->haar_roi_timestamp        = 0;
    filter->fg_roi_timestamp          = 0;
    filter->fg_mask_timestamp         = 0;
    filter->haar_roi_array            = g_array_new(FALSE, FALSE, sizeof(CvRect));
    filter->fg_roi_array              = g_array_new(FALSE, FALSE, sizeof(CvRect));
    filter->stored_objects            = g_ptr_array_new_with_free_func(g_free);
    filter->fg_mask                   = NULL;
}

static void
gst_optical_flow_tracker_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstOpticalFlowTracker *filter = GST_OPTICAL_FLOW_TRACKER(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY:
            filter->display = g_value_get_boolean(value);
            break;
        case PROP_FEATURES_MAX_SIZE:
            filter->features_max_size = g_value_get_uint(value);
            break;
        case PROP_FEATURES_QUALITY_LEVEL:
            filter->features_quality_level = g_value_get_float(value);
            break;
        case PROP_FEATURES_MIN_DISTANCE:
            filter->features_min_distance = g_value_get_uint(value);
            break;
        case PROP_CORNER_SUBPIX_WIN_SIZE:
            filter->corner_subpix_win_size = g_value_get_uint(value);
            break;
        case PROP_TERM_CRITERIA_ITERATIONS:
            filter->term_criteria_iterations = g_value_get_uint(value);
            break;
        case PROP_TERM_CRITERIA_ACCURACY:
            filter->term_criteria_accuracy = g_value_get_float(value);
            break;
        case PROP_PYRAMID_LEVELS:
            filter->pyramid_levels = g_value_get_uint(value);
            break;
        case PROP_MIN_OVERLAP_AREA_PERC:
            filter->min_overlap_area_perc = g_value_get_float(value);
            break;
        case PROP_MIN_MATCH_FEATURES_PERC:
            filter->min_match_features_perc = g_value_get_float(value);
            break;
        case PROP_NUM_NOISE_FRAMES:
            filter->n_noise_frames = g_value_get_uint(value);
            break;
        case PROP_MAX_NOISE_TRACKED_FRAMES:
            filter->max_noise_tracked_frames = g_value_get_uint(value);
            break;
        case PROP_MAX_TRACKED_FRAMES:
            filter->max_tracked_frames = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_optical_flow_tracker_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstOpticalFlowTracker *filter = GST_OPTICAL_FLOW_TRACKER(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_DISPLAY:
            g_value_set_boolean(value, filter->display);
            break;
        case PROP_FEATURES_MAX_SIZE:
            g_value_set_uint(value, filter->features_max_size);
            break;
        case PROP_FEATURES_QUALITY_LEVEL:
            g_value_set_float(value, filter->features_quality_level);
            break;
        case PROP_FEATURES_MIN_DISTANCE:
            g_value_set_uint(value, filter->features_min_distance);
            break;
        case PROP_CORNER_SUBPIX_WIN_SIZE:
            g_value_set_uint(value, filter->corner_subpix_win_size);
            break;
        case PROP_TERM_CRITERIA_ITERATIONS:
            g_value_set_uint(value, filter->term_criteria_iterations);
            break;
        case PROP_TERM_CRITERIA_ACCURACY:
            g_value_set_float(value, filter->term_criteria_accuracy);
            break;
        case PROP_PYRAMID_LEVELS:
            g_value_set_uint(value, filter->pyramid_levels);
            break;
        case PROP_MIN_OVERLAP_AREA_PERC:
            g_value_set_float(value, filter->min_overlap_area_perc);
            break;
        case PROP_MIN_MATCH_FEATURES_PERC:
            g_value_set_float(value, filter->min_match_features_perc);
            break;
        case PROP_NUM_NOISE_FRAMES:
            g_value_set_uint(value, filter->n_noise_frames);
            break;
        case PROP_MAX_NOISE_TRACKED_FRAMES:
            g_value_set_uint(value, filter->max_noise_tracked_frames);
            break;
        case PROP_MAX_TRACKED_FRAMES:
            g_value_set_uint(value, filter->max_tracked_frames);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static gboolean
gst_optical_flow_tracker_set_caps(GstPad *pad, GstCaps *caps)
{
    GstOpticalFlowTracker *filter;
    GstPad            *other_pad;
    GstStructure      *structure;
    gint               width, height, depth;

    filter = GST_OPTICAL_FLOW_TRACKER(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth", &depth);

    filter->image        = cvCreateImage(cvSize(width, height), depth / 3, 3);
    filter->gray         = cvCreateImage(cvSize(width, height), depth / 3, 1);
    filter->prev_gray    = cvCreateImage(cvSize(width, height), depth / 3, 1);
    filter->pyramid      = cvCreateImage(cvSize(width, height), depth / 3, 1);
    filter->prev_pyramid = cvCreateImage(cvSize(width, height), depth / 3, 1);
    filter->flags        = 0;

    // set font scaling based on the frame area
    filter->font_scaling = ((filter->image->width * filter->image->height) > (320 * 240)) ? 0.5f : 0.3f;

    // add roi event probe on the sinkpad
    gst_pad_add_event_probe(filter->sinkpad, (GCallback) events_cb, filter);

    other_pad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);

    return gst_pad_set_caps(other_pad, caps);
}

// chain function; this function does the actual processing
static GstFlowReturn
gst_optical_flow_tracker_chain(GstPad *pad, GstBuffer *buf)
{
    GstOpticalFlowTracker *filter;
    IplImage          *bgr;
    GstClockTime       timestamp;
    gpointer           swap_pointer;
    guint              i;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_OPTICAL_FLOW_TRACKER(GST_OBJECT_PARENT(pad));
    filter->image->imageData = (char*) GST_BUFFER_DATA(buf);

    // Create the gray image for the surf 'features' search process
    cvCvtColor(filter->image, filter->gray, CV_RGB2GRAY);
    bgr = cvCreateImage(cvGetSize(filter->image), filter->image->depth, 3);
    cvCopy(filter->image, bgr, NULL);
    cvCvtColor(filter->image, bgr, CV_RGB2BGR);

    ++filter->n_frames;
    timestamp = GST_BUFFER_TIMESTAMP(buf);

    GST_DEBUG_OBJECT(filter, "------------------- frame %d ----------------------", filter->n_frames);

    // first, process all ROIs received from the 'haar adjust' element
    if ((filter->haar_roi_timestamp == timestamp) && (filter->haar_roi_array != NULL)) {
        GST_DEBUG_OBJECT(filter, "processing haar ROIs");
        for (i = 0; i < filter->haar_roi_array->len; ++i) {
            IplImage       *mask;
            InstanceObject *object;
            CvRect         *rect;
            float           max_area_overlap;
            gint            best_match_idx;
            guint           j;

            rect = &g_array_index(filter->haar_roi_array, CvRect, i);

            mask                  = gst_optical_flow_tracker_print_haar_mask(filter, rect);
            if ((filter->fg_mask != NULL) && (timestamp == filter->fg_mask_timestamp))
                cvAnd(mask, filter->fg_mask, mask, NULL);

            object                = g_new(InstanceObject, 1);
            object->rect          = *rect;
            object->timestamp     = timestamp;
            object->n_features    = filter->features_max_size;
            object->features      = g_new(CvPoint2D32f, object->n_features);
            object->prev_features = g_new(CvPoint2D32f, object->n_features);
            object->last_frame    = object->last_haar_frame = filter->n_frames;

            // select features
            cvGoodFeaturesToTrack(filter->gray, NULL, NULL, object->features, (int*) &object->n_features,
                                  filter->features_quality_level, filter->features_min_distance, mask,
                                  3, 0, 0.04); // 3, 0, 0.04 => opencv defaults

            cvFindCornerSubPix(filter->gray, object->features, object->n_features,
                               cvSize(filter->corner_subpix_win_size, filter->corner_subpix_win_size),
                               cvSize(-1, -1), cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS,
                                                              filter->term_criteria_iterations, filter->term_criteria_accuracy));

            object->haar_n_features = object->n_features;

            // now, search the known objects for the one with the largest overlapping
            // bounding rect (if the rect areas overlapping is above the
            // MIN_OVERLAP_AREA_PERC threshold)
            max_area_overlap = FLT_MIN;
            best_match_idx   = -1;
            for (j = 0; j < filter->stored_objects->len; ++j) {
                InstanceObject *other;
                float area_overlap;

                other = (InstanceObject*) g_ptr_array_index(filter->stored_objects, j);
                area_overlap = rect_area_overlap_perc(rect, &other->rect);
                if ((area_overlap > filter->min_overlap_area_perc) &&
                    (area_overlap > max_area_overlap)) {
                    max_area_overlap = area_overlap;
                    best_match_idx   = j;
                }
            }

            // if a matching object was found, replace it; otherwise, add a new one
            if (best_match_idx >= 0) {
                InstanceObject *other = (InstanceObject*) g_ptr_array_index(filter->stored_objects, best_match_idx);
                g_free(other->features);
                g_free(other->prev_features);
                object->id = other->id; // save id before overwriting the other fields
                *other = *object;
                object = other;
            } else {
                // add new object
                object->id            = filter->n_objects++;
                object->n_haar_frames = 0;
                g_ptr_array_add(filter->stored_objects, object);
            }

            object->n_haar_frames++;
        }
    }

    // apply rules to discard objects based on the last time a haar ROI was set:
    //
    // 1. objects that haven't been tagged by a haar ROI more than NUM_NOISE_FRAMES
    // are considered 'potential noise'. They should be discarded if more than
    // MAX_NOISE_TRACKED_FRAMES frames have ellapsed since the last time the
    // haar ROI was set
    //
    // 2. conversely, objects that haven been tagged by a haar ROI for
    // more than NOISE_FRAMES are considered actual objects. We should try
    // to track them for a longer period (the number of frames that may
    // ellapse since the last haar ROI is defined by MAX_TRACKED_FRAMES)
    for (i = 0; i < filter->stored_objects->len; ++i) {
        InstanceObject *object;
        guint           frames_since_haar, max_ellapsed_frames;

        object              = (InstanceObject*) g_ptr_array_index(filter->stored_objects, i);
        frames_since_haar   = object->last_frame - object->last_haar_frame;
        max_ellapsed_frames = (object->n_haar_frames <= filter->n_noise_frames) ? filter->max_noise_tracked_frames : filter->max_tracked_frames;

        GST_DEBUG_OBJECT(filter, "testing object %d: frames since haar (%d) >= max ellapsed frames (%d)\n", object->id, frames_since_haar, max_ellapsed_frames);

        if (frames_since_haar >= max_ellapsed_frames) {
            // discard object
            GST_INFO_OBJECT(filter, "removing object %d (frames since haar (%d) >= max ellapsed frames (%d))\n", object->id, frames_since_haar, max_ellapsed_frames);
            g_free(object->features);
            g_free(object->prev_features);
            g_ptr_array_remove_index(filter->stored_objects, i);
        }
    }

    // then, search for objects that haven't been associated with a haar ROI
    for (i = 0; i < filter->stored_objects->len; ++i) {
        InstanceObject *object;
        CvRect          rect;
        CvPoint         bounding_point1, bounding_point2;
        gchar          *status;
        guint           j, k;

        object = (InstanceObject*) g_ptr_array_index(filter->stored_objects, i);

        // skip objects created/updated on this very frame
        if (object->timestamp == timestamp)
            continue;

        status = g_new(gchar, object->n_features);

        cvCalcOpticalFlowPyrLK(filter->prev_gray, filter->gray,
                               filter->prev_pyramid, filter->pyramid,
                               object->prev_features,
                               object->features,
                               object->n_features,
                               cvSize(filter->corner_subpix_win_size, filter->corner_subpix_win_size),
                               filter->pyramid_levels,
                               status,
                               NULL,
                               cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, filter->term_criteria_iterations, filter->term_criteria_accuracy),
                               filter->flags);

        filter->flags |= CV_LKFLOW_PYR_A_READY;

        rect            = cvRect(filter->image->width, filter->image->height, 0, 0);
        bounding_point1 = cvPoint(MAX(0, object->rect.x * 0.95),
                                  MAX(0, object->rect.y * 0.95));
        bounding_point2 = cvPoint(MIN(filter->image->width, (object->rect.x + object->rect.width) * 1.05),
                                  MIN(filter->image->height, (object->rect.y + object->rect.height) * 1.05));

        for (j = k = 0; j < object->n_features; ++j) {
            CvPoint p;

            // skip features that could not be tracked
            if (status[j] == FALSE)
                continue;

            // skip features that lie outside the bounding perimeter (i.e.: 120% the original
            // bounding rectangle)
            p = cvPointFrom32f(object->features[j]);
            if (p.x < bounding_point1.x || p.y < bounding_point1.y || p.x > bounding_point2.x || p.y > bounding_point2.y)
                continue;

            object->features[k++] = object->features[j];

            // set coordinates of the new bounding rectangle
            if (p.x < rect.x) rect.x = p.x;
            if (p.y < rect.y) rect.y = p.y;
            if (p.x > (rect.x + rect.width))  rect.width = p.x - rect.x;
            if (p.y > (rect.y + rect.height)) rect.height = p.y - rect.y;
        }

        // update number of features and the bounding rectangle
        object->n_features = k;
        object->rect       = rect;
        object->last_frame = filter->n_frames;

        // if the percentage of the original features still tracked is below
        // MIN_MATCH_FEATURES_PERC, discard this object
        if (((float) object->n_features / object->haar_n_features) <= filter->min_match_features_perc) {
            GST_INFO_OBJECT(filter, "removing object %d (%d / %d (%.2f%%) features\n", object->id, object->n_features, object->haar_n_features, ((float) object->n_features / object->haar_n_features));
            g_free(object->features);
            g_free(object->prev_features);
            g_ptr_array_remove_index(filter->stored_objects, i);
            continue;
        }


        g_free(status);
    }

    // finally, generate the events for all the objects found in this frame
    for (i = 0; (filter->stored_objects != NULL) && (i < filter->stored_objects->len); ++i) {
        GstEvent       *event;
        GstMessage     *message;
        GstStructure   *structure;
        InstanceObject *object;
        CvRect          rect;
        gchar          *id_label;

        object = g_ptr_array_index(filter->stored_objects, i);
        rect   = object->rect;

        // skip objects not found on this frame
        if (object->last_frame != filter->n_frames)
            continue;

        GST_DEBUG_OBJECT(filter, "[object #%d rect] x: %d, y: %d, width: %d, height: %d\n", object->id, rect.x, rect.y, rect.width, rect.height);

        // send downstream event and bus message with the rect info
        id_label = g_strdup_printf("OBJ#%d", object->id);
        structure = gst_structure_new("object-roi",
                                      "id",        G_TYPE_STRING, id_label,
                                      "x",         G_TYPE_UINT,   rect.x,
                                      "y",         G_TYPE_UINT,   rect.y,
                                      "width",     G_TYPE_UINT,   rect.width,
                                      "height",    G_TYPE_UINT,   rect.height,
                                      "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP(buf),
                                      NULL);

        message = gst_message_new_element(GST_OBJECT(filter), gst_structure_copy(structure));
        gst_element_post_message(GST_ELEMENT(filter), message);
        event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
        gst_pad_push_event(filter->srcpad, event);

        g_free(id_label);

    }

    if (filter->display) {
        //// shade the mask background
        //if ((filter->fg_mask != NULL) && (timestamp == filter->fg_mask_timestamp)) {
        //    //cvAddS(filter->image,    CV_RGB( 64,  64,  64), filter->image,    filter->fg_mask);
        //    cvXorS(filter->fg_mask,  CV_RGB(255, 255, 255), filter->fg_mask,  NULL);
        //    cvSubS(filter->image,    CV_RGB(191, 191, 191), filter->image,    filter->fg_mask);
        //    cvXorS(filter->fg_mask,  CV_RGB(255, 255, 255), filter->fg_mask,  NULL);
        //}

        // print bounding rectangles and feature points of each object found on this frame on the
        // output image
        for (i = 0; (filter->stored_objects != NULL) && (i < filter->stored_objects->len); ++i) {
            InstanceObject *object;
            CvRect         *r;
            CvScalar        color;
            gchar          *label;
            guint           j;
            
            object = g_ptr_array_index(filter->stored_objects, i);
            if (object->last_frame != filter->n_frames)
                continue;

            // haar frames are highlighted with a darker blue
            color = object->last_haar_frame == filter->n_frames ? HAAR_COLOR : TRACKED_COLOR;
            label = g_strdup_printf("OBJ#%i", object->id);
            r     = &object->rect;

            cvRectangle(filter->image, cvPoint(r->x, r->y), cvPoint(r->x + r->width, r->y + r->height),
                        color, 1, 8, 0);

            // draw feature points
            for (j = 0; j < object->n_features; ++j)
                cvCircle(filter->image, cvPoint(object->features[j].x, object->features[j].y),
                         1, color, CV_FILLED, 8, 0);

            printText(filter->image, cvPoint(r->x + (r->width / 2), r->y + (r->height / 2)), label,
                      color, filter->font_scaling, 1);
        }

        if (filter->verbose) {
            // draw number of objects stored
            if ((filter->verbose) && (filter->display)) {
                gchar *label = g_strdup_printf("# objects: %3i", filter->stored_objects->len);
                printText(filter->image, cvPoint(0, 0), label, DEBUG_BOX_COLOR, filter->font_scaling, 1);
                g_free(label);
            }
        }
    }

    // cycle buffers
    for (i = 0; (filter->stored_objects != NULL) && (i < filter->stored_objects->len); ++i) {
        InstanceObject *object = g_ptr_array_index(filter->stored_objects, i);
        CV_SWAP(object->prev_features, object->features, swap_pointer);
    }
    CV_SWAP(filter->prev_gray,     filter->gray,     swap_pointer);
    CV_SWAP(filter->prev_pyramid,  filter->pyramid,  swap_pointer);

    gst_buffer_set_data(buf, (guint8*) filter->image->imageData, (guint) filter->image->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}

static IplImage*
gst_optical_flow_tracker_print_fg_mask(GstOpticalFlowTracker *filter)
{
    IplImage *mask;
    guint     i;

    mask = cvCreateImage(cvSize(filter->image->width, filter->image->height), filter->image->depth, 1);
    cvSet(mask, cvScalarAll(0), 0); // draw black mask
    for (i = 0; i < filter->fg_roi_array->len; ++i) {
        CvRect *r = &g_array_index(filter->fg_roi_array, CvRect, i);
        cvRectangle(mask, cvPoint(r->x, r->y), cvPoint(r->x + r->width, r->y + r->height),
                    cvScalarAll(255), CV_FILLED, 8, 0);
    }
    return mask;
}

static IplImage*
gst_optical_flow_tracker_print_haar_mask(GstOpticalFlowTracker *filter, CvRect *r)
{
    IplImage *mask;

    mask = cvCreateImage(cvSize(filter->image->width, filter->image->height), filter->image->depth, 1);
    cvSet(mask, cvScalarAll(0), 0); // draw black mask
    cvRectangle(mask,
                cvPoint(r->x, r->y),
                cvPoint(r->x + r->width, r->y + r->height),
                cvScalarAll(255), CV_FILLED, 8, 0);
    return mask;
}

static CvRect
rect_intersection(const CvRect *a, const CvRect *b)
{
    CvRect r = cvRect(MAX(a->x, b->x), MAX(a->y, b->y), 0, 0);
    r.width  = MIN(a->x + a->width,  b->x + b->width)  - r.x;
    r.height = MIN(a->y + a->height, b->y + b->height) - r.y;
    return r;
}

static float
rect_area_overlap_perc(const CvRect *a, const CvRect *b)
{
    CvRect rect;

    rect = rect_intersection(a, b);
    return ((a->height * a->width == .0f) ? .0f : (float) (rect.height * rect.width) / (a->height * a->width));
}

// callbacks
static gboolean
events_cb(GstPad *pad, GstEvent *event, gpointer user_data)
{
    GstOpticalFlowTracker  *filter;
    const GstStructure *structure;

    filter = GST_OPTICAL_FLOW_TRACKER(user_data);

    // sanity checks
    g_return_val_if_fail(pad    != NULL, FALSE);
    g_return_val_if_fail(event  != NULL, FALSE);
    g_return_val_if_fail(filter != NULL, FALSE);

    structure = gst_event_get_structure(event);

    if ((structure != NULL) && (strcmp(gst_structure_get_name(structure), "bgfg-mask") == 0)) {
        GArray      *data_array;
        GstClockTime timestamp;
        guint        width, height, depth, channels;

        gst_structure_get((GstStructure*) structure,
                          "data",      G_TYPE_POINTER, &data_array,
                          "width",     G_TYPE_UINT,    &width,
                          "height",    G_TYPE_UINT,    &height,
                          "depth",     G_TYPE_UINT,    &depth,
                          "channels",  G_TYPE_UINT,    &channels,
                          "timestamp", G_TYPE_UINT64,  &timestamp,
                          NULL);

        if (filter->fg_mask != NULL)
            cvReleaseImage(&filter->fg_mask);

        filter->fg_mask_timestamp = timestamp;
        filter->fg_mask = cvCreateImage(cvSize(width, height), depth, channels);
        memcpy(filter->fg_mask->imageData, data_array->data, data_array->len);
    }

    if ((structure != NULL) && (strcmp(gst_structure_get_name(structure), "haar-adjust-roi") == 0)) {
        CvRect rect;
        GstClockTime timestamp;

        gst_structure_get((GstStructure*) structure,
                          "x",         G_TYPE_UINT,   &rect.x,
                          "y",         G_TYPE_UINT,   &rect.y,
                          "width",     G_TYPE_UINT,   &rect.width,
                          "height",    G_TYPE_UINT,   &rect.height,
                          "timestamp", G_TYPE_UINT64, &timestamp,
                          NULL);

        if (timestamp > filter->haar_roi_timestamp) {
            filter->haar_roi_timestamp = timestamp;
            g_array_free(filter->haar_roi_array, TRUE);
            filter->haar_roi_array = g_array_sized_new(FALSE, FALSE, sizeof (CvRect), 1);
        }
        g_array_append_val(filter->haar_roi_array, rect);
    }

    if ((structure != NULL) && (strcmp(gst_structure_get_name(structure), "bgfg-roi") == 0)) {
        CvRect rect;
        GstClockTime timestamp;

        gst_structure_get((GstStructure*) structure,
                          "x", G_TYPE_UINT, &rect.x,
                          "y", G_TYPE_UINT, &rect.y,
                          "width", G_TYPE_UINT, &rect.width,
                          "height", G_TYPE_UINT, &rect.height,
                          "timestamp", G_TYPE_UINT64, &timestamp, NULL);

        if (timestamp > filter->fg_roi_timestamp) {
            filter->fg_roi_timestamp = timestamp;
            g_array_free(filter->fg_roi_array, TRUE);
            filter->fg_roi_array = g_array_sized_new(FALSE, FALSE, sizeof(CvRect), 4);
        }
        g_array_append_val(filter->fg_roi_array, rect);
    }

    return TRUE;
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features
gboolean
gst_optical_flow_tracker_plugin_init(GstPlugin * plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_optical_flow_tracker_debug, "optflowtracker", 0,
                            "Tracks objects using the 'Pyramid/Lucas Kanade' optical flow algorithm");

    return gst_element_register(plugin, "optflowtracker", GST_RANK_NONE, GST_TYPE_OPTICAL_FLOW_TRACKER);
}
