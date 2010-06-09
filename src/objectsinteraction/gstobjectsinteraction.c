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
 * SECTION:element-objectsinteraction
 *
 * Performs face recognition using Vetta Labs' Facemetrix server. It depends
 * on 'object-tracking' events generated by the 'objectstracker' plugin.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace     !
 *                 bgfgcodebook roi=true                           !
 *                 haardetect                                      !
 *                 haaradjust                                      !
 *                 surftracker                                     !
 *                 objectsinteraction verbose=true display=true    !
 *                 ffmpegcolorspace ! xvimagesink sync=false
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gstobjectsinteraction.h>

#include <gst/gst.h>
#include <gst/gststructure.h>
#include <cvaux.h>
#include <highgui.h>

#define PRINT_COLOR CV_RGB(100, 185, 185)

GST_DEBUG_CATEGORY_STATIC(gst_objectsinteraction_debug);
#define GST_CAT_DEFAULT gst_objectsinteraction_debug

typedef struct _InstanceObjectIn InstanceObjectIn;
struct _InstanceObjectIn
{
    gint   id;
    CvRect rect;
};

enum {
    PROP_0,
    PROP_VERBOSE,
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

GST_BOILERPLATE(GstObjectsInteraction, gst_objectsinteraction, GstElement, GST_TYPE_ELEMENT);

static void          gst_objectsinteraction_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_objectsinteraction_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean      gst_objectsinteraction_set_caps     (GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_objectsinteraction_chain        (GstPad *pad, GstBuffer *buf);
static gboolean      events_cb                           (GstPad *pad, GstEvent *event, gpointer user_data);

static void
gst_objectsinteraction_finalize(GObject *obj)
{
    GstObjectsInteraction *filter = GST_OBJECTSINTERACTION(obj);
    if (filter->image) cvReleaseImage(&filter->image);
    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

// gobject vmethod implementations

static void
gst_objectsinteraction_base_init(gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "objectsinteraction",
                                         "Filter/Video",
                                         "Performs objects interaction",
                                         "Lucas Pantuza Amorim <lucas@vettalabs.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the objectsinteraction's class

static void
gst_objectsinteraction_class_init(GstObjectsInteractionClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_objectsinteraction_finalize);
    gobject_class->set_property = gst_objectsinteraction_set_property;
    gobject_class->get_property = gst_objectsinteraction_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose",
                                                         "Verbose", "Sets whether the movement direction should be printed to the standard output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display",
                                                         "Highligh the metrixed faces in the video output",
                                                         FALSE, G_PARAM_READWRITE));
}

// initialize the new element
// instantiate pads and add them to element
// set pad calback functions
// initialize instance structure

static void
gst_objectsinteraction_init(GstObjectsInteraction *filter, GstObjectsInteractionClass *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_objectsinteraction_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_objectsinteraction_chain));

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    filter->verbose = FALSE;
    filter->display = FALSE;
    filter->rect_timestamp = 0;
    filter->object_in_array = g_array_new(FALSE, FALSE, sizeof(InstanceObjectIn));
}

static void
gst_objectsinteraction_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstObjectsInteraction *filter = GST_OBJECTSINTERACTION(object);

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
gst_objectsinteraction_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstObjectsInteraction *filter = GST_OBJECTSINTERACTION(object);

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
gst_objectsinteraction_set_caps(GstPad *pad, GstCaps *caps)
{
    GstObjectsInteraction *filter;
    GstPad                *other_pad;
    GstStructure          *structure;
    gint                   width, height, depth;

    filter = GST_OBJECTSINTERACTION(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth", &depth);

    filter->image = cvCreateImage(cvSize(width, height), depth / 3, 3);

    // add roi event probe on the sinkpad
    gst_pad_add_event_probe(filter->sinkpad, (GCallback) events_cb, filter);

    other_pad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);
    return gst_pad_set_caps(other_pad, caps);
}

// chain function; this function does the actual processing

static GstFlowReturn
gst_objectsinteraction_chain(GstPad *pad, GstBuffer *buf)
{
    GstObjectsInteraction *filter;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_OBJECTSINTERACTION(GST_OBJECT_PARENT(pad));
    filter->image->imageData = (char*) GST_BUFFER_DATA(buf);

    // Process all objects
    if ((filter->object_in_array != NULL) && (filter->object_in_array->len > 0)) {
        // Find interceptions rects pairs
        guint i, j;

        for (i = 0; i < filter->object_in_array->len; ++i) {
            for (j = i + 1; j < filter->object_in_array->len; ++j) {
                InstanceObjectIn obj_a, obj_b;
                gint             interception;

                obj_a = g_array_index(filter->object_in_array, InstanceObjectIn, i);
                obj_b = g_array_index(filter->object_in_array, InstanceObjectIn, j);
                interception = 100 * MIN(rectIntercept(&obj_a.rect, &obj_b.rect), rectIntercept(&obj_b.rect, &obj_a.rect));

                if (interception) {
                    GstEvent     *event;
                    GstMessage   *message;
                    GstStructure *structure;
                    CvRect        rect;

                    // Interception percentage
                    rect = rectIntersection(&obj_a.rect, &obj_b.rect);

                    if (filter->verbose)
                        GST_INFO_OBJECT(filter, "INTERCEPTION %i%%: rect_a(%i, %i, %i, %i), rect_b(%i, %i, %i, %i), rect_intercept(%i, %i, %i, %i)\n",
                                        interception,
                                        obj_a.rect.x, obj_a.rect.y, obj_a.rect.width, obj_a.rect.height,
                                        obj_b.rect.x, obj_b.rect.y, obj_b.rect.width, obj_b.rect.height,
                                        rect.x, rect.y, rect.width, rect.height);

                    // Draw intercept rect and label
                    if (filter->display) {
                        char *label;
                        float font_scaling;

                        cvRectangle(filter->image,
                                    cvPoint(rect.x, rect.y),
                                    cvPoint(rect.x + rect.width, rect.y + rect.height),
                                    PRINT_COLOR, -1, 8, 0);
                        font_scaling = ((filter->image->width * filter->image->height) > (320 * 240)) ? 0.5f : 0.3f;
                        label = g_strdup_printf("%i+%i (%i%%)", obj_a.id, obj_b.id, interception);
                        printText(filter->image, cvPoint(rect.x + (rect.width / 2), rect.y + (rect.height / 2)), label, PRINT_COLOR, font_scaling, 1);
                        g_free(label);
                    }

                    // Send downstream event and bus message with the rect info
                    structure = gst_structure_new("object-interaction",
                                                  "id_a",       G_TYPE_UINT,   obj_a.id,
                                                  "id_b",       G_TYPE_UINT,   obj_b.id,
                                                  "percentage", G_TYPE_UINT,   interception,
                                                  "x",          G_TYPE_UINT,   rect.x,
                                                  "y",          G_TYPE_UINT,   rect.y,
                                                  "width",      G_TYPE_UINT,   rect.width,
                                                  "height",     G_TYPE_UINT,   rect.height,
                                                  "timestamp",  G_TYPE_UINT64, GST_BUFFER_TIMESTAMP(buf),
                                                  NULL);
                    message = gst_message_new_element(GST_OBJECT(filter), gst_structure_copy(structure));
                    gst_element_post_message(GST_ELEMENT(filter), message);
                    event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
                    gst_pad_push_event(filter->srcpad, event);

                }
            }
        }

    }

    // Clean objects
    g_array_free(filter->object_in_array, TRUE);
    filter->object_in_array = g_array_sized_new(FALSE, FALSE, sizeof(InstanceObjectIn), 1);

    gst_buffer_set_data(buf, (guint8*) filter->image->imageData, (guint) filter->image->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}


// callbacks

static
gboolean
events_cb(GstPad *pad, GstEvent *event, gpointer user_data)
{
    GstObjectsInteraction *filter;
    const GstStructure    *structure;

    filter = GST_OBJECTSINTERACTION(user_data);

    // sanity checks
    g_return_val_if_fail(pad != NULL, FALSE);
    g_return_val_if_fail(event != NULL, FALSE);
    g_return_val_if_fail(filter != NULL, FALSE);

    structure = gst_event_get_structure(event);

    if ((structure != NULL) && (strcmp(gst_structure_get_name(structure), "object-tracking") == 0)) {
        InstanceObjectIn object_in;
        GstClockTime     timestamp;

        gst_structure_get((GstStructure*) structure,
                          "id",        G_TYPE_UINT,   &object_in.id,
                          "x",         G_TYPE_UINT,   &object_in.rect.x,
                          "y",         G_TYPE_UINT,   &object_in.rect.y,
                          "width",     G_TYPE_UINT,   &object_in.rect.width,
                          "height",    G_TYPE_UINT,   &object_in.rect.height,
                          "timestamp", G_TYPE_UINT64, &timestamp, NULL);

        if (timestamp > filter->rect_timestamp) {
            filter->rect_timestamp = timestamp;
        }
        g_array_append_val(filter->object_in_array, object_in);
    }

    return TRUE;
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features

gboolean
gst_objectsinteraction_plugin_init(GstPlugin *plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_objectsinteraction_debug, "objectsinteraction", 0,
                            "Performs objects interaction");

    return gst_element_register(plugin, "objectsinteraction", GST_RANK_NONE, GST_TYPE_OBJECTSINTERACTION);
}