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
 * SECTION:element-objectsareainteraction
 *
 * Performs face recognition using Vetta Labs' Facemetrix server. It depends
 * on 'object-tracking' events generated by the 'objectstracker' plugin.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace !
 *      bgfgcodebook roi=true model-min=40 model-max=50 !
 *      haardetect profile=/apps/opencv/opencv/data/haarcascades/haarcascade_mcs_upperbody.xml min-size=30 min-neighbors=5 roi-only=true verbose=true !
 *      haaradjust display=TRUE !
 *      objectstracker verbose=true display=true !
 *      objectsareainteraction verbose=true display=true display-area=true display-object=true contours=390x126,390x364,400x393,400x126 !
 *      ffmpegcolorspace ! xvimagesink sync=false
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gstobjectsareainteraction.h>

#include <gst/gst.h>
#include <gst/gststructure.h>
#include <cvaux.h>
#include <highgui.h>

GST_DEBUG_CATEGORY_STATIC(gst_objectsareainteraction_debug);
#define GST_CAT_DEFAULT gst_objectsareainteraction_debug

enum {
    PROP_0,
    PROP_VERBOSE,
    PROP_CONTOURS,
    PROP_DISPLAY,
    PROP_DISPLAY_AREA,
    PROP_DISPLAY_OBJECT
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

GST_BOILERPLATE(GstObjectsAreaInteraction, gst_objectsareainteraction, GstElement, GST_TYPE_ELEMENT);

static void gst_objectsareainteraction_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_objectsareainteraction_get_property(GObject * object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean gst_objectsareainteraction_set_caps(GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_objectsareainteraction_chain(GstPad * pad, GstBuffer * buf);
static gboolean events_cb(GstPad *pad, GstEvent *event, gpointer user_data);

static void
gst_objectsareainteraction_finalize(GObject *obj) {
    GstObjectsAreaInteraction *filter = GST_OBJECTSAREAINTERACTION(obj);
    if (filter->image) cvReleaseImage(&filter->image);
    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

// gobject vmethod implementations

static void
gst_objectsareainteraction_base_init(gpointer gclass) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
            "objectsareainteraction",
            "Filter/Video",
            "Performs objects interaction",
            "Lucas Pantuza Amorim <lucas@vettalabs.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the objectsareainteraction's class

static void
gst_objectsareainteraction_class_init(GstObjectsAreaInteractionClass *klass) {
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_objectsareainteraction_finalize);
    gobject_class->set_property = gst_objectsareainteraction_set_property;
    gobject_class->get_property = gst_objectsareainteraction_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
            g_param_spec_boolean("verbose", "Verbose", "Sets whether the movement direction should be printed to the standard output",
            FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
            g_param_spec_boolean("display", "Display", "Highligh the interations in the video output",
            FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY_AREA,
            g_param_spec_boolean("display-area", "Display area", "Highligh the settled areas in the video output",
            FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY_OBJECT,
            g_param_spec_boolean("display-object", "Display object", "Highligh the objects tracker contours in the video output",
            FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CONTOURS,
            g_param_spec_string("contours", "Contours", "Settled contours",
            NULL, G_PARAM_READWRITE));
}

// initialize the new element
// instantiate pads and add them to element
// set pad calback functions
// initialize instance structure

static void
gst_objectsareainteraction_init(GstObjectsAreaInteraction *filter, GstObjectsAreaInteractionClass *gclass) {
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_objectsareainteraction_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_objectsareainteraction_chain));

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    filter->verbose = FALSE;
    filter->display = FALSE;
    filter->display_area = FALSE;
    filter->display_object = FALSE;
    filter->contours = NULL;
    filter->timestamp = 0;
    filter->contours_area_settled = g_array_new(FALSE, FALSE, sizeof (InstanceObjectAreaContour));
    filter->contours_area_in = g_array_new(FALSE, FALSE, sizeof (InstanceObjectAreaContour));
}

static void
gst_objectsareainteraction_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstObjectsAreaInteraction *filter = GST_OBJECTSAREAINTERACTION(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY:
            filter->display = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY_AREA:
            filter->display_area = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY_OBJECT:
            filter->display_object = g_value_get_boolean(value);
            break;
        case PROP_CONTOURS:
            filter->contours = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_objectsareainteraction_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstObjectsAreaInteraction *filter = GST_OBJECTSAREAINTERACTION(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_DISPLAY:
            g_value_set_boolean(value, filter->display);
            break;
        case PROP_DISPLAY_AREA:
            g_value_set_boolean(value, filter->display_area);
            break;
        case PROP_DISPLAY_OBJECT:
            g_value_set_boolean(value, filter->display_object);
            break;
        case PROP_CONTOURS:
            g_value_take_string(value, filter->contours);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static gboolean
gst_objectsareainteraction_set_caps(GstPad *pad, GstCaps *caps) {
    GstObjectsAreaInteraction *filter;
    GstPad *other_pad;
    GstStructure *structure;
    gint width, height, depth;

    filter = GST_OBJECTSAREAINTERACTION(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth", &depth);

    filter->image = cvCreateImage(cvSize(width, height), depth / 3, 3);

    // add roi event probe on the sinkpad
    gst_pad_add_event_probe(filter->sinkpad, (GCallback) events_cb, filter);

    other_pad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);

    // Set settled contours
    if (filter->contours != NULL) {
        gchar **str_area = NULL;
        str_area = g_strsplit(filter->contours, "-", -1);
        g_assert(str_area);

        gint i;
        for(i = 0; str_area[i]; ++i){

            gchar **str_labelpts = NULL;
            str_labelpts = g_strsplit(str_area[i], ":", 2);
            g_assert(str_labelpts[0] && str_labelpts[1]);

            InstanceObjectAreaContour contour_temp;
            contour_temp.mem_storage = cvCreateMemStorage(0);
            makeContour(str_labelpts[1], &contour_temp.contour, contour_temp.mem_storage);
            contour_temp.id = i;
            contour_temp.name = g_strdup_printf(str_labelpts[0]);

            g_array_append_val(filter->contours_area_settled, contour_temp);

            g_strfreev(str_labelpts);
        }

        g_strfreev(str_area);
    }

    return gst_pad_set_caps(other_pad, caps);
}

// chain function; this function does the actual processing

static GstFlowReturn
gst_objectsareainteraction_chain(GstPad *pad, GstBuffer *buf) {
    GstObjectsAreaInteraction *filter;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_OBJECTSAREAINTERACTION(GST_OBJECT_PARENT(pad));
    filter->image->imageData = (char*) GST_BUFFER_DATA(buf);

    // Draw objects contour
    if ((filter->display_object) && (filter->contours_area_in != NULL) && (filter->contours_area_in->len > 0)){
        int i;
        InstanceObjectAreaContour *obj;
        for (i = 0; i < filter->contours_area_in->len; ++i) {
            obj = &g_array_index(filter->contours_area_in, InstanceObjectAreaContour, i);
            cvDrawContours(filter->image, obj->contour, PRINT_COLOR_OBJCONTOUR, PRINT_COLOR_OBJCONTOUR, 0, PRINT_LINE_SIZE_OBJCONTOUR, 8, cvPoint(0, 0));
        }
    }

    // Draw settled area contour
    if ((filter->display_area) && (filter->contours_area_settled != NULL) && (filter->contours_area_settled->len > 0)){
        int i;
        InstanceObjectAreaContour *obj;
        for (i = 0; i < filter->contours_area_settled->len; ++i) {
            obj = &g_array_index(filter->contours_area_settled, InstanceObjectAreaContour, i);
            cvDrawContours(filter->image, obj->contour, PRINT_COLOR_AREACONTOUR, PRINT_COLOR_AREACONTOUR, 0, PRINT_LINE_SIZE_AREACONTOUR, 8, cvPoint(0, 0));
        }
    }

    // Process all objects
    if ((filter->contours_area_in != NULL) && (filter->contours_area_in->len > 0) && (filter->contours_area_settled != NULL) && (filter->contours_area_settled->len > 0)) {

        // Find interceptions rects pairs
        int i, j;
        InstanceObjectAreaContour *obj_settled, *obj_in;
        for (i = 0; i < filter->contours_area_settled->len; ++i) {
            obj_settled = &g_array_index(filter->contours_area_settled, InstanceObjectAreaContour, i);

            for (j = 0; j < filter->contours_area_in->len; ++j) {
                obj_in = &g_array_index(filter->contours_area_in, InstanceObjectAreaContour, j);

                // Process the interception contour
                InstanceObjectAreaContour contour_interception;
                contour_interception.mem_storage = cvCreateMemStorage(0);
                calcInterception(obj_settled, obj_in, &contour_interception);
                if (contour_interception.contour != NULL) {

                    CvRect rect = cvBoundingRect(contour_interception.contour, 1);
                    int interception = (int)(((gdouble) cvContourArea(contour_interception.contour, CV_WHOLE_SEQ) / cvContourArea(obj_in->contour, CV_WHOLE_SEQ))*100);

                    if (filter->display) {
                        cvDrawContours(filter->image, contour_interception.contour, PRINT_COLOR_INTCONTOUR, PRINT_COLOR_INTCONTOUR, 0, PRINT_LINE_SIZE_INTCONTOUR, 8, cvPoint(0, 0));
                        char *label;
                        float font_scaling = ((filter->image->width * filter->image->height) > (320 * 240)) ? 0.5f : 0.3f;
                        label = g_strdup_printf("OBJ#%i in '%s' (%i%%)", obj_in->id, obj_settled->name, interception);
                        printText(filter->image, cvPoint(rect.x + (rect.width / 2), rect.y + (rect.height / 2)), label, PRINT_COLOR_INTCONTOUR, font_scaling, 1);
                        g_free(label);
                    }

                    if (filter->verbose){
                        GST_INFO("OBJ#%i in '%s' (%i%%): rect(%i, %i, %i, %i)\n",
                            obj_in->id, obj_settled->name, interception,
                            rect.x, rect.y, rect.width, rect.height);
                    }

                    // Send downstream event and bus message with the rect info
                    GstEvent *event;
                    GstMessage *message;
                    GstStructure *structure;
                    structure = gst_structure_new("object-areainteraction",
                            "obj_in_id", G_TYPE_UINT, obj_in->id,
                            "obj_settled_name", G_TYPE_STRING, obj_settled->name,
                            "percentage", G_TYPE_UINT, interception,
                            "x", G_TYPE_UINT, rect.x,
                            "y", G_TYPE_UINT, rect.y,
                            "width", G_TYPE_UINT, rect.width,
                            "height", G_TYPE_UINT, rect.height,
                            "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP(buf),
                            NULL);
                    message = gst_message_new_element(GST_OBJECT(filter), gst_structure_copy(structure));
                    gst_element_post_message(GST_ELEMENT(filter), message);
                    event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
                    gst_pad_push_event(filter->srcpad, event);
                }

                // Clear 'contour_interception'
                if (contour_interception.contour != NULL) cvClearSeq(contour_interception.contour);
                cvReleaseMemStorage(&contour_interception.mem_storage);
            }
        }
    }

    // Clean objects

    int k;
    for (k = filter->contours_area_in->len - 1; k >= 0; --k) {
        InstanceObjectAreaContour *object;
        object = &g_array_index(filter->contours_area_in, InstanceObjectAreaContour, k);
        if (object->contour != NULL) cvClearSeq(object->contour);
        cvReleaseMemStorage(&object->mem_storage);
        g_array_remove_index_fast(filter->contours_area_in, k);
    }

    gst_buffer_set_data(buf, (guint8*) filter->image->imageData, (guint) filter->image->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}


// callbacks

static
gboolean events_cb(GstPad *pad, GstEvent *event, gpointer user_data) {
    GstObjectsAreaInteraction *filter;
    const GstStructure *structure;

    filter = GST_OBJECTSAREAINTERACTION(user_data);

    // sanity checks
    g_return_val_if_fail(pad != NULL, FALSE);
    g_return_val_if_fail(event != NULL, FALSE);
    g_return_val_if_fail(filter != NULL, FALSE);

    structure = gst_event_get_structure(event);

    if ((structure != NULL) && (strcmp(gst_structure_get_name(structure), "object-tracking") == 0)) {
        InstanceObjectAreaContour contour_temp;
        GstClockTime timestamp;
        CvRect rect;

        gst_structure_get((GstStructure*) structure,
                "id", G_TYPE_UINT, &contour_temp.id,
                "x", G_TYPE_UINT, &rect.x,
                "y", G_TYPE_UINT, &rect.y,
                "width", G_TYPE_UINT, &rect.width,
                "height", G_TYPE_UINT, &rect.height,
                "timestamp", G_TYPE_UINT64, &timestamp, NULL);

        CvContour header;
        CvSeqBlock block;
        CvMat* vector = cvCreateMat(1, 4, CV_32SC2); // rect = 4 points
        CV_MAT_ELEM(*vector, CvPoint, 0, 0) = cvPoint(rect.x, rect.y);
        CV_MAT_ELEM(*vector, CvPoint, 0, 1) = cvPoint(rect.x, rect.y+rect.height);
        CV_MAT_ELEM(*vector, CvPoint, 0, 2) = cvPoint(rect.x+rect.width, rect.y+rect.height);
        CV_MAT_ELEM(*vector, CvPoint, 0, 3) = cvPoint(rect.x+rect.width, rect.y);
        contour_temp.mem_storage = cvCreateMemStorage(0);
        contour_temp.contour = cvCloneSeq(cvPointSeqFromMat(CV_SEQ_KIND_CURVE + CV_SEQ_FLAG_CLOSED, vector, &header, &block), contour_temp.mem_storage);

        g_array_append_val(filter->contours_area_in, contour_temp);

        if (timestamp > filter->timestamp)
            filter->timestamp = timestamp;
    }

    return TRUE;
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features

gboolean
gst_objectsareainteraction_plugin_init(GstPlugin * plugin) {
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_objectsareainteraction_debug, "objectsareainteraction", 0,
            "Performs objects interaction");

    return gst_element_register(plugin, "objectsareainteraction", GST_RANK_NONE, GST_TYPE_OBJECTSAREAINTERACTION);
}
