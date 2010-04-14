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
 * SECTION:element-surftracker
 *
 * Performs face recognition using Vetta Labs' Facemetrix server. It depends
 * on 'face' events generated by the 'facedetect' plugin.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace !
 *                 bgfgcodebook                                !
 *                 haardetect roi-only=true                    !
 *                 haaradjust                                  !
 *                 surftracker verbose=true display=true       !
 *                 ffmpegcolorspace ! xvimagesink sync=false
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gstsurftracker.h>

#include <gst/gst.h>
#include <gst/gststructure.h>
#include <cvaux.h>
#include <highgui.h>

GST_DEBUG_CATEGORY_STATIC(gst_surf_tracker_debug);
#define GST_CAT_DEFAULT gst_surf_tracker_debug

#define PERC_RECT_TO_SAME_OBJECT         .6
#define PAIRS_PERC_CONSIDERATE           .6
#define PRINT_COLOR                      CV_RGB(205, 85, 85)
#define MIN_MATCH_OBJECT                 .15
#define DELOBJ_NFRAMES_IS_OLD            10
#define DELOBJ_COMBOFRAMES_IS_IRRELEVANT 3

enum {
    PROP_0,
    PROP_VERBOSE,
    PROP_DISPLAY,
    PROP_DISPLAY_FEATURES
};

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

struct _InstanceObject
{
    gint          id;
    gint          last_frame_viewed;
    gint          range_viewed;
    CvSeq        *surf_object_keypoints;
    CvSeq        *surf_object_descriptors;
    CvSeq        *surf_object_keypoints_last_match;
    CvSeq        *surf_object_descriptors_last_match;
    CvMemStorage *mem_storage;
    CvRect        rect;
    CvRect        rect_estimated;
    GstClockTime  timestamp;
    GstClockTime  last_body_identify_timestamp;
};
typedef struct _InstanceObject InstanceObject;

GST_BOILERPLATE(GstSURFTracker, gst_surf_tracker, GstElement, GST_TYPE_ELEMENT);

static void          gst_surf_tracker_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_surf_tracker_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean      gst_surf_tracker_set_caps     (GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_surf_tracker_chain        (GstPad *pad, GstBuffer *buf);
static gboolean      events_cb                     (GstPad *pad, GstEvent *event, gpointer user_data);

static void
gst_surf_tracker_finalize(GObject *obj) {
    GstSURFTracker *filter = GST_SURF_TRACKER(obj);

    if (filter->image) cvReleaseImage(&filter->image);
    if (filter->gray) cvReleaseImage(&filter->gray);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

// gobject vmethod implementations

static void
gst_surf_tracker_base_init(gpointer gclass) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "surftracker",
                                         "Filter/Video",
                                         "Tracks objects using SURF-based feature extraction",
                                         "Lucas Pantuza Amorim <lucas@vettalabs.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the surftracker's class

static void
gst_surf_tracker_class_init(GstSURFTrackerClass *klass) {
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class  = g_type_class_peek_parent(klass);

    gobject_class->finalize     = GST_DEBUG_FUNCPTR(gst_surf_tracker_finalize);
    gobject_class->set_property = gst_surf_tracker_set_property;
    gobject_class->get_property = gst_surf_tracker_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose",
                                                         "Print movement direction to stdout",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display",
                                                         "Highligh the objects in the video output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY_FEATURES,
                                    g_param_spec_boolean("display-features", "Display features",
                                                         "Highlight the SURF feature points in the video output",
                                                         FALSE, G_PARAM_READWRITE));
}

// initialize the new element
// instantiate pads and add them to element
// set pad calback functions
// initialize instance structure
static void
gst_surf_tracker_init(GstSURFTracker *filter, GstSURFTrackerClass *gclass) {
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_surf_tracker_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_surf_tracker_chain));

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    filter->verbose              = FALSE;
    filter->display              = FALSE;
    filter->display_features     = FALSE;
    filter->params               = cvSURFParams(100, 1);
    filter->static_count_objects = 0;
    filter->frames_processed     = 0;
    filter->rect_timestamp       = 0;
    filter->rect_array           = g_array_new(FALSE, FALSE, sizeof(CvRect));
    filter->stored_objects       = g_array_new(FALSE, FALSE, sizeof(InstanceObject));
}

static void
gst_surf_tracker_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstSURFTracker *filter = GST_SURF_TRACKER(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY:
            filter->display = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY_FEATURES:
            filter->display_features = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_surf_tracker_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstSURFTracker *filter = GST_SURF_TRACKER(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_DISPLAY:
            g_value_set_boolean(value, filter->display);
            break;
        case PROP_DISPLAY_FEATURES:
            g_value_set_boolean(value, filter->display_features);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static gboolean
gst_surf_tracker_set_caps(GstPad *pad, GstCaps *caps) {
    GstSURFTracker *filter;
    GstPad         *other_pad;
    GstStructure   *structure;
    gint            width, height, depth;

    filter    = GST_SURF_TRACKER(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth", &depth);

    filter->image = cvCreateImage(cvSize(width, height), depth / 3, 3);
    filter->gray  = cvCreateImage(cvSize(width, height), 8, 1);

    // add roi event probe on the sinkpad
    gst_pad_add_event_probe(filter->sinkpad, (GCallback) events_cb, filter);

    other_pad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);

    return gst_pad_set_caps(other_pad, caps);
}

// chain function; this function does the actual processing
static GstFlowReturn
gst_surf_tracker_chain(GstPad *pad, GstBuffer *buf) {
    GstSURFTracker *filter;
    GstClockTime    timestamp;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_SURF_TRACKER(GST_OBJECT_PARENT(pad));
    filter->image->imageData = (char*) GST_BUFFER_DATA(buf);

    // Create the gray image for the surf 'features' search process
    cvCvtColor(filter->image, filter->gray, CV_BGR2GRAY);
    ++filter->frames_processed;
    timestamp = GST_BUFFER_TIMESTAMP(buf);

    // If exist stored_objects: search matching, update, cleaning
    if ((filter->stored_objects != NULL) && (filter->stored_objects->len > 0)) {
        CvMemStorage *surf_image_mem_storage;
        CvSeq        *surf_image_keypoints, *surf_image_descriptors;
        guint         i;
        gint          j;

        // Update the match set 'features' for each object
        surf_image_mem_storage = cvCreateMemStorage(0);

        // Search 'features' in full image
        surf_image_keypoints = surf_image_descriptors = NULL;
        cvExtractSURF(filter->gray, NULL, &surf_image_keypoints, &surf_image_descriptors,
                      surf_image_mem_storage, filter->params, 0);

        for (i = 0; i < filter->stored_objects->len; ++i) {
            InstanceObject *object;
            GArray         *pairs;

            object = &g_array_index(filter->stored_objects, InstanceObject, i);
            pairs  = g_array_new(FALSE, FALSE, sizeof(IntPair));

            findPairs(object->surf_object_keypoints, object->surf_object_descriptors,
                      surf_image_keypoints, surf_image_descriptors, pairs);

            // if match, update object
            if (pairs->len && (float) pairs->len / object->surf_object_descriptors->total >= MIN_MATCH_OBJECT) {
                object->range_viewed++;
                object->last_frame_viewed = filter->frames_processed;
                object->timestamp         = timestamp;

                if (object->surf_object_keypoints_last_match != NULL)
                    cvClearSeq(object->surf_object_keypoints_last_match);
                object->surf_object_keypoints_last_match = getMatchPoints(surf_image_keypoints, pairs, 1, object->mem_storage);

                if (object->surf_object_descriptors_last_match != NULL)
                    cvClearSeq(object->surf_object_descriptors_last_match);
                object->surf_object_descriptors_last_match = getMatchPoints(surf_image_descriptors, pairs, 1, object->mem_storage);

                // Estimate rect of objects localized
                object->rect_estimated = rectDisplacement(object->surf_object_keypoints, surf_image_keypoints, pairs, object->rect, PAIRS_PERC_CONSIDERATE);
            }

            g_array_free(pairs, TRUE);
        }

        if (surf_image_keypoints != NULL) cvClearSeq(surf_image_keypoints);
        if (surf_image_descriptors != NULL) cvClearSeq(surf_image_descriptors);
        cvReleaseMemStorage(&surf_image_mem_storage);

        // Clean old objects
        for (j = filter->stored_objects->len - 1; j >= 0; --j) {
            InstanceObject *object;

            object = &g_array_index(filter->stored_objects, InstanceObject, j);
            if ((filter->frames_processed - object->last_frame_viewed > DELOBJ_NFRAMES_IS_OLD) ||
                (filter->frames_processed != object->last_frame_viewed && object->range_viewed < DELOBJ_COMBOFRAMES_IS_IRRELEVANT)) {
                if (object->surf_object_keypoints != NULL) cvClearSeq(object->surf_object_keypoints);
                if (object->surf_object_descriptors != NULL) cvClearSeq(object->surf_object_descriptors);
                if (object->surf_object_keypoints_last_match != NULL) cvClearSeq(object->surf_object_keypoints_last_match);
                if (object->surf_object_descriptors_last_match != NULL) cvClearSeq(object->surf_object_descriptors_last_match);
                cvReleaseMemStorage(&object->mem_storage);
                g_array_remove_index_fast(filter->stored_objects, j);
            }
        }

    } // if any object exist

    // Process all haar rects
    if ((filter->rect_array != NULL) && (filter->rect_array->len > 0)) {
        guint i, j;

        for (i = 0; i < filter->rect_array->len; ++i) {
            CvRect rect = g_array_index(filter->rect_array, CvRect, i);

            // If already exist in 'stored_objects', update features. Else save
            // as new.
            for (j = 0; j < filter->stored_objects->len; ++j) {
                InstanceObject *object;

                object = &g_array_index(filter->stored_objects, InstanceObject, j);

                // It is considered equal if the "centroid match features" is inner
                // haar rect AND max area deviation is PERC_RECT_TO_SAME_OBJECT
                if (pointIntoRect(rect, (object->surf_object_keypoints_last_match != NULL) ? surfCentroid(object->surf_object_keypoints_last_match, cvPoint(0, 0)) : surfCentroid(object->surf_object_keypoints, cvPoint(0, 0))) &&
                    ((float) MIN((object->rect.width * object->rect.height), (rect.width * rect.height)) / (float) MAX((object->rect.width * object->rect.height), (rect.width * rect.height)) >= PERC_RECT_TO_SAME_OBJECT)) {

                    // Update the object features secound the new body rect
                    cvSetImageROI(filter->gray, rect);
                    cvExtractSURF(filter->gray, NULL, &object->surf_object_keypoints, &object->surf_object_descriptors,
                                  object->mem_storage, filter->params, 0);
                    cvResetImageROI(filter->gray);
                    object->rect = object->rect_estimated = rect;
                    object->last_body_identify_timestamp = timestamp;

                    break;
                }
            }

            // If new, create object and append in stored_objects
            if (j >= filter->stored_objects->len) {
                InstanceObject object;

                object.surf_object_keypoints   = 0;
                object.surf_object_descriptors = 0;
                object.mem_storage             = cvCreateMemStorage(0);

                cvSetImageROI(filter->gray, rect);
                cvExtractSURF(filter->gray, NULL, &object.surf_object_keypoints, &object.surf_object_descriptors,
                              object.mem_storage, filter->params, 0);
                cvResetImageROI(filter->gray);

                if (object.surf_object_descriptors && object.surf_object_descriptors->total > 0) {
                    object.id                                 = filter->static_count_objects++;
                    object.last_frame_viewed                  = filter->frames_processed;
                    object.range_viewed                       = 1;
                    object.rect                               = object.rect_estimated               = rect;
                    object.timestamp                          = object.last_body_identify_timestamp = timestamp;
                    object.surf_object_keypoints_last_match   = NULL;
                    object.surf_object_descriptors_last_match = NULL;

                    g_array_append_val(filter->stored_objects, object);
                }
            } // new
        }
    }

    // Put the objects found in the frame in gstreamer pad
    if ((filter->stored_objects != NULL) && (filter->stored_objects->len > 0)) {
        guint i;

        for (i = 0; i < filter->stored_objects->len; ++i) {
            InstanceObject object = g_array_index(filter->stored_objects, InstanceObject, i);

            // 'Continue' whether the object is not found in this frame
            if (object.timestamp == timestamp) {
                GstEvent     *event;
                GstMessage   *message;
                GstStructure *structure;
                CvRect        rect;

                rect = ((object.last_body_identify_timestamp == timestamp) ? object.rect : object.rect_estimated);

                if (filter->verbose) {
                    GST_INFO("[object #%d rect] x: %d, y: %d, width: %d, height: %d\n", object.id, rect.x, rect.y, rect.width, rect.height);
                    // drawSurfPoints(object.surf_object_keypoints, cvPoint(object.rect.x, object.rect.y), filter->image, PRINT_COLOR, 0);
                    // drawSurfPoints(object.surf_object_keypoints_last_match, cvPoint(object.rect.x, object.rect.y), filter->image, PRINT_COLOR, 1);
                }

                if (filter->display_features) {
                    drawSurfPoints(object.surf_object_keypoints_last_match, cvPoint(0, 0), filter->image, PRINT_COLOR, 1);
                }

                if (filter->display) {
                    char *label;
                    float font_scaling;

                    label        = g_strdup_printf("OBJ#%i (%i%%)", object.id, (!object.surf_object_descriptors_last_match) ? 100 : 100 * object.surf_object_descriptors_last_match->total / object.surf_object_descriptors->total);
                    font_scaling = ((filter->image->width * filter->image->height) > (320 * 240)) ? 0.5f : 0.3f;

                    cvRectangle(filter->image, cvPoint(rect.x, rect.y), cvPoint(rect.x + rect.width, rect.y + rect.height),
                                PRINT_COLOR, ((object.last_body_identify_timestamp == timestamp) ? 2 : 1), 8, 0);
                    label = g_strdup_printf("OBJ#%i", object.id);
                    printText(filter->image, cvPoint(rect.x + (rect.width / 2), rect.y + (rect.height / 2)), label, PRINT_COLOR, font_scaling, 1);
                    g_free(label);
                }

                // Send downstream event and bus message with the rect info
                structure = gst_structure_new("object-tracking",
                                              "id",        G_TYPE_UINT,   object.id,
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
            }
        }
    }

    // Clean body rects
    g_array_free(filter->rect_array, TRUE);
    filter->rect_array = g_array_sized_new(FALSE, FALSE, sizeof(CvRect), 1);

    // Draw number of objects stored
    if (filter->display) {
        char *label = g_strdup_printf("N_STORED_OBJS: %3i", filter->stored_objects->len);
        printText(filter->image, cvPoint(0, 0), label, PRINT_COLOR, .5, 1);
        g_free(label);
    }

    gst_buffer_set_data(buf, (guint8*) filter->image->imageData, (guint) filter->image->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}

// callbacks

static
gboolean events_cb(GstPad *pad, GstEvent *event, gpointer user_data) {
    GstSURFTracker *filter;
    const GstStructure *structure;

    filter = GST_SURF_TRACKER(user_data);

    // sanity checks
    g_return_val_if_fail(pad    != NULL, FALSE);
    g_return_val_if_fail(event  != NULL, FALSE);
    g_return_val_if_fail(filter != NULL, FALSE);

    structure = gst_event_get_structure(event);

    if ((structure != NULL) && (strcmp(gst_structure_get_name(structure), "haar-adjust-roi") == 0)) {
        CvRect       rect;
        GstClockTime timestamp;

        gst_structure_get((GstStructure*) structure,
                          "x", G_TYPE_UINT, &rect.x,
                          "y", G_TYPE_UINT, &rect.y,
                          "width", G_TYPE_UINT, &rect.width,
                          "height", G_TYPE_UINT, &rect.height,
                          "timestamp", G_TYPE_UINT64, &timestamp, NULL);

        if (timestamp > filter->rect_timestamp) {
            filter->rect_timestamp = timestamp;
        }
        g_array_append_val(filter->rect_array, rect);
    }

    return TRUE;
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features
gboolean
gst_surf_tracker_plugin_init(GstPlugin *plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_surf_tracker_debug, "surftracker", 0,
                            "Tracks objects using SURF-based feature extraction");

    return gst_element_register(plugin, "surftracker", GST_RANK_NONE, GST_TYPE_SURF_TRACKER);
}
