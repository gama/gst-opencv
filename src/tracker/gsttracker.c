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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gsttracker.h"
#include "util.h"

GST_DEBUG_CATEGORY_STATIC(gst_tracker_debug);

// New tracker
#define TRACKER_NUM_PARTICLES               100
#define TRACKER_NUM_SUBSEQUENT_DETECTIONS   2

// Remove tracker
#define FRAMES_TO_LAST_DETECTING_REM        6
#define FRAMES_OF_WRONG_CLASSIFIER_REM      6

// Tracker weights
#define DEFAULT_DETECTION_PARAMETER         1.0
#define DEFAULT_DET_CONFIDENCE_PARAMETER    0.50
#define DEFAULT_CLASSIFIER_PARAMETER        0.25

// Greedy algorithm
#define MIN_GREEDY_CONSIDER_PAIR            0.4f

enum {
    PROP_0,
    PROP_VERBOSE,
    PROP_SHOW_PARTICLES,
    PROP_DETECTION_PARAMETER,
    PROP_DET_CONFIDENCE_PARAMETER,
    PROP_CLASSIFIER_PARAMETER,
    PROP_SHOW_FEATURES_BOX
};

typedef struct {
    CvRect  region;
    guint   count;
} unassociated_obj_t;

/* the capabilities of the inputs and outputs.*/
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
static void          gst_tracker_set_property               (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_tracker_get_property               (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean      gst_tracker_set_caps                   (GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_tracker_chain                      (GstPad *pad, GstBuffer *buf);
static gboolean      gst_tracker_events_cb                  (GstPad *pad, GstEvent *event, gpointer user_data);
static GSList*       has_intersection                       (CvRect *obj, GSList *objects);
static void          associate_detected_obj_to_tracker      (IplImage *image, GSList *detected_objects, GSList *trackers, GSList **unassociated_objects);
static Tracker*      closer_tracker_with_a_detected_obj_to  (Tracker *tracker, GSList *trackers);
void                 print_tracker                          (Tracker *tracker, IplImage *image, gint id_tracker, gboolean show_particles);
static void          remove_old_trackers                    (IplImage *image, GSList **trackers);
void                 distribution_test                      (CvRect rect, IplImage *image);

// clean up
static void
gst_tracker_finalize(GObject * obj)
{
    GstTracker *filter = GST_TRACKER(obj);

    if (filter->image)        cvReleaseImage(&filter->image);
    if (filter->verbose)      g_print("\n");

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

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose", "Sets whether the movement direction should be printed to the standard output.",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SHOW_PARTICLES,
                                    g_param_spec_boolean("show-particles", "Show particles", "Sets whether particles location should be printed to the video.",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SHOW_FEATURES_BOX,
                                    g_param_spec_boolean("show-features-box", "Show features box", "Sets whether features box should be printed to the video.",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DETECTION_PARAMETER,
                                    g_param_spec_float("detection-influence", "Detection Parameter (beta)", "How much hog detector influences in particle update (beta parameter in Breitenstein paper).",
                                                       0.0, 20 * DEFAULT_DETECTION_PARAMETER, DEFAULT_DETECTION_PARAMETER, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DET_CONFIDENCE_PARAMETER,
                                    g_param_spec_float("confidence-influence", "Detector Confidence Parameter (gamma)", "How much confidence map of hog detectior influences in particle update (gamma parameter in Breitenstein paper).",
                                                       0.0, 20 * DEFAULT_DET_CONFIDENCE_PARAMETER, DEFAULT_DET_CONFIDENCE_PARAMETER, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CLASSIFIER_PARAMETER,
                                    g_param_spec_float("classifier-influence", "Classifier Parameter (eta)", "How much classifier influences in particle update (eta parameter in Breitenstein paper).",
                                                       0.0, 20 * DEFAULT_CLASSIFIER_PARAMETER, DEFAULT_CLASSIFIER_PARAMETER, G_PARAM_READWRITE));
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
    filter->verbose                      = FALSE;
    filter->show_particles               = FALSE;
    filter->show_features_box            = FALSE;
    filter->beta                         = DEFAULT_DETECTION_PARAMETER;
    filter->gamma                        = DEFAULT_DET_CONFIDENCE_PARAMETER;
    filter->eta                          = DEFAULT_CLASSIFIER_PARAMETER;
    filter->detect_timestamp             = 0;
    filter->confidence_density_timestamp = 0;
}

static void
gst_tracker_set_property(GObject *object, guint prop_id,
                               const GValue *value, GParamSpec *pspec)
{
    GstTracker *filter = GST_TRACKER(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_DETECTION_PARAMETER:
            filter->beta = g_value_get_float(value);
            break;
        case PROP_DET_CONFIDENCE_PARAMETER:
            filter->gamma = g_value_get_float(value);
            break;
        case PROP_CLASSIFIER_PARAMETER:
            filter->eta = g_value_get_float(value);
            break;
        case PROP_SHOW_PARTICLES:
            filter->show_particles = g_value_get_boolean(value);
            break;
        case PROP_SHOW_FEATURES_BOX:
            filter->show_features_box = g_value_get_boolean(value);
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
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_DETECTION_PARAMETER:
            g_value_set_float(value, filter->beta);
            break;
        case PROP_DET_CONFIDENCE_PARAMETER:
            g_value_set_float(value, filter->gamma);
            break;
        case PROP_CLASSIFIER_PARAMETER:
            g_value_set_float(value, filter->eta);
            break;
        case PROP_SHOW_PARTICLES:
            g_value_set_boolean(value, filter->show_particles);
            break;
        case PROP_SHOW_FEATURES_BOX:
            g_value_set_boolean(value, filter->show_features_box);
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

    filter->image = cvCreateImage(cvSize(width, height), 8, 3);

    // add event probe to capture detection rectangles and confidence density
    // sent by the upstream hog detect element
    gst_pad_add_event_probe(filter->sinkpad, (GCallback) gst_tracker_events_cb, filter);

    otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);

    return gst_pad_set_caps(otherpad, caps);
}

/* The greedy algorithm */
static void
associate_detected_obj_to_tracker(IplImage *image, GSList *detected_objects, GSList *trackers, GSList **unassociated_objects)
{
    GSList      *it_detected_obj;
    GSList      *it_tracker;
    Tracker     *tracker;
    CvRect      *detected_obj;
    guint       it_d, it_tr, it_dxtr, size_tr, size_d;
    gfloat      results_vet[g_slist_length(detected_objects)*g_slist_length(trackers)];
    gfloat      partc_vet[g_slist_length(trackers)];
    guint       remain_d[g_slist_length(detected_objects)];

    *unassociated_objects   = NULL;
    size_d                  = g_slist_length(detected_objects);
    size_tr                 = g_slist_length(trackers);

    // Init the remain detected objects vector
    for (it_d = 0; it_d < size_d; ++it_d)
        remain_d[it_d] = 1;

    GST_INFO("detected_objects: %d, trackers: %d\n", size_d, size_tr);

    // If exist any tracker
    if (size_tr) {

        // Init the greedy pairs array
        for (it_dxtr = 0; it_dxtr < size_d * size_tr; ++it_dxtr)
            results_vet[it_dxtr] = -1;

        it_d = 0;
        for (it_detected_obj = detected_objects; it_detected_obj; it_detected_obj = it_detected_obj->next) {
            CvPoint rect_centroid_d;
            detected_obj = (CvRect*) it_detected_obj->data;
            rect_centroid_d = rect_centroid(detected_obj);

            // PART C: probability according to the concentration of particles
            {
                gfloat max_partc;

                max_partc = -1;
                it_tr = 0;
                for (it_tracker = trackers; it_tracker; it_tracker = it_tracker->next) {
                    tracker = (Tracker*) it_tracker->data;
                    {
                        gint i, j, count_diff_particles;
                        CvPoint vet_particles[tracker->filter->SamplesNum];

                        // Cria o subconjunto de particulas diferentes entre si
                        count_diff_particles = 0;
                        for (i = 0; i < tracker->filter->SamplesNum; ++i) {
                            CvPoint point_temp = cvPoint(tracker->filter->flSamples[i][0], tracker->filter->flSamples[i][1]);

                            for (j = 0; j < count_diff_particles; ++j)
                                if (point_temp.x == vet_particles[j].x && point_temp.y == vet_particles[j].y) break;

                            if (j >= count_diff_particles) {
                                vet_particles[count_diff_particles] = point_temp;
                                count_diff_particles++;
                            }
                        }

                        if (count_diff_particles <= 1) {
                            partc_vet[it_tr] = -1;
                        } else {
                            gfloat mean, standard_deviation, part_c;

                            mean = standard_deviation = part_c = 0;

                            for (i = 0; i < count_diff_particles; ++i)
                                mean += abs(euclidian_distance(rect_centroid_d, vet_particles[i]));
                            mean /= count_diff_particles;

                            for (i = 0; i < count_diff_particles; ++i)
                                standard_deviation += pow((abs(euclidian_distance(rect_centroid_d, vet_particles[i])) - mean), 2);
                            standard_deviation /= count_diff_particles;
                            standard_deviation = sqrt(standard_deviation);

                            for (i = 0; i < count_diff_particles; ++i)
                                part_c += gaussian_function(abs(euclidian_distance(rect_centroid_d, vet_particles[i])), mean, standard_deviation);
                            part_c /= count_diff_particles;
                            partc_vet[it_tr] = part_c;
                            if (part_c > max_partc) max_partc = part_c;
                        }
                    }
                    it_tr++;
                }

                // FIXME: apply alpha value (evaluate situation of only one particle)
                for (it_tr = 0; it_tr < size_tr; it_tr++) {
                    if (partc_vet[it_tr] == -1) partc_vet[it_tr] = 1;
                    else partc_vet[it_tr] /= max_partc;
                }
            }

            it_tr = 0;
            for (it_tracker = trackers; it_tracker; it_tracker = it_tracker->next) {
                gfloat part_a, part_b, result;
                tracker = (Tracker*) it_tracker->data;

                // FIXME: clean the bad trackers
                if ((tracker == NULL) || (tracker->tracker_area.x < 0 || tracker->tracker_area.y < 0))
                    continue;

                // PART A: probability according to size and placement
                {
                    gfloat area_proportion, area_tr, area_d;
                    gfloat dist_tr_ini_to_end, dist_tr_end_to_d, dist_max;
                    CvPoint rect_centroid_tr;

                    // Area proportion
                    area_tr = tracker->tracker_area.height * tracker->tracker_area.width;
                    area_d = detected_obj->height * detected_obj->width;
                    area_proportion = gaussian_function((gfloat) ((area_tr - area_d) / area_tr), 0, 0.4);

                    // Distanciamento entre os objetos
                    dist_max = euclidian_distance(cvPoint(0, 0), cvPoint(image->width, image->height));
                    rect_centroid_tr = rect_centroid(&tracker->tracker_area);
                    dist_tr_ini_to_end = 1 - (euclidian_distance(rect_centroid_tr, tracker->previous_centroid) / dist_max);
                    dist_tr_end_to_d = 1 - (euclidian_distance(rect_centroid_tr, rect_centroid_d) / dist_max);

                    // If the tracker displacement greater than threshold that identifies the motion object
                    if (dist_tr_ini_to_end > 0.95) {
                        // Radial decay (stationary)
                        part_a = area_proportion * dist_tr_end_to_d;
                    } else {
                        // Cone decay (moving)
                        gfloat angle, dist_tr_vet_to_d;
                        angle = (180 - get_inner_angle_b(rect_centroid_tr, tracker->previous_centroid, rect_centroid_d)) / 180;
                        dist_tr_vet_to_d = 1 - (dist_point_segment(rect_centroid_d.x,
                                                rect_centroid_d.y, tracker->previous_centroid.x,
                                                tracker->previous_centroid.y, rect_centroid_tr.x,
                                                rect_centroid_tr.y) / dist_max);
                        part_a = area_proportion * ((angle + dist_tr_end_to_d + dist_tr_vet_to_d) / 3);
                    }
                }

                // PART B: probability according to similarity
                {
                    part_b = classifier_intermediate_classify(tracker->classifier, image, *detected_obj);
                    part_b = (part_b < 0) ? 0 : (part_b + 30) / 60;
                    part_b = pow(part_b, 4);
                }

                result = part_a * ((part_b + partc_vet[it_tr]) / 2);
                results_vet[it_tr + (it_d * size_tr)] = result;
                GST_INFO("A:%5.3f B:%5.3f C:%5.3f RESULT:%5.3f\n", part_a, part_b, partc_vet[it_tr], result);
                it_tr++;
            }
            it_d++;
        }

        // Discards pairs with values irrelevant
        for (it_dxtr = 0; it_dxtr < size_d * size_tr; ++it_dxtr)
            if (results_vet[it_dxtr] < MIN_GREEDY_CONSIDER_PAIR)
                results_vet[it_dxtr] = -1;

        // Find pairs and Rect updating (tracker->detected_object) of the associated objects
        for (;;) {

            gfloat max_result;
            guint  it_d_max, it_tr_max;

            max_result = -1;

            // Find the max result_value in array
            for (it_dxtr = 0; it_dxtr < size_d * size_tr; ++it_dxtr)
                if (results_vet[it_dxtr] > max_result){
                    max_result = results_vet[it_dxtr];
                    it_d_max = (guint) it_dxtr / size_tr;
                    it_tr_max = it_dxtr - ((guint) (it_d_max * size_tr));
                }

            // If not exist any valid value, abort
            if (max_result == -1) break;

            // Add the detected object selected in tracker
            detected_obj = (CvRect*) g_slist_nth_data(detected_objects, it_d_max);
            tracker = (Tracker*) g_slist_nth_data(trackers, it_tr_max);
            *tracker->detected_object = *detected_obj;
            tracker->frames_to_last_detecting = 0;
            remain_d[it_d_max] = 0;

            // Clean the row and column
            for (it_dxtr = 0; it_dxtr < size_d * size_tr; ++it_dxtr){
                guint it_d_temp, it_tr_temp;
                it_d_temp = (guint) it_dxtr / size_tr;
                it_tr_temp = it_dxtr - ((guint) (it_d_temp * size_tr));
                if(it_d_temp == it_d_max || it_tr_temp == it_tr_max)
                    results_vet[it_dxtr] = -1;
            }
        }
    }

    // Include the detected object without tr in unassociated array
    for (it_d = 0; it_d < size_d; ++it_d) {
        if (remain_d[it_d]) {
            detected_obj = (CvRect*) g_slist_nth_data(detected_objects, it_d);
            *unassociated_objects = g_slist_prepend(*unassociated_objects, detected_obj);
            GST_INFO("adding CvRect(%d, %d, %d, %d) at unassociated_objects",
                    detected_obj->x, detected_obj->y, detected_obj->width,
                    detected_obj->height);
        }
    }
}

static void
remove_old_trackers(IplImage *image, GSList **trackers) {

    GSList *it_tracker;
    for (it_tracker = *trackers; it_tracker; it_tracker = it_tracker->next) {
        Tracker *tracker = (Tracker*) it_tracker->data;

        if (classifier_intermediate_classify(tracker->classifier, image, tracker->tracker_area) >= 0)
            tracker->frames_of_wrong_classifier_to_del = 0;
        else
            tracker->frames_of_wrong_classifier_to_del++;

        //printf("%i) %f #notdet:%i #neg:%i\n", tracker->id, classifier_intermediate_classify(tracker->classifier, image, tracker->tracker_area), tracker->frames_to_last_detecting, tracker->frames_of_wrong_classifier_to_del);

        if (tracker->frames_to_last_detecting > FRAMES_TO_LAST_DETECTING_REM && tracker->frames_of_wrong_classifier_to_del > FRAMES_OF_WRONG_CLASSIFIER_REM)
            *trackers = g_slist_remove(*trackers, tracker);
    }
}

/* search and return the closer
 * region with intersection of obj */
GSList*
has_intersection(CvRect *obj, GSList *objects)
{
    gfloat       dist, min_dist;
    GSList      *correspondent_obj = NULL;
    CvRect       rect;
    CvPoint      center;
    GSList      *it_obj;
    gfloat       left, top, right, bottom;

    min_dist     = G_MAXFLOAT;
    center       = rect_centroid(obj);

    GST_INFO("num of objects: %d", g_slist_length(objects));
    for (it_obj = objects; it_obj; it_obj = it_obj->next) {

        rect = ((unassociated_obj_t*)it_obj->data)->region;

        left   = MAX(obj->x, rect.x);
        top    = MAX(obj->y, rect.y);
        right  = MIN(obj->x + obj->width, rect.x + rect.width );
        bottom = MIN(obj->y + obj->height, rect.y + rect.height);

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
    gfloat   dist, min_dist;
    Tracker *tr, *closer_tracker;
    GSList  *it_tracker;

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
    gint            i, pointCount;
    CvRNG           rng_state;
    CvPoint         p;
    gfloat          horizontal_sigma, vertical_sigma;
    gfloat          center_x,center_y;
    CvMat           *locations;

    pointCount = 100;
    rng_state  = cvRNG(0xffffffff);

    cvRectangle(    image,
                    cvPoint(rect.x, rect.y),
                    cvPoint(rect.x+ rect.width, rect.y+ rect.height),
                    CV_RGB(255, 255,0), 1, 8, 0);

    center_x = (gfloat)(rect.width)/2.0 + rect.x;
    center_y = (gfloat)(rect.height)/2.0 + rect.y;

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

void print_tracker(Tracker *tracker, IplImage *image, gint id_tracker, gboolean show_particles)
{
    gint i;
    gfloat intensity;

    if (show_particles) {
        for (i = 0; i < tracker->filter->SamplesNum; i++) {
                intensity =  tracker->filter->flConfidence[i]/tracker->max_confidence;


                cvCircle(image, cvPoint(tracker->filter->flSamples[i][0], tracker->filter->flSamples[i][1]),
                                3, CV_RGB(0, (1-intensity)*255, intensity * 255), -1, 8, 0);
        }
        // center of the tracker (state vector)
        cvCircle(image, cvPoint(tracker->filter->State[0], tracker->filter->State[1]),
                            7, colorIdx(id_tracker), 2, 8, 0);

    }

    // region of the tracker
    cvRectangle(image,  cvPoint(tracker->tracker_area.x, tracker->tracker_area.y),
                        cvPoint(tracker->tracker_area.x + tracker->tracker_area.width,
                                tracker->tracker_area.y + tracker->tracker_area.height),
                        colorIdx(id_tracker), 2, 8, 0);

    // Prints the detected object rect and connects it to the tracker rect
    //print_rect(image, *tracker->detected_object, 1);
    //cvLine(image, cvPoint(tracker->detected_object->x, tracker->detected_object->y), cvPoint(tracker->tracker_area.x, tracker->tracker_area.y), CV_RGB(0, 0, 255), 2, 8, 0);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_tracker_chain(GstPad *pad, GstBuffer *buf)
{
    GstTracker          *filter;
    IplImage            *image;
    GSList              *unassociated_objects = NULL;
    unassociated_obj_t  *unassociated_obj = NULL;

    GSList              *it_obj = NULL;
    GSList              *it_tracker = NULL;
    GSList              *intersection_last_frame = NULL;

    Tracker             *new_tracker = NULL;
    Tracker             *tracker = NULL;
    Tracker             *closer_tracker = NULL;

    filter = GST_TRACKER(GST_OBJECT_PARENT(pad));
    filter->image->imageData = (char *) GST_BUFFER_DATA(buf);

    image = cvCloneImage(filter->image);

    // Remove old trackers
    remove_old_trackers(image, &filter->trackers);

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

                if (unassociated_obj->count >= TRACKER_NUM_SUBSEQUENT_DETECTIONS) {
                    new_tracker = tracker_new( &unassociated_obj->region, 4, 4,
                                                TRACKER_NUM_PARTICLES,
                                                image,
                                                filter->beta, filter->gamma, filter->eta,
                                                g_slist_length(filter->trackers)+1 );

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
        /*
        for (it_obj = unassociated_objects; it_obj; it_obj = it_obj->next) {
            GST_INFO("free unassociated_object");
            g_free((CvRect*)it_obj->data);
        }
        */
        g_slist_free(unassociated_objects);
    }

    // tracking
    GST_INFO("trackers: %d\n", g_slist_length(filter->trackers));
    for (it_tracker = filter->trackers; it_tracker; it_tracker = it_tracker->next) {
        tracker = (Tracker*)it_tracker->data;
        tracker->frames_to_last_detecting++;

        GST_INFO("running tracker: %d", tracker->id);

        print_tracker(tracker, filter->image, tracker->id, filter->show_particles);

        closer_tracker = closer_tracker_with_a_detected_obj_to( tracker, filter->trackers );
        tracker_run(tracker, closer_tracker, &filter->confidence_density, image );
    }

    gst_buffer_set_data(buf, (guint8*) filter->image->imageData, (guint) filter->image->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}

// callbacks
static gboolean
gst_tracker_events_cb(GstPad *pad, GstEvent *event, gpointer user_data)
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
