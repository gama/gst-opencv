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
 * SECTION:element-objectdistances
 *
 * Parses static-object definitions from string properties and
 * pushes downstream events every frame with these objects using
 * the 'TrackedObject' class format
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace !
 *                 objectdistances verbose=true
 *                                 display=true
 *                 ffmpegcolorspace ! xvimagesink sync=false
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstobjectdistances.h"
#include "tracked-object.h"
#include "draw.h"

#include <gst/gst.h>
#include <gst/gststructure.h>

#define LINE_COLOR CV_RGB(127, 31, 127)

GST_DEBUG_CATEGORY_STATIC(gst_object_distances_debug);
#define GST_CAT_DEFAULT gst_object_distances_debug

enum {
    PROP_0,
    PROP_VERBOSE,
    PROP_OBJECTS,
    PROP_DISPLAY
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

GST_BOILERPLATE(GstObjectDistances, gst_object_distances, GstElement, GST_TYPE_ELEMENT);

static void          gst_object_distances_set_property      (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_object_distances_get_property      (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean      gst_object_distances_set_caps          (GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_object_distances_chain             (GstPad *pad, GstBuffer *buf);
static gboolean      gst_object_distances_parse_objects_str (GstObjectDistances *filter);
static gboolean      gst_object_distances_events_cb         (GstPad *pad, GstEvent *event, gpointer user_data);
static float         gst_object_distance_euclidian_distance (TrackedObject *object1, TrackedObject *object2, CvPoint2D32f *point1, CvPoint2D32f *point2);

static void
gst_object_distances_finalize(GObject *obj)
{
    GstObjectDistances *filter = GST_OBJECT_DISTANCES(obj);

    if (filter->image) cvReleaseImage(&filter->image);
    g_list_foreach(filter->objects_list, (GFunc) tracked_object_free, NULL);
    if (filter->objects_list) g_list_free(filter->objects_list);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

// gobject vmethod implementations
static void
gst_object_distances_base_init(gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "objectdistances",
                                         "Filter/Video",
                                         "Calculates the euclidian distance between objects",
                                         "Lucas Pantuza Amorim <lucas@vettalabs.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the objectdistances's class

static void
gst_object_distances_class_init(GstObjectDistancesClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class  = g_type_class_peek_parent(klass);

    gobject_class->finalize     = GST_DEBUG_FUNCPTR(gst_object_distances_finalize);
    gobject_class->set_property = gst_object_distances_set_property;
    gobject_class->get_property = gst_object_distances_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose",
                                                         "Sets whether the movement direction should be printed to the standard output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display",
                                                         "Highligh the interations in the video output",
                                                         FALSE, G_PARAM_READWRITE));
}

// initialize the new element
// instantiate pads and add them to element
// set pad calback functions
// initialize instance structure

static void
gst_object_distances_init(GstObjectDistances *filter, GstObjectDistancesClass *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_object_distances_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_object_distances_chain));

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    filter->verbose      = FALSE;
    filter->display      = FALSE;
    filter->objects_list = NULL;
}

static void
gst_object_distances_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstObjectDistances *filter = GST_OBJECT_DISTANCES(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY:
            filter->display = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_object_distances_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstObjectDistances *filter = GST_OBJECT_DISTANCES(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_DISPLAY:
            g_value_set_boolean(value, filter->display);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static gboolean
gst_object_distances_set_caps(GstPad *pad, GstCaps *caps)
{
    GstObjectDistances *filter;
    GstPad           *other_pad;
    GstStructure     *structure;
    gint              width, height, depth;

    filter    = GST_OBJECT_DISTANCES(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth", &depth);

    filter->image = cvCreateImage(cvSize(width, height), depth / 3, 3);

    // add roi event probe on the sinkpad
    gst_pad_add_event_probe(filter->sinkpad, (GCallback) gst_object_distances_events_cb, filter);

    other_pad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);

    return gst_pad_set_caps(other_pad, caps);
}

// chain function; this function does the actual processing
static GstFlowReturn
gst_object_distances_chain(GstPad *pad, GstBuffer *buf)
{
    GstObjectDistances *filter;
    GList              *iter1, *iter2, *iter_next;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_OBJECT_DISTANCES(GST_OBJECT_PARENT(pad));
    filter->image->imageData = (char*) GST_BUFFER_DATA(buf);

    // send downstream events with the euclidian distance between each pair
    // of tracked objects
    for (iter1 = filter->objects_list; iter1 != NULL; iter1 = iter_next) {
        TrackedObject *tracked_object1 = iter1->data;

        iter_next = iter1->next;

        // skip objects where the timestamp doesn't match
        if (tracked_object1->timestamp != GST_BUFFER_TIMESTAMP(buf))
            continue;

        for (iter2 = iter1->next; iter2 != NULL; iter2 = iter2->next) {
            GstEvent      *event;
            GstStructure  *structure;
            TrackedObject *tracked_object2;
            CvPoint2D32f   point1, point2;
            float          distance;

            tracked_object2 = iter2->data;

            // skip objects where the timestamp doesn't match
            if (tracked_object2->timestamp != GST_BUFFER_TIMESTAMP(buf))
                continue;

            // dont't calculate/publish distances between static objects
            if ((tracked_object1->type == TRACKED_OBJECT_STATIC) &&
                (tracked_object2->type == TRACKED_OBJECT_STATIC))
                continue;

            point1.x = point1.y = point2.x = point2.y = 0.0f; // avoid gcc warnings
            distance = gst_object_distance_euclidian_distance(tracked_object1, tracked_object2,
                                                              &point1, &point2);
            if (filter->verbose)
                GST_DEBUG_OBJECT(filter, "distance between %s and %s: %.2f",
                                 tracked_object1->id, tracked_object2->id, distance);

            if (filter->display) {
                gchar *distance_label;

                // draw a line between the objects and a label between them
                cvLine(filter->image, cvPointFrom32f(point1), cvPointFrom32f(point2), LINE_COLOR, 2, 8, 0);

                // then, draw a label with the distance in the middle of the line
                distance_label = g_strdup_printf("%.2fm", distance);
                printText(filter->image, cvPoint((point1.x + point2.x) / 2, (point1.y + point2.y) / 2),
                          distance_label, LINE_COLOR, 0.3, TRUE);
                g_free(distance_label);
            }

            structure = gst_structure_new("tracked-objects-distance", 
                                          "obj1",      G_TYPE_STRING, tracked_object1->id,
                                          "obj2",      G_TYPE_STRING, tracked_object2->id,
                                          "distance",  G_TYPE_FLOAT,  distance,
                                          "timestamp", G_TYPE_UINT64, tracked_object1->timestamp,
                                          NULL);
            event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
            gst_pad_push_event(filter->srcpad, event);
        }

        // free the current tracked object and add its list node to a 'to_remove' list
        tracked_object_free(tracked_object1);
        filter->objects_list = g_list_delete_link(filter->objects_list, iter1);
    }

    gst_buffer_set_data(buf, (guint8*) filter->image->imageData, (guint) filter->image->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}

// calculates the euclidian distance between the tracked objects passed
// as parameters 'object1' and 'object2'.
//
// a few special rules are applied to select the points that will be used
// to calculate the distance. The exact points used to calculate the
// distance are returned in the output parameters 'point1' and 'point2'.
static float
gst_object_distance_euclidian_distance(TrackedObject *object1, TrackedObject *object2,
                                       CvPoint2D32f *point1, CvPoint2D32f *point2)
{
    CvPoint2D32f centroid1, centroid2, p1, p2;
    float        distance, min_distance;
    guint        i, j;
    
    min_distance = FLT_MAX;

    for (i = 1; i < object1->point_array->len; ++i) {
        p1 = cvPointTo32f(g_array_index(object1->point_array, CvPoint, i - 1));
        p2 = cvPointTo32f(g_array_index(object1->point_array, CvPoint, i));
        centroid1.x = (p1.x + p2.x) / 2;
        centroid1.y = (p1.y + p2.y) / 2;

        for (j = 1; j < object2->point_array->len; ++j) {
            p1 = cvPointTo32f(g_array_index(object2->point_array, CvPoint, i - 1));
            p2 = cvPointTo32f(g_array_index(object2->point_array, CvPoint, i));

            centroid2.x = (p1.x + p2.x) / 2;
            centroid2.y = (p1.y + p2.y) / 2;

            distance = sqrtf(powf(centroid1.x - centroid2.x, 2) + powf(centroid1.y - centroid2.y, 2));
            if (distance < min_distance) {
                min_distance = distance;
                *point1 = centroid1;
                *point2 = centroid2;
            }
        }
    }
    return min_distance;
}

// callbacks
static gboolean
gst_object_distances_events_cb(GstPad *pad, GstEvent *event, gpointer user_data)
{
    GstObjectDistances *filter;
    const GstStructure *structure;

    filter = GST_OBJECT_DISTANCES(user_data);

    // sanity checks
    g_return_val_if_fail(pad    != NULL, FALSE);
    g_return_val_if_fail(event  != NULL, FALSE);
    g_return_val_if_fail(filter != NULL, FALSE);

    structure = gst_event_get_structure(event);

    // plugins possible: haar-detect-roi, haar-adjust-roi, object-tracking
    if ((structure != NULL) && (strcmp(gst_structure_get_name(structure), "tracked-object") == 0)) {
        TrackedObject *object = tracked_object_from_structure(structure);
        filter->objects_list = g_list_prepend(filter->objects_list, object);
    }

    return TRUE;
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features
gboolean
gst_object_distances_plugin_init(GstPlugin *plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_object_distances_debug, "objectdistances", 0,
                            "Calculates the euclidian distance between objects");

    return gst_element_register(plugin, "objectdistances", GST_RANK_NONE, GST_TYPE_OBJECT_DISTANCES);
}
