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
 * SECTION:element-interpreterinteraction
 *
 * Performs interpreterinteraction using haardetect. It depends
 * on 'haardetect' events generated by the 'haardetect' plugin.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace !
 *      bgfgcodebook !
 *      facedetect !
 *      objectsareainteraction !
 *      interpreterinteraction verbose=TRUE !
 *      ffmpegcolorspace ! xvimagesink sync=false
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstinterpreterinteraction.h"

#include <gst/gst.h>
#include <gst/gststructure.h>
#include <cvaux.h>
#include <glib-2.0/glib/garray.h>
#include <highgui.h>
#include <glib-2.0/glib/gtypes.h>

GST_DEBUG_CATEGORY_STATIC (gst_interpreter_interaction_debug);
#define GST_CAT_DEFAULT gst_interpreter_interaction_debug

#define OLD_TIMESTAMPDIFF_TO_PROCESS 1000000000LL
#define MIN_PERC_HIT 0.3f

enum
{
    PROP_0,
    PROP_VERBOSE,
    PROP_DISPLAY,
    PROP_DISPLAY_DATA
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

GST_BOILERPLATE(GstInterpreterInteraction, gst_interpreter_interaction, GstElement, GST_TYPE_ELEMENT);

static void          gst_interpreter_interaction_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_interpreter_interaction_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean      gst_interpreter_interaction_set_caps     (GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_interpreter_interaction_chain        (GstPad *pad, GstBuffer *buf);
static gboolean      events_cb                                (GstPad *pad, GstEvent *event, gpointer user_data);
static ObjectList*   include_objects_itens                    (GArray *list, gint id, gint type_1ojb_0area, gchar *name, GstClockTime timestamp, guint sizeof_relation_item);
static void          process_events                           (GstInterpreterInteraction *filter, const guint64 old_timestampdiff_to_process);

// clean up
static void
gst_interpreter_interaction_finalize(GObject *obj)
{
    GstInterpreterInteraction *filter = GST_INTERPRETERINTERACTION(obj);

    if (filter->image)       cvReleaseImage(&filter->image);

    // Processes all remaining events
    process_events(filter, 0);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

// gobject vmethod implementations
static void
gst_interpreter_interaction_base_init(gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "interpreterinteraction",
                                         "Filter/Video",
                                         "Interpret interactions among objects",
                                         "Lucas Pantuza Amorim <lucas@vettalabs.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the interpreterinteraction's class
static void
gst_interpreter_interaction_class_init(GstInterpreterInteractionClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class  = g_type_class_peek_parent(klass);

    gobject_class->finalize     = GST_DEBUG_FUNCPTR(gst_interpreter_interaction_finalize);
    gobject_class->set_property = gst_interpreter_interaction_set_property;
    gobject_class->get_property = gst_interpreter_interaction_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose", "Print useful debugging information to stdout",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display", "Highlight the adjusted ROI on the output video stream",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY_DATA,
                                    g_param_spec_boolean("display-data", "Display data", "Print data structure",
                                                         FALSE, G_PARAM_READWRITE));
}

// initialize the new element
// instantiate pads and add them to element
// set pad calback functions
// initialize instance structure
static void
gst_interpreter_interaction_init(GstInterpreterInteraction *filter, GstInterpreterInteractionClass *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_interpreter_interaction_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_interpreter_interaction_chain));

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    filter->verbose                 = FALSE;
    filter->display                 = FALSE;
    filter->display_data            = FALSE;

    filter->objects_in_scene        = g_array_sized_new(FALSE, FALSE, sizeof(ObjectList), 1);
    filter->event_interaction_in    = g_array_sized_new(FALSE, FALSE, sizeof(EventInteractionIn), 1);
}

static void
gst_interpreter_interaction_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstInterpreterInteraction *filter = GST_INTERPRETERINTERACTION(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY:
            filter->display = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY_DATA:
            filter->display_data = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_interpreter_interaction_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstInterpreterInteraction *filter = GST_INTERPRETERINTERACTION(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_DISPLAY:
            g_value_set_boolean(value, filter->display);
            break;
        case PROP_DISPLAY_DATA:
            g_value_set_boolean(value, filter->display_data);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

// GstElement vmethod implementations

// this function handles the link with other elements
static gboolean
gst_interpreter_interaction_set_caps(GstPad *pad, GstCaps *caps)
{
    GstInterpreterInteraction   *filter;
    GstPad                      *other_pad;
    GstStructure                *structure;
    gint                         width, height, depth;

    filter = GST_INTERPRETERINTERACTION(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width",  &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth",  &depth);

    filter->image   = cvCreateImage(cvSize(width, height), depth/3, 3);

    // add roi event probe on the sinkpad
    gst_pad_add_event_probe(filter->sinkpad, (GCallback) events_cb, filter);

    other_pad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);
    return gst_pad_set_caps(other_pad, caps);
}

// chain function; this function does the actual processing
static GstFlowReturn
gst_interpreter_interaction_chain(GstPad *pad, GstBuffer *buf)
{
    GstInterpreterInteraction *filter;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_INTERPRETERINTERACTION(GST_OBJECT_PARENT(pad));

    filter->image->imageData = (char*) GST_BUFFER_DATA(buf);

    if(filter->display)
        printText(filter->image, cvPoint(filter->image->width/2, 0), "INTERPRETERINTERACTION ACTIVED", COLOR_RED, .5, 1);

    // Update timestamp
    filter->timestamp = GST_BUFFER_TIMESTAMP(buf);

    if ((filter->event_interaction_in != NULL) && (filter->event_interaction_in->len > 0)) {
        gint i;

        for (i = filter->event_interaction_in->len - 1; i >= 0; --i) {

            EventInteractionIn event;
            ObjectList *objectlist_temp;

            event = g_array_index(filter->event_interaction_in, EventInteractionIn, i);

            // Include event A->B
            objectlist_temp = include_objects_itens(filter->objects_in_scene, event.obj_a_id, 1, event.obj_a_name, 0, sizeof (ObjectList));
            objectlist_temp = include_objects_itens(objectlist_temp->relations, event.obj_b_id, event.type, event.obj_b_name, event.timestamp, sizeof (gfloat));
            g_array_append_val(objectlist_temp->relations, event.distance);

            // Include event B->A
            objectlist_temp = include_objects_itens(filter->objects_in_scene, event.obj_b_id, event.type, event.obj_b_name, 0, sizeof (ObjectList));
            objectlist_temp = include_objects_itens(objectlist_temp->relations, event.obj_a_id, 1, event.obj_a_name, event.timestamp, sizeof (gfloat));
            g_array_append_val(objectlist_temp->relations, event.distance);

            g_array_remove_index_fast(filter->event_interaction_in, i);
        }
    }

    // Show the data structure
    if (filter->display_data && filter->objects_in_scene && filter->objects_in_scene->len) {
        guint j, k, m;
        ObjectList *objectlist;
        ObjectList *objectlist2;
        gfloat dist;

        for (j = 0; j < filter->objects_in_scene->len; ++j) {
            objectlist = &g_array_index(filter->objects_in_scene, ObjectList, j);
            GST_INFO("%s (type_1ojb_0area: %i) - time: %lld\n", objectlist->name, objectlist->type_1ojb_0area, objectlist->last_timestamp);

            for (m = 0; m < objectlist->relations->len; ++m) {
                objectlist2 = &g_array_index(objectlist->relations, ObjectList, m);
                GST_INFO("\t%s (type_1ojb_0area: %i) - time: %lld\n", objectlist2->name, objectlist2->type_1ojb_0area, objectlist2->last_timestamp);

                for (k = 0; k < objectlist2->relations->len; ++k) {
                    dist = g_array_index(objectlist2->relations, gfloat, k);
                    GST_INFO("\t\tdist: %1.2f\n", dist);
                }
            }
        }
        GST_INFO("\n");
    }

    // Processes finalized events
    process_events(filter, OLD_TIMESTAMPDIFF_TO_PROCESS);

    gst_buffer_set_data(buf, (guint8*) filter->image->imageData, (guint) filter->image->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}

// callbacks
static
gboolean events_cb(GstPad *pad, GstEvent *event, gpointer user_data)
{
    GstInterpreterInteraction  *filter;
    const GstStructure *structure;

    filter = GST_INTERPRETERINTERACTION(user_data);

    // sanity checks
    g_return_val_if_fail(pad    != NULL, FALSE);
    g_return_val_if_fail(event  != NULL, FALSE);
    g_return_val_if_fail(filter != NULL, FALSE);

    structure = gst_event_get_structure(event);

    if ((structure != NULL) && (strcmp(gst_structure_get_name(structure), "object-areainteraction") == 0)) {

        EventInteractionIn oi;

        gst_structure_get((GstStructure*) structure,

              "obj_a_id",     G_TYPE_UINT,    &oi.obj_a_id,
              "obj_a_name",   G_TYPE_STRING,  &oi.obj_a_name,
              "obj_a_x",      G_TYPE_UINT,    &oi.obj_a_x,
              "obj_a_y",      G_TYPE_UINT,    &oi.obj_a_y,

              "obj_b_id",     G_TYPE_UINT,    &oi.obj_b_id,
              "obj_b_name",   G_TYPE_STRING,  &oi.obj_b_name,
              "obj_b_x",      G_TYPE_UINT,    &oi.obj_b_x,
              "obj_b_y",      G_TYPE_UINT,    &oi.obj_b_y,

              "distance",     G_TYPE_FLOAT,   &oi.distance,
              "type",         G_TYPE_UINT,    &oi.type,
              "timestamp",    G_TYPE_UINT64,  &oi.timestamp,

              NULL);

        g_array_append_val(filter->event_interaction_in, oi);
    }

    return TRUE;
}

static ObjectList *
include_objects_itens(GArray * list, gint id, gint type_1ojb_0area, gchar *name, GstClockTime timestamp, guint sizeof_relation_item)
{
    guint i;
    ObjectList  *objectlist_temp;

    // Find object in list
    for (i = 0; i < list->len; ++i) {
        objectlist_temp = &g_array_index(list, ObjectList, i);
        if (objectlist_temp->id == id && objectlist_temp->type_1ojb_0area == type_1ojb_0area)
            break;
    }

    // If not exist, include. If not, update
    if (i >= list->len) {
        objectlist_temp = (ObjectList *) g_malloc(sizeof (ObjectList));
        objectlist_temp->id = id;
        objectlist_temp->type_1ojb_0area = type_1ojb_0area;
        objectlist_temp->relations = g_array_sized_new(FALSE, FALSE, sizeof_relation_item, 1);
        objectlist_temp->name = g_strdup_printf("%s", name);
        objectlist_temp->last_timestamp = timestamp;
        g_array_append_val(list, *objectlist_temp);
    } else {
        objectlist_temp->last_timestamp = timestamp;
    }

    // Return its point
    return objectlist_temp;
}

static void
process_events(GstInterpreterInteraction *filter, const guint64 old_timestampdiff_to_process) {

    if (filter->objects_in_scene && filter->objects_in_scene->len) {

        gint j, m;
        guint k;
        ObjectList *objectlist;
        ObjectList *objectlist2;

        for (j = filter->objects_in_scene->len - 1; j >= 0; --j) {
            objectlist = &g_array_index(filter->objects_in_scene, ObjectList, j);

            for (m = objectlist->relations->len - 1; m >= 0; --m) {
                objectlist2 = &g_array_index(objectlist->relations, ObjectList, m);

                if (filter->timestamp - objectlist2->last_timestamp >= old_timestampdiff_to_process) {

                    if (objectlist2->relations->len >= 2) {

                        gfloat sum_neg, sum_pos, val_min, val_max, a, b, g, perc_hit;
                        guint i_min;

                        i_min = 0;
                        sum_pos = sum_neg = 0;
                        for (k = 1; k < objectlist2->relations->len; ++k) {
                            a = g_array_index(objectlist2->relations, gfloat, k - 1);
                            b = g_array_index(objectlist2->relations, gfloat, k);
                            g = b - a;

                            if (g < 0) sum_neg += g;
                            else sum_pos += g;

                            if (k == 1) val_min = val_max = a;
                            if (val_min > b) {val_min = b; i_min = k;}
                            if (val_max < b) val_max = b;
                        }
                        perc_hit = (!(sum_neg + sum_pos)) ? 0 : (sum_neg + sum_pos) / (val_max - val_min);

                        if (perc_hit >= MIN_PERC_HIT) {

                            char *label;
                            GstEvent *event;
                            GstMessage *message;
                            GstStructure *structure;

                            // Manual processing of events
                            if (!objectlist->type_1ojb_0area) continue; // area->obj interaction
                            if (
                                    (!objectlist->type_1ojb_0area || !objectlist2->type_1ojb_0area) &&
                                    (sum_pos > sum_neg) &&
                                    g_array_index(objectlist2->relations, gfloat, 0) <= 2
                                    ) {
                                label = g_strdup_printf("walked out the");
                            } else {
                                if (i_min != 0 && i_min != objectlist2->relations->len && perc_hit != 1) {
                                    label = g_strdup_printf("spent by");
                                } else {
                                    if (sum_pos > sum_neg) {
                                        label = g_strdup_printf("departed");
                                    } else {
                                        label = g_strdup_printf("approached");
                                    }
                                }
                            }

                            if (filter->verbose) {
                                GST_INFO("%s '%s' %s (perc_hit: %1.2f)", objectlist->name, label, objectlist2->name, perc_hit);
                            }

                            // Send downstream event
                            structure = gst_structure_new("object-interpreterinteraction",
                                    "obj_a_id",     G_TYPE_UINT,    objectlist->id,
                                    "obj_a_name",   G_TYPE_STRING,  objectlist->name,
                                    "obj_b_id",     G_TYPE_UINT,    objectlist2->id,
                                    "obj_b_name",   G_TYPE_STRING,  objectlist2->name,
                                    "event",        G_TYPE_STRING,  label,
                                    "timestamp",    G_TYPE_UINT64,  objectlist2->last_timestamp,
                                    "perc_hit",     G_TYPE_FLOAT,   perc_hit,
                                    NULL);

                            message = gst_message_new_element(GST_OBJECT(filter), gst_structure_copy(structure));
                            gst_element_post_message(GST_ELEMENT(filter), message);
                            event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
                            gst_pad_push_event(filter->srcpad, event);

                            g_free(label);
                        }
                    }

                    g_array_free(objectlist2->relations, TRUE);
                    g_array_remove_index_fast(objectlist->relations, m);
                }
            }
        }
    }
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features
gboolean
gst_interpreter_interaction_plugin_init (GstPlugin * plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_interpreter_interaction_debug, "interpreterinteraction", 0,
                            "Interpret interactions among objects");

    return gst_element_register(plugin, "interpreterinteraction", GST_RANK_NONE, GST_TYPE_INTERPRETERINTERACTION);
}