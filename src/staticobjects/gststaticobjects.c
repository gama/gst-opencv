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
 * SECTION:element-staticobjects
 *
 * Parses static-object definitions from string properties and
 * pushes downstream events every frame with these objects using
 * the 'TrackedObject' class format
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace !
 *                 staticobjects verbose=true
 *                               display=true
 *                               objects=meeting-room-door,240,392,367,397,387\;kitchen-door,191,288,318,360,31 !
 *                 ffmpegcolorspace ! xvimagesink sync=false
 * ]|
 * </refsect2>
 *
 * sample webcam marks:
 *    meeting-room-door,240,392,367,397,387
 *    kitchen-door,191,288,318,360,318
 *    bathroom-door,230,253,367,256,350
 *    management-room-door,270,241,410,245,388
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gststaticobjects.h"
#include "tracked-object.h"

#include <gst/gst.h>
#include <gst/gststructure.h>

#define OBJECT_COLOR CV_RGB(31, 127, 127)

GST_DEBUG_CATEGORY_STATIC(gst_static_objects_debug);
#define GST_CAT_DEFAULT gst_static_objects_debug

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

GST_BOILERPLATE(GstStaticObjects, gst_static_objects, GstElement, GST_TYPE_ELEMENT);

static void          gst_static_objects_set_property       (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_static_objects_get_property       (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean      gst_static_objects_set_caps           (GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_static_objects_chain              (GstPad *pad, GstBuffer *buf);
static GArray*       make_point_array                      (const gchar *str);
static gboolean      gst_static_objects_parse_objects_str  (GstStaticObjects *filter);

static void
gst_static_objects_finalize(GObject *obj)
{
    GstStaticObjects *filter;
    GList            *iter;

    filter = GST_STATIC_OBJECTS(obj);

    if (filter->image)       cvReleaseImage(&filter->image);
    if (filter->objects_str) g_free(filter->objects_str);

    for (iter = filter->objects_list; iter != NULL; iter = iter->next)
        tracked_object_free(iter->data);
    if (filter->objects_list) g_list_free(filter->objects_list);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

// gobject vmethod implementations
static void
gst_static_objects_base_init(gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "staticobjects",
                                         "Filter/Video",
                                         "Parses static object definitions and push events with the parsed data",
                                         "Lucas Pantuza Amorim <lucas@vettalabs.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the staticobjects's class

static void
gst_static_objects_class_init(GstStaticObjectsClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class  = g_type_class_peek_parent(klass);

    gobject_class->finalize     = GST_DEBUG_FUNCPTR(gst_static_objects_finalize);
    gobject_class->set_property = gst_static_objects_set_property;
    gobject_class->get_property = gst_static_objects_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose",
                                                         "Sets whether the movement direction should be printed to the standard output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display",
                                                         "Highligh the interations in the video output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_OBJECTS,
                                    g_param_spec_string("objects", "Objects definition string",
                                                        "String defining the list of objects. Format is: <obj1-label>,<obj1-height>,<obj1-x1>,<obj1-y1>,<obj1-x2>,<obj1-y2>,...;<obj2-label>,<obj2-height>,<obj2-x1>,<obj2-y1>,<obj2-x2>,<obj2-y2>,...",
                                                         NULL, G_PARAM_READWRITE));
}

// initialize the new element
// instantiate pads and add them to element
// set pad calback functions
// initialize instance structure

static void
gst_static_objects_init(GstStaticObjects *filter, GstStaticObjectsClass *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_static_objects_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_static_objects_chain));

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    filter->verbose      = FALSE;
    filter->display      = FALSE;
    filter->objects_str  = NULL;
    filter->objects_list = NULL;
}

static void
gst_static_objects_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstStaticObjects *filter = GST_STATIC_OBJECTS(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY:
            filter->display = g_value_get_boolean(value);
            break;
        case PROP_OBJECTS:
            if (filter->objects_str) g_free(filter->objects_str);
            filter->objects_str = g_value_dup_string(value);
            if (gst_static_objects_parse_objects_str(filter) == FALSE)
                GST_WARNING_OBJECT(filter, "unable to parse objects string: \"%s\"", filter->objects_str);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_static_objects_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstStaticObjects *filter = GST_STATIC_OBJECTS(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_DISPLAY:
            g_value_set_boolean(value, filter->display);
            break;
        case PROP_OBJECTS:
            g_value_set_string(value, filter->objects_str);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static gboolean
gst_static_objects_set_caps(GstPad *pad, GstCaps *caps)
{
    GstStaticObjects *filter;
    GstPad           *other_pad;
    GstStructure     *structure;
    gint              width, height, depth;

    filter    = GST_STATIC_OBJECTS(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth", &depth);

    filter->image = cvCreateImage(cvSize(width, height), depth / 3, 3);

    other_pad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);

    return gst_pad_set_caps(other_pad, caps);
}

// chain function; this function does the actual processing
static GstFlowReturn
gst_static_objects_chain(GstPad *pad, GstBuffer *buf)
{
    GstStaticObjects *filter;
    GList            *iter;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_STATIC_OBJECTS(GST_OBJECT_PARENT(pad));
    filter->image->imageData = (char*) GST_BUFFER_DATA(buf);

    // send downstream events with the tracked objects' data
    for (iter = filter->objects_list; iter != NULL; iter = iter->next) {
        TrackedObject *tracked_object;
        GstEvent      *event;
        GstStructure  *structure;

        tracked_object = iter->data;

        // update the object's timestamp
        tracked_object->timestamp = GST_BUFFER_TIMESTAMP(buf);

        if (filter->verbose) {
            gchar *tracked_object_str = tracked_object_to_string(tracked_object);
            GST_DEBUG_OBJECT(filter, "static object: %s\n", tracked_object_str);
            g_free(tracked_object_str);
        }

        if (filter->display) {
            // draw object contours on the output image
            guint i;

            for (i = 0; i < tracked_object->point_array->len; ++i) {
                // we use two points: one at the exact coordinates [x, y], and
                // another at [x, y + height]
                CvPoint *base_point   = &g_array_index(tracked_object->point_array, CvPoint, i);
                CvPoint  summit_point = cvPoint(base_point->x, base_point->y - tracked_object->height);

                // draw the points and a line between them
                cvCircle(filter->image, *base_point,  2, OBJECT_COLOR, CV_FILLED, 8, 0);
                cvCircle(filter->image, summit_point, 2, OBJECT_COLOR, CV_FILLED, 8, 0);
                cvLine(filter->image, *base_point, summit_point, OBJECT_COLOR, 1, 8, 0);

                if (i > 0) {
                    // draw the lines connecting the base segments and the summit points
                    CvPoint *previous_base_point   = &g_array_index(tracked_object->point_array, CvPoint, i - 1);
                    CvPoint  previous_summit_point = cvPoint(previous_base_point->x, previous_base_point->y - tracked_object->height);
                    cvLine(filter->image, *previous_base_point, *base_point, OBJECT_COLOR, 1, 8, 0);
                    cvLine(filter->image, previous_summit_point, summit_point, OBJECT_COLOR, 1, 8, 0);
                }
            }
        }

        // now, send a new event with the new object
        if ((structure = tracked_object_to_structure(tracked_object, "tracked-object")) == NULL) {
            GST_WARNING_OBJECT(filter, "unable to build structure from tracked object \"%d\"", tracked_object->id);
            continue;
        }

        event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
        gst_pad_push_event(filter->srcpad, event);
    }

    gst_buffer_set_data(buf, (guint8*) filter->image->imageData, (guint) filter->image->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}

static gboolean
gst_static_objects_parse_objects_str(GstStaticObjects *filter)
{
    gchar **object_str;
    guint   i, j;

    // sanity checks
    g_return_val_if_fail(filter->objects_str != NULL, FALSE);

    object_str = g_strsplit(filter->objects_str, ";", -1);
    for (i = 0; object_str[i] != NULL; ++i) {
        TrackedObject *tracked_object;
        gchar        **fields;
        guint          nfields;

        fields  = g_strsplit(object_str[i], ",", -1);
        nfields = g_strv_length(fields);

        // check that the # of fields is
        //   (1) larger than 6 (because we must have at least a label, height,
        //       and two points)
        //   (2) even, as additional points must always have two coordinates
        //       (x and y)
        if ((nfields < 6) || ((nfields % 2) != 0)) {
            GST_WARNING_OBJECT(filter, "invalid object string: \"%s\"", object_str[i]);
            g_strfreev(fields);
            g_strfreev(object_str);
            return FALSE;
        }

        // ensure that the height and points fields are all integers
        for (j = 1; j < nfields; ++j) {
            glong tmp; // just to silence a compiler warning

            errno = 0;
            tmp = strtol(fields[j], NULL, 0);
            if (errno != 0) {
                GST_WARNING_OBJECT(filter, "invalid field (%d) when parsing object \"%s\"", j, object_str[i]);
                g_strfreev(fields);
                g_strfreev(object_str);
                return FALSE;
            }
        }

        // allocate and initialize the tracked object structure
        tracked_object         = tracked_object_new();
        tracked_object->id     = g_strdup(fields[0]);
        tracked_object->type   = TRACKED_OBJECT_STATIC;
        tracked_object->height = strtol(fields[1], NULL, 0);
        for (j = 2; j < nfields; j += 2) {
            tracked_object_add_point(tracked_object,
                                     strtol(fields[j],     NULL, 0),
                                     strtol(fields[j + 1], NULL, 0));

        }
        filter->objects_list = g_list_prepend(filter->objects_list, tracked_object);

        g_strfreev(fields);
    }

    g_strfreev(object_str);
    return TRUE;
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features
gboolean
gst_static_objects_plugin_init(GstPlugin *plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_static_objects_debug, "staticobjects", 0,
                            "Parses static object definitions and push events with the parsed data");

    return gst_element_register(plugin, "staticobjects", GST_RANK_NONE, GST_TYPE_STATIC_OBJECTS);
}
