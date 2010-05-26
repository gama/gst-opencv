/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
 * Copyright (C) 2010 Erickson Range do Nascimento <erickson@vettalabs.com>
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
 * SECTION:element-tracker
 *
 * FIXME:Describe tracker here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace ! tracker ! ffmpegcolorspace ! xvimagesink
 * ]|
 * </refsect2>
 */

#define breitenstein_tracking_algorithm_implemented 1

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gsttracker.h"
#include "identifier.h"
#include "util.h"

#include <gst/gst.h>
#include <identifier_motion.h>
#include <condensation.h>


GST_DEBUG_CATEGORY_STATIC(gst_tracker_debug);
#define GST_CAT_DEFAULT gst_tracker_debug

// transition matrix F describes model parameters at and k and k+1
static const float F[] = { 1, 1, 0, 1 };

#define DEFAULT_MAX_POINTS         500
#define DEFAULT_MIN_POINTS          20
#define DEFAULT_WIN_SIZE            10
#define DEFAULT_MOVEMENT_THRESHOLD   2.0

#define DEFAULT_STATE_DIM           4
#define DEFAULT_MEASUREMENT_DIM     4
#define DEFAULT_SAMPLE_SIZE         50
#define DEFAULT_MAX_SAMPLE_SIZE     10*DEFAULT_SAMPLE_SIZE

#define DEFAULT_FRAMES_LEARN_BG         50
#define DEFAULT_MIN_FRAMES_TO_LEARN_BG  5
#define DEFAULT_MAX_FRAMES_TO_LEARN_BG  200

#define DEFAULT_BGMODEL_MODMIN_0     3
#define DEFAULT_BGMODEL_MODMIN_1     3
#define DEFAULT_BGMODEL_MODMIN_2     3
#define DEFAULT_BGMODEL_MODMAX_0     10
#define DEFAULT_BGMODEL_MODMAX_1     10
#define DEFAULT_BGMODEL_MODMAX_2     10
#define DEFAULT_BGMODEL_CBBOUNDS_0   10
#define DEFAULT_BGMODEL_CBBOUNDS_1   10
#define DEFAULT_BGMODEL_CBBOUNDS_2   10

enum {
    PROP_0,
    PROP_VERBOSE,
    PROP_MAX_POINTS,
    PROP_MIN_POINTS,
    PROP_WIN_SIZE,
    PROP_MOVEMENT_THRESHOLD,
    PROP_SHOW_PARTICLES,
    PROP_SHOW_FEATURES,
    PROP_SHOW_FEATURES_BOX,
    PROP_SHOW_BORDERS,
    PROP_SAMPLE_SIZE,
    PROP_FRAMES_LEARN_BG,
    PROP_TRACKER_BY_MOTION
};


/* the capabilities of the inputs and outputs.
*/
static GstStaticPadTemplate sink_factory =
    GST_STATIC_PAD_TEMPLATE("sink",
                            GST_PAD_SINK,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS("video/x-raw-rgb"));

static GstStaticPadTemplate src_factory =
    GST_STATIC_PAD_TEMPLATE("src",
                            GST_PAD_SRC,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS("video/x-raw-rgb"));

GST_BOILERPLATE(GstTracker, gst_tracker, GstElement, GST_TYPE_ELEMENT);

// function prototypes
static void          gst_tracker_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_tracker_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean      gst_tracker_set_caps     (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_tracker_chain        (GstPad * pad, GstBuffer * buf);
static gboolean      gst_tracker_events_cb    (GstPad *pad, GstEvent *event, gpointer user_data);

// clean up
static void
gst_tracker_finalize(GObject * obj)
{
    GstTracker *filter = GST_TRACKER(obj);

    if (filter->image)        cvReleaseImage(&filter->image);
    if (filter->grey)         cvReleaseImage(&filter->image);
    if (filter->prev_grey)    cvReleaseImage(&filter->image);
    if (filter->pyramid)      cvReleaseImage(&filter->image);
    if (filter->prev_pyramid) cvReleaseImage(&filter->image);
    if (filter->points[0])    cvFree(&filter->points[0]);
    if (filter->points[1])    cvFree(&filter->points[1]);
    if (filter->status)       cvFree(&filter->status);
    if (filter->verbose)      g_print("\n");

    if (filter->background)         cvReleaseImage(&filter->background);
    if (filter->backgroundModel)    cvReleaseBGCodeBookModel(&filter->backgroundModel);

    if (filter->cvMotion)         cvReleaseImage (&filter->cvMotion);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}


/* GObject vmethod implementations */
static void
gst_tracker_base_init(gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "tracker",
                                         "Filter/Effect/Video",
                                         "Track the motion of objects of a scene",
                                         "Erickson Rangel do Nascimento <erickson@vettalabs.com>");

    gst_element_class_add_pad_template(element_class,
                                        gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class,
                                        gst_static_pad_template_get(&sink_factory));
}

/* initialize the tracker's class */
static void
gst_tracker_class_init(GstTrackerClass * klass)
{
    GObjectClass *gobject_class = (GObjectClass *) klass;

    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_tracker_finalize);
    gobject_class->set_property = gst_tracker_set_property;
    gobject_class->get_property = gst_tracker_get_property;

    g_object_class_install_property(gobject_class, PROP_TRACKER_BY_MOTION,
                                    g_param_spec_boolean("tracker-by-motion", "Tracker by motion", "Amendment application to track moving object, instead of subtracting background.",
                                                         TRUE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose", "Sets whether the movement direction should be printed to the standard output.",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SHOW_PARTICLES,
                                    g_param_spec_boolean("show-particles", "Show particles", "Sets whether particles location should be printed to the video.",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SHOW_FEATURES,
                                    g_param_spec_boolean("show-features", "Show features", "Sets whether features location should be printed to the video.",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SHOW_FEATURES_BOX,
                                    g_param_spec_boolean("show-features-box", "Show features box", "Sets whether features box should be printed to the video.",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SHOW_BORDERS,
                                    g_param_spec_boolean("show-borders", "Show borders in features box", "Sets whether borders in features box should be printed to the video.",
                                                         TRUE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MAX_POINTS,
                                    g_param_spec_uint("max-points", "Max points", "Maximum number of feature points.",
                                                      0, 2 * DEFAULT_MAX_POINTS, DEFAULT_MAX_POINTS, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MIN_POINTS,
                                    g_param_spec_uint("min-points", "Min points", "Minimum number of feature points accepted. If the number of points falls belows this threshold, another feature-selection is attempted",
                                                      0, DEFAULT_MAX_POINTS, DEFAULT_MIN_POINTS, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_WIN_SIZE,
                                    g_param_spec_uint("win-size", "Window size", "Size of the corner-subpixels window.",
                                                      0, 2 * DEFAULT_WIN_SIZE, DEFAULT_WIN_SIZE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MOVEMENT_THRESHOLD,
                                    g_param_spec_float("movement-threshold", "Movement threshold", "Threshold that defines what constitutes a left (< -THRESHOLD) or right (> THRESHOLD) movement (in average # of pixels).",
                                                       0.0, 20 * DEFAULT_MOVEMENT_THRESHOLD, DEFAULT_MOVEMENT_THRESHOLD, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SAMPLE_SIZE,
                                    g_param_spec_uint("sample-size", "Sample size", "Number of particles used in Condensation", 0, DEFAULT_MAX_SAMPLE_SIZE, DEFAULT_SAMPLE_SIZE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_FRAMES_LEARN_BG,
                                    g_param_spec_uint("frames-learn-bg", "Number of frames to learn bg", "Number of frames used to learn the backgound", DEFAULT_MIN_FRAMES_TO_LEARN_BG, DEFAULT_MAX_FRAMES_TO_LEARN_BG, DEFAULT_FRAMES_LEARN_BG, G_PARAM_READWRITE));


}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_tracker_init(GstTracker * filter, GstTrackerClass * gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad,
                                 GST_DEBUG_FUNCPTR(gst_tracker_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad,
                                 GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad,
                               GST_DEBUG_FUNCPTR(gst_tracker_chain));

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad,
                                 GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    // set default properties
    filter->verbose            = FALSE;
    filter->show_particles     = FALSE;
    filter->show_features      = FALSE;
    filter->show_features_box  = FALSE;
    filter->show_borders       = TRUE;
    filter->max_points         = DEFAULT_MAX_POINTS;
    filter->min_points         = DEFAULT_MIN_POINTS;
    filter->win_size           = DEFAULT_WIN_SIZE;
    filter->movement_threshold = DEFAULT_MOVEMENT_THRESHOLD;
    filter->tracker_by_motion  = TRUE;

    filter->state_dim          = DEFAULT_STATE_DIM;
    filter->measurement_dim    = DEFAULT_MEASUREMENT_DIM;
    filter->sample_size        = DEFAULT_SAMPLE_SIZE;

    filter->nframesToLearnBG   = DEFAULT_FRAMES_LEARN_BG;
    filter->framesProcessed    = 0;

    filter->backgroundModel = cvCreateBGCodeBookModel();
    filter->backgroundModel->modMin[0]      = DEFAULT_BGMODEL_MODMIN_0;
    filter->backgroundModel->modMax[0]      = DEFAULT_BGMODEL_MODMAX_0;
    filter->backgroundModel->modMin[1]      = DEFAULT_BGMODEL_MODMIN_1;
    filter->backgroundModel->modMax[1]      = DEFAULT_BGMODEL_MODMAX_1;
    filter->backgroundModel->modMin[2]      = DEFAULT_BGMODEL_MODMIN_2;
    filter->backgroundModel->modMax[2]      = DEFAULT_BGMODEL_MODMAX_2;
    filter->backgroundModel->cbBounds[0]    = DEFAULT_BGMODEL_CBBOUNDS_0;
    filter->backgroundModel->cbBounds[1]    = DEFAULT_BGMODEL_CBBOUNDS_1;
    filter->backgroundModel->cbBounds[2]    = DEFAULT_BGMODEL_CBBOUNDS_2;

    filter->detect_timestamp = 0;
    filter->confidence_density_timestamp = 0;

}

static void
gst_tracker_set_property(GObject *object, guint prop_id,
                               const GValue *value, GParamSpec *pspec)
{
    GstTracker *filter = GST_TRACKER(object);

    switch (prop_id) {
        case PROP_TRACKER_BY_MOTION:
            filter->tracker_by_motion = g_value_get_boolean(value);
            break;
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_MAX_POINTS:
            filter->max_points = g_value_get_uint(value);
            break;
        case PROP_MIN_POINTS:
            filter->min_points = g_value_get_uint(value);
            break;
        case PROP_WIN_SIZE:
            filter->win_size = g_value_get_uint(value);
            break;
        case PROP_MOVEMENT_THRESHOLD:
            filter->win_size = g_value_get_float(value);
            break;
        case PROP_SHOW_PARTICLES:
            filter->show_particles = g_value_get_boolean(value);
            break;
        case PROP_SHOW_FEATURES:
            filter->show_features = g_value_get_boolean(value);
            break;
        case PROP_SHOW_FEATURES_BOX:
            filter->show_features_box = g_value_get_boolean(value);
            break;
        case PROP_SHOW_BORDERS:
            filter->show_borders = g_value_get_boolean(value);
            break;
        case PROP_SAMPLE_SIZE:
            filter->sample_size = g_value_get_uint(value);
            break;
         case PROP_FRAMES_LEARN_BG:
            filter->nframesToLearnBG = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_tracker_get_property(GObject * object, guint prop_id,
                               GValue * value, GParamSpec * pspec)
{
    GstTracker *filter = GST_TRACKER(object);

    switch (prop_id) {
        case PROP_TRACKER_BY_MOTION:
            g_value_set_boolean(value, filter->tracker_by_motion);
            break;
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_MAX_POINTS:
            g_value_set_uint(value, filter->max_points);
            break;
        case PROP_MIN_POINTS:
            g_value_set_uint(value, filter->min_points);
            break;
        case PROP_WIN_SIZE:
            g_value_set_uint(value, filter->win_size);
            break;
        case PROP_MOVEMENT_THRESHOLD:
            g_value_set_float(value, filter->movement_threshold);
            break;
        case PROP_SHOW_PARTICLES:
            g_value_set_boolean(value, filter->show_particles);
            break;
        case PROP_SHOW_FEATURES:
            g_value_set_boolean(value, filter->show_features);
            break;
        case PROP_SHOW_FEATURES_BOX:
            g_value_set_boolean(value, filter->show_features_box);
            break;
        case PROP_SHOW_BORDERS:
            g_value_set_boolean(value, filter->show_borders);
            break;
        case PROP_SAMPLE_SIZE:
            g_value_set_uint(value, filter->sample_size);
            break;
         case PROP_FRAMES_LEARN_BG:
            g_value_set_uint(value, filter->nframesToLearnBG);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_tracker_set_caps(GstPad * pad, GstCaps * caps)
{
    GstTracker *filter;
    GstPad *otherpad;
    gint width, height;
    GstStructure *structure;

    filter = GST_TRACKER(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    // initialize opencv data structures
    filter->width_image   = width;
    filter->height_image  = height;
    filter->image         = cvCreateImage(cvSize(width, height), 8, 3);
    filter->background    = cvCreateImage(cvSize(width, height), 8, 3);
    filter->grey          = cvCreateImage(cvSize(width, height), 8, 1);
    filter->prev_grey     = cvCreateImage(cvSize(width, height), 8, 1);
    filter->pyramid       = cvCreateImage(cvSize(width, height), 8, 1);
    filter->prev_pyramid  = cvCreateImage(cvSize(width, height), 8, 1);
    filter->cvMotion      = cvCreateImage(cvSize(width, height), 8, 1);
    filter->points[0]     = (CvPoint2D32f*) cvAlloc(filter->max_points * sizeof(filter->points[0][0]));
    filter->points[1]     = (CvPoint2D32f*) cvAlloc(filter->max_points * sizeof(filter->points[0][0]));
    filter->status        = (char*) cvAlloc(filter->max_points);
    filter->flags         = 0;
    filter->initialized   = FALSE;

    filter->ConDens = initCondensation(filter->state_dim, filter->measurement_dim, filter->sample_size, filter->width_image, filter->height_image);

    // add event probe to capture detection rectangles and confidence density
    // sent by the upstream hog detect element
    gst_pad_add_event_probe(filter->sinkpad, (GCallback) gst_tracker_events_cb, filter);

    otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);

    return gst_pad_set_caps(otherpad, caps);
}


static GSList* has_intersection(CvRect *obj, GSList *objects);
static void associate_detected_obj_to_tracker(IplImage *image, GSList *detected_objects, GSList *trackers, GSList **unassociated_objects);
static Tracker* closer_tracker_with_a_detected_obj_to(Tracker* tracker, GSList* trackers);
void print_tracker(Tracker *tracker, IplImage *image, gint id_tracker);


typedef struct {
    CvRect region;
    guint count;
} unassociated_obj_t;

float dist_calc(CvPoint p1, CvPoint p2)
{
    return sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
}

float dist_point_to_segment(float x, float y, float x1, float y1, float x2, float y2)
{
    float A = x - x1;
    float B = y - y1;
    float C = x2 - x1;
    float D = y2 - y1;

    float dot = A * C + B * D;
    float len_sq = C * C + D * D;
    float param = dot / len_sq;

    float xx, yy;

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

// FIXME implement the greedy algorithm
void associate_detected_obj_to_tracker(IplImage *image, GSList *detected_objects, GSList *trackers, GSList **unassociated_objects)
{
    Tracker *closer_tracker, *tracker;
    gfloat min_dist, dist;
    CvRect *detected_obj;
    GSList *it_detected_obj, *it_tracker;
    gfloat dist_threshold;
    gfloat ratio, old_area, new_area;

    // FIXME: select other threshold
    dist_threshold = 200.0;

    min_dist = G_MAXFLOAT;
    closer_tracker = NULL;
    *unassociated_objects = NULL;


    GST_INFO("detected_objects: %d\n", g_slist_length(detected_objects));
    for (it_detected_obj = detected_objects; it_detected_obj; it_detected_obj = it_detected_obj->next) {
        detected_obj = (CvRect*)it_detected_obj->data;

        for (it_tracker = trackers; it_tracker; it_tracker = it_tracker->next) 
        {
            tracker = (Tracker*)it_tracker->data;
            if (tracker != NULL)
            {

                #if 0
                //-------------------

                gfloat part_a, part_b, part_c;
                CvPoint rect_centroid_d = rect_centroid(detected_obj);


                // PART A
                {
                    gfloat area_tr = tracker->tracker_area.height * tracker->tracker_area.width;
                    gfloat area_d = detected_obj->height * detected_obj->width;
                    CvPoint rect_centroid_tr = rect_centroid(&tracker->tracker_area);

                    // mean = 0, variance = 0.4
                    part_a = gaussian_function((gfloat) ((area_tr - area_d) / area_tr), 0, 0.4);

                    gfloat dist_temp = abs(dist_calc(rect_centroid_tr, rect_centroid_d));

                    const gfloat threshold_dist = 20;
                    if (dist_temp > threshold_dist)
                        dist_temp = dist_point_to_segment(rect_centroid_d.x, rect_centroid_d.y, rect_centroid_tr.x, rect_centroid_tr.y,
                            tracker->previous_centroid.x, tracker->previous_centroid.y);

                    // mean = 25, variance = 5
                    part_a *= gaussian_function(dist_temp, 25, 5);
                }


                // PART B
                part_b = classifier_intermediate_classify(tracker->classifier, image, tracker->tracker_area);


                // PART C
                {
                    part_c = 0;
                    int i;
                    gfloat particles_dist_to_d;
                    for (i = 0; i < tracker->filter->SamplesNum; ++i) {
                        particles_dist_to_d = abs(dist_calc(rect_centroid_d, cvPoint(tracker->filter->flSamples[i][0], tracker->filter->flSamples[i][1])));
                        part_c += gaussian_function(particles_dist_to_d, 110, 57);
                    }
                }


                gfloat result = part_a * (part_b*10 + part_c);
                printf("A:%5.3f B:%5.3f C:%5.3f RESULT:%5.3f\n", part_a, part_b, part_c, result);

                //---------------------------
                #endif

                dist = euclidian_distance(rect_centroid(detected_obj), cvPoint(tracker->filter->State[0], tracker->filter->State[1]));
                GST_INFO("dist: %f", dist);
                if (dist < min_dist)
                {
                    min_dist = dist;
                    closer_tracker = tracker;
                }
            }
        }
        if (min_dist <= dist_threshold && closer_tracker != NULL){
            GST_INFO("tracker found a detected_obj");
            closer_tracker->detected_object = detected_obj;
            //FIXME: tracker area update
            /*
            new_area = (float)detected_obj->width * detected_obj->height;
            old_area = (float)closer_tracker->tracker_area.width * closer_tracker->tracker_area.height;
            ratio = abs((new_area-old_area)/old_area);

            if ( ratio <= 0.05 ){
                closer_tracker->tracker_area.width = detected_obj->width;
                closer_tracker->tracker_area.height = detected_obj->height;
                }
            */
        }
        else if (detected_obj != NULL)
        {
            GST_INFO("adding CvRect(%d, %d, %d, %d) at unassociated_objects",
                                detected_obj->x, detected_obj->y, detected_obj->width,
                                detected_obj->height); *unassociated_objects =
            g_slist_prepend(*unassociated_objects, detected_obj);
        }
    }
}

static gfloat max(gfloat n1, gfloat n2)
{
    return n1 > n2 ? n1 : n2;
}

static gfloat min(gfloat n1, gfloat n2)
{
    return n1 < n2 ? n1 : n2;
}


/* search and return the closer
 * region with intersection of obj */
GSList*
has_intersection(CvRect *obj, GSList *objects)
{
    gfloat dist, min_dist;
    GSList* correspondent_obj = NULL;
    CvRect rect;
    CvPoint center;
    GSList *it_obj;
    gfloat left, top, right, bottom;

    min_dist = G_MAXFLOAT;
    center = rect_centroid(obj);

    GST_INFO("num of objects: %d", g_slist_length(objects));
    for (it_obj = objects; it_obj; it_obj = it_obj->next) {

        rect = ((unassociated_obj_t*)it_obj->data)->region;

        left   = max(obj->x, rect.x);
        top    = max(obj->y, rect.y);
        right  = min(obj->x + obj->width, rect.x + rect.width );
        bottom = min(obj->y + obj->height, rect.y + rect.height);

        if ( right > left && bottom > top )
        {
            dist = euclidian_distance(center, rect_centroid(&rect));
            if (dist < min_dist)
            {
                correspondent_obj = it_obj->data;
                min_dist = dist;
            }
        }

    }
    return correspondent_obj;
}

Tracker*
closer_tracker_with_a_detected_obj_to(Tracker* tracker, GSList* trackers)
{
    gfloat dist, min_dist;
    Tracker *tr, *closer_tracker;
    GSList *it_tracker;

    min_dist = G_MAXFLOAT;
    for (it_tracker = trackers; it_tracker; it_tracker = it_tracker->next)
    {
        tr = (Tracker*)it_tracker->data;

        if (tr->detected_object != NULL){
            dist = euclidian_distance(  cvPoint(tracker->filter->State[0], tracker->filter->State[1]), 
                                        cvPoint(tr->filter->State[0], tr->filter->State[1]));
            if (dist < min_dist){
                closer_tracker = tr;
                min_dist = dist;
            }

        }
    }
    return closer_tracker;
}

void 
distribution_test(CvRect rect, IplImage *image)
{
    int             i, pointCount;
    CvRNG           rng_state;
    CvPoint         p;
    gfloat          horizontal_sigma, vertical_sigma;
    float           center_x,center_y;
    CvMat           *locations;
    
    pointCount = 100;

    rng_state = cvRNG(0xffffffff);
    cvRectangle(    image, 
                    cvPoint(rect.x, rect.y), 
                    cvPoint(rect.x+ rect.width, 
                            rect.y+ rect.height), 
                    CV_RGB(255, 255,0), 1, 8, 0);

    center_x = (float)(rect.width)/2.0 + rect.x;
    center_y = (float)(rect.height)/2.0 + rect.y;

    horizontal_sigma = rect.width/2.0 / 3.0; 
    vertical_sigma   = rect.height/2.0 / 3.0;

    locations = cvCreateMat(pointCount, 1, CV_32SC2);

    #if 0
    cvRandArr(&rng_state, locations, CV_RAND_UNI, cvScalar(rect.x,rect.y,0,0),
               cvScalar(rect.x+rect.width, rect.y+rect.height,0,0));

    #else
    cvRandArr(&rng_state, locations, CV_RAND_NORMAL,
               cvScalar(center_x, center_y, 0, 0), // average intensity
               cvScalar(horizontal_sigma, 
                        vertical_sigma, 0, 0) // deviation of the intensity
              );
    #endif

    for (i = 0; i < pointCount; ++i) {
        p = *(CvPoint*)cvPtr1D(locations, i, 0);
        cvCircle(image, p, 3, CV_RGB(0, 255,0), 1, 8, 0);
    }   

    cvReleaseMat(&locations);
}

void print_tracker(Tracker *tracker, IplImage *image, gint id_tracker)
{
    int i;

    for (i = 0; i < tracker->filter->SamplesNum; i++) {
        if (tracker->filter->flConfidence[i] >= 0)
            cvCircle(image, cvPoint(tracker->filter->flSamples[i][0], tracker->filter->flSamples[i][1]),
                            1*tracker->filter->flConfidence[i], 
                            CV_RGB(0, (1-1.0/id_tracker)*255, (1.0/id_tracker)*255), 1, 8, 0);
    }

    cvRectangle(image,  cvPoint(tracker->tracker_area.x, tracker->tracker_area.y), 
                        cvPoint(    tracker->tracker_area.x + tracker->tracker_area.width, 
                                    tracker->tracker_area.y + tracker->tracker_area.height),
                        CV_RGB(0, (1-1.0/id_tracker)*255, (1.0/id_tracker)*255), 2, 8, 0);

    cvCircle(image, cvPoint(tracker->filter->State[0], tracker->filter->State[1]),
                        10, CV_RGB((1-1.0/id_tracker)*255, 0, (1.0/id_tracker)*255), -1, 8, 0);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_tracker_chain(GstPad *pad, GstBuffer *buf)
{
    GstTracker *filter;
    IplImage *swap_temp;
    CvPoint2D32f *swap_points;
    gint id_tracker;
    float avg_x = 0.0;
    IplImage *image;


    GSList *unassociated_objects = NULL;
    unassociated_obj_t *unassociated_obj = NULL;

    GSList *it_obj = NULL;
    GSList *it_tracker = NULL;
    GSList *intersection_last_frame = NULL;

    // select a better threshold
    guint num_subsequent_detections = 2;

    Tracker *new_tracker = NULL;
    Tracker *tracker = NULL;
    Tracker *closer_tracker = NULL;

    // choose better values
    gfloat beta = 0.2;
    gfloat gamma = 0.7;
    gfloat mi   = 0.1;


    guint  num_particles = 100;

    filter = GST_TRACKER(GST_OBJECT_PARENT(pad));
    filter->image->imageData = (char *) GST_BUFFER_DATA(buf);

    cvCvtColor(filter->image, filter->grey, CV_BGR2GRAY);
    image = cvCloneImage(filter->image);


    if (filter->detect_timestamp == GST_BUFFER_TIMESTAMP(buf) && filter->confidence_density_timestamp == GST_BUFFER_TIMESTAMP(buf))
    {

        GST_INFO("detected_objects: %d", g_slist_length(filter->detected_objects));
        // data association
        associate_detected_obj_to_tracker(image, filter->detected_objects, filter->trackers, &unassociated_objects);
        GST_INFO("unassociated_objects: %d", g_slist_length(unassociated_objects));

        // creating tracker for new detected object
        for (it_obj = unassociated_objects; it_obj; it_obj = it_obj->next) {
            intersection_last_frame = has_intersection((CvRect*)it_obj->data, filter->unassociated_objects_last_frame);
            if (intersection_last_frame != NULL) 
            {
                unassociated_obj = ((unassociated_obj_t*)intersection_last_frame);
                unassociated_obj->count++;
                GST_INFO("total of intersection: %d", unassociated_obj->count);
                unassociated_obj->region = *((CvRect*)it_obj->data); // update the region

                //distribution_test(unassociated_obj->region, filter->image);

                if (unassociated_obj->count >= num_subsequent_detections) {
                    new_tracker = tracker_new( &unassociated_obj->region, 4, 4,
                                                num_particles,
                                                image,
                                                beta, gamma, mi );
                    filter->trackers = g_slist_prepend(filter->trackers, new_tracker);

                    filter->unassociated_objects_last_frame = g_slist_remove(filter->unassociated_objects_last_frame, intersection_last_frame);
                    g_free(unassociated_obj);
                }
            } else {
                unassociated_obj = g_new(unassociated_obj_t, 1);
                unassociated_obj->region = *((CvRect*)it_obj->data);
                unassociated_obj->count = 0;
                filter->unassociated_objects_last_frame = g_slist_prepend( filter->unassociated_objects_last_frame, unassociated_obj );
            }
        }

        //TODO: review unassociated_objects list memory manage
        g_slist_free(unassociated_objects);
    }

    // tracking
    GST_INFO("trackers: %d\n", g_slist_length(filter->trackers));
    id_tracker = 1;
    for (it_tracker = filter->trackers; it_tracker; it_tracker = it_tracker->next) {
        //GST_INFO("running tracker.");
        tracker = (Tracker*)it_tracker->data;

        closer_tracker = closer_tracker_with_a_detected_obj_to( tracker, filter->trackers );
        tracker_run(tracker, closer_tracker, &filter->confidence_density, image );
        print_tracker(tracker, filter->image, it_tracker);
        id_tracker++;
    }



    filter->prev_avg_x = avg_x;
    CV_SWAP(filter->prev_grey, filter->grey, swap_temp);
    CV_SWAP(filter->prev_pyramid, filter->pyramid, swap_temp);
    CV_SWAP(filter->points[0], filter->points[1], swap_points);
    gst_buffer_set_data(buf, (guint8*) filter->image->imageData, (guint) filter->image->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}

// callbacks
static
gboolean gst_tracker_events_cb(GstPad *pad, GstEvent *event, gpointer user_data)
{
    GstTracker          *filter;
    const GstStructure  *structure;

    filter = GST_TRACKER(user_data);

    // sanity checks
    g_return_val_if_fail(pad    != NULL, FALSE);
    g_return_val_if_fail(event  != NULL, FALSE);
    g_return_val_if_fail(filter != NULL, FALSE);

    structure = gst_event_get_structure(event);
    if (structure == NULL)
        return TRUE;

    if (strcmp(gst_structure_get_name(structure), "hog-confidence-density") == 0) {
        CvMat       *confidence_density;
        GstClockTime timestamp;

        gst_structure_get((GstStructure*) structure,
                          "matrix",    G_TYPE_POINTER, &confidence_density,
                          "timestamp", G_TYPE_UINT64,  &timestamp, NULL);

        if (timestamp > filter->confidence_density_timestamp) {
            filter->confidence_density_timestamp = timestamp;
            cvInitMatHeader(&filter->confidence_density, confidence_density->rows, confidence_density->cols,
                            confidence_density->type, confidence_density->data.ptr, confidence_density->step);
            // increment ref. counter so that the data ptr won't be deallocated
            // once the upstream element releases the matrix header
            cvIncRefData(&filter->confidence_density);
        }
    }

    if (strcmp(gst_structure_get_name(structure), "hog-detect-roi") == 0) {
        CvRect       *detect_rect;
        GstClockTime timestamp;
        GSList       *it_list;


        detect_rect = g_new(CvRect,1);
        gst_structure_get((GstStructure*) structure,
                          "x",         G_TYPE_UINT,   &detect_rect->x,
                          "y",         G_TYPE_UINT,   &detect_rect->y,
                          "width",     G_TYPE_UINT,   &detect_rect->width,
                          "height",    G_TYPE_UINT,   &detect_rect->height,
                          "timestamp", G_TYPE_UINT64, &timestamp, NULL);

        if (timestamp > filter->detect_timestamp) {
            filter->detect_timestamp = timestamp;
            for(it_list = filter->detected_objects; it_list; it_list = it_list->next)
                g_free(it_list->data);

            g_slist_free(filter->detected_objects);
            filter->detected_objects = NULL;
        }
        filter->detected_objects = g_slist_prepend(filter->detected_objects, detect_rect);
    }

    return TRUE;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
gboolean
gst_tracker_plugin_init(GstPlugin * plugin)
{
    /* debug category for fltering log messages */
    GST_DEBUG_CATEGORY_INIT(gst_tracker_debug, "tracker", 0, "Track the motion of objects of a scene");
    return gst_element_register(plugin, "tracker", GST_RANK_NONE, GST_TYPE_TRACKER);
}
