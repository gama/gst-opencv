/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
 * Copyright (C) 2010 Gustavo Gama <gama@vettalabs.com>
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
 * SECTION:element-facemetrix
 *
 * Performs face recognition using Vetta Labs' Facemetrix server. It depends
 * on 'face' events generated by the 'facedetect' plugin.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace ! facedetect ! facemetrix ! ffmpegcolorspace ! xvimagesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstfacemetrix.h"

#include <gst/gst.h>
#include <cvaux.h>
#include <highgui.h>

GST_DEBUG_CATEGORY_STATIC (gst_facemetrix_debug);
#define GST_CAT_DEFAULT gst_facemetrix_debug

#define DEFAULT_PROFILE       "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml"
#define DEFAULT_SAVE_PREFIX   "/tmp/facemetrix"
#define DEFAULT_MIN_NEIGHBORS  5
#define DEFAULT_MIN_SIZE      20
#define DEFAULT_HOST          "localhost"
#define DEFAULT_PORT          1500
#define DEFAULT_RECOGNIZER_ID "gstfacemetrix"

#define FACE_ID_LABEL_BORDER   4

enum
{
    PROP_0,
    PROP_VERBOSE,
    PROP_DISPLAY,
    PROP_UNKNOWN_FACES,
    PROP_HOST,
    PROP_PORT,
    PROP_RECOGNIZER_ID
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

GST_BOILERPLATE(GstFaceMetrix, gst_facemetrix, GstElement, GST_TYPE_ELEMENT);

static void          gst_facemetrix_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_facemetrix_get_property (GObject * object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean      gst_facemetrix_set_caps     (GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_facemetrix_chain        (GstPad * pad, GstBuffer * buf);
static gboolean      face_events_cb              (GstPad *pad, GstEvent *event, gpointer user_data);
static void          draw_face_id                (IplImage *image, const gchar *face_id, const CvRect face_rect, CvScalar color, float font_scale, gboolean draw_face_box);

// clean up
static void
gst_facemetrix_finalize(GObject *obj)
{
    GstFaceMetrix *filter = GST_FACEMETRIX(obj);

    if (filter->image)         cvReleaseImage(&filter->image);
    if (filter->host)          g_free(filter->host);
    if (filter->recognizer_id) g_free(filter->recognizer_id);
    if (filter->sgl) {
        sgl_client_close(filter->sgl);
        g_object_unref(filter->sgl);
    }

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

// gobject vmethod implementations
static void
gst_facemetrix_base_init(gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "facemetrix",
                                         "Filter/Video",
                                         "Performs face recognition using Vetta Labs' Facemetrix server",
                                         "Gustavo Gama <gama@vettalabs.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the facemetrix's class
static void
gst_facemetrix_class_init(GstFaceMetrixClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class  = g_type_class_peek_parent(klass);

    gobject_class->finalize     = GST_DEBUG_FUNCPTR(gst_facemetrix_finalize);
    gobject_class->set_property = gst_facemetrix_set_property;
    gobject_class->get_property = gst_facemetrix_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose", "Sets whether the movement direction should be printed to the standard output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display", "Highligh the metrixed faces in the video output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_UNKNOWN_FACES,
                                    g_param_spec_boolean("unknown-faces", "Unknown Faces", "Emit events/messages even when the facemetrix server was unable to recognize a detected face",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_HOST,
                                    g_param_spec_string("host", "Facemetrix server hostname", "Hostname/IP of the Facemetrix server",
                                                        DEFAULT_HOST, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_PORT,
                                    g_param_spec_uint("port", "Facemetrix server port", "TCP port used by the Facemetrix server",
                                                      0, USHRT_MAX, DEFAULT_PORT, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_RECOGNIZER_ID,
                                    g_param_spec_string("recognizer-id", "ID of the Facemetrix recognizer", "ID of the facemetrix recognizer. This will be forwarded to the facemetrix server to identify the recognizer instance that should be used to recognize the detected faces",
                                                        DEFAULT_RECOGNIZER_ID, G_PARAM_READWRITE));
}

// initialize the new element
// instantiate pads and add them to element
// set pad calback functions
// initialize instance structure
static void
gst_facemetrix_init(GstFaceMetrix *filter, GstFaceMetrixClass *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_facemetrix_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_facemetrix_chain));

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    filter->verbose        = FALSE;
    filter->display        = FALSE;
    filter->unknown_faces  = FALSE;
    filter->host           = g_strdup(DEFAULT_HOST);
    filter->recognizer_id  = g_strdup(DEFAULT_RECOGNIZER_ID);
    filter->face_timestamp = 0;
    filter->face_array     = g_array_sized_new(FALSE, FALSE, sizeof(CvRect), 1);
}

static void
gst_facemetrix_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstFaceMetrix *filter = GST_FACEMETRIX(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY:
            filter->display = g_value_get_boolean(value);
            break;
        case PROP_UNKNOWN_FACES:
            filter->unknown_faces = g_value_get_boolean(value);
            break;
        case PROP_HOST:
            if (filter->host) g_free(filter->host);
            filter->host = g_value_dup_string(value);
            break;
        case PROP_PORT:
            filter->port = g_value_get_uint(value);
            break;
        case PROP_RECOGNIZER_ID:
            if (filter->recognizer_id) g_free(filter->recognizer_id);
            filter->recognizer_id = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_facemetrix_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstFaceMetrix *filter = GST_FACEMETRIX(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_DISPLAY:
            g_value_set_boolean(value, filter->display);
            break;
        case PROP_UNKNOWN_FACES:
            g_value_set_boolean(value, filter->unknown_faces);
            break;
        case PROP_HOST:
            g_value_set_string(value, filter->host);
            break;
        case PROP_PORT:
            g_value_set_uint(value, filter->port);
            break;
        case PROP_RECOGNIZER_ID:
            g_value_set_string(value, filter->recognizer_id);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

// GstElement vmethod implementations

// this function handles the link with other elements
static gboolean
gst_facemetrix_set_caps(GstPad *pad, GstCaps *caps)
{
    GstFaceMetrix *filter;
    GstPad        *other_pad;
    GstStructure  *structure;
    gint           width, height, depth;

    filter = GST_FACEMETRIX(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width",  &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth",  &depth);

    filter->image   = cvCreateImage(cvSize(width, height), depth/3, 3);

    // initialize sgl connection
    if ((filter->sgl = g_object_new(SGL_CLIENT_TYPE, NULL)) == NULL) {
        GST_WARNING("unable to create sgl client instance");
    } else {
        if (sgl_client_open(filter->sgl, filter->host, filter->port) == FALSE) {
            GST_WARNING("unable to connect to sgl server (%s:%u)", filter->host, filter->port);
            g_object_unref(filter->sgl);
            filter->sgl = NULL;
        }
    }

    // add roi event probe on the sinkpad
    gst_pad_add_event_probe(filter->sinkpad, (GCallback) face_events_cb, filter);

    other_pad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);
    return gst_pad_set_caps(other_pad, caps);
}

// chain function; this function does the actual processing
static GstFlowReturn
gst_facemetrix_chain(GstPad *pad, GstBuffer *buf)
{
    GstFaceMetrix *filter;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_FACEMETRIX(GST_OBJECT_PARENT(pad));

    // if no connection to the facemetrix server could be established, let
    // the pipeline continue as if the facemetrix element was not present
    g_return_val_if_fail(filter->sgl != NULL, GST_FLOW_OK);

    // check face timestamps and face array length; these should have been
    // set at the face_events_cb callback
    if ((filter->face_timestamp == GST_BUFFER_TIMESTAMP(buf)) &&
        (filter->face_array != NULL) &&
        (filter->face_array->len > 0)) {

        guint i;

        filter->image->imageData = (char*) GST_BUFFER_DATA(buf);

        for (i = 0; i < filter->face_array->len; ++i) {
            IplImage     *face_image;
            CvRect        face_rect;
            CvMat        *jpegface;
            GstEvent     *event;
            GstMessage   *message;
            GstStructure *structure;
            gchar        *id;

            face_rect = g_array_index(filter->face_array, CvRect, i);
            face_image = cvCreateImage(cvSize(face_rect.width, face_rect.height), IPL_DEPTH_8U, 3);
            cvSetImageROI(filter->image, face_rect);
            cvCopy(filter->image, face_image, NULL);
            cvResetImageROI(filter->image);

            // the opencv load/saving functions only work on the BGR colorspace
            cvCvtColor(face_image, face_image, CV_RGB2BGR);

            jpegface = cvEncodeImage(".jpg", face_image, NULL);
            cvReleaseImage(&face_image);

            if (!CV_IS_MAT(jpegface)) {
                GST_WARNING("[facemetrix] unable to convert face image to jpeg format");
                continue;
            }

            id = sgl_client_recognize(filter->sgl, filter->recognizer_id, FALSE, (gchar*) jpegface->data.ptr,
                                      jpegface->rows * jpegface->step, NULL, NULL, NULL, NULL);
            cvReleaseMat(&jpegface);

            if ((filter->unknown_faces == FALSE) && ((id == NULL) || (strcmp(id, SGL_UNKNOWN_FACE_ID) == 0)))
                continue;

            if (filter->verbose)
                GST_INFO("[facemetrix] id: %s\n", id == NULL ? SGL_UNKNOWN_FACE_ID : id);

            // send downstream event and bus message with the face info
            structure = gst_structure_new("faceid",
                                          "id",        G_TYPE_STRING, id == NULL ? SGL_UNKNOWN_FACE_ID : id,
                                          "x",         G_TYPE_UINT,   face_rect.x,
                                          "y",         G_TYPE_UINT,   face_rect.y,
                                          "width",     G_TYPE_UINT,   face_rect.width,
                                          "height",    G_TYPE_UINT,   face_rect.height,
                                          "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP(buf),
                                          NULL);

            message = gst_message_new_element(GST_OBJECT(filter), gst_structure_copy(structure));
            gst_element_post_message(GST_ELEMENT(filter), message);

            event   = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
            gst_pad_push_event(filter->srcpad, event);

            if (filter->display) {
                float font_scaling = ((filter->image->width * filter->image->height) > (320 * 240)) ? 0.5f : 0.3f;
                draw_face_id(filter->image, id, face_rect, CV_RGB(0, 255, 0), font_scaling, TRUE);
                gst_buffer_set_data(buf, (guchar*) filter->image->imageData, filter->image->imageSize);
            }

            if (id != NULL)
                g_free(id);
        }
    }

    return gst_pad_push(filter->srcpad, buf);
}

static void
draw_face_id(IplImage *image, const gchar *face_id, const CvRect face_rect,
             CvScalar color, float font_scale, gboolean draw_face_box)
{
    CvFont font;
    CvSize text_size;
    int    baseline;

    // sanity checks
    g_return_if_fail(image != NULL);

    // draw a box around the face, if requested
    if (draw_face_box)
        cvRectangle(image,
                    cvPoint(face_rect.x, face_rect.y),
                    cvPoint(face_rect.x + face_rect.width, face_rect.y + face_rect.height),
                    color, 1, 8, 0);

    if ((face_id == NULL) || (strcmp(face_id, SGL_UNKNOWN_FACE_ID) == 0))
        return;

    // setup font and calculate text size
    cvInitFont(&font, CV_FONT_HERSHEY_DUPLEX, font_scale, font_scale, 0, 1, CV_AA);
    cvGetTextSize(face_id, &font, &text_size, &baseline);

    // the text is drawn using the given color as background and black as foreground
    cvRectangle(image,
                cvPoint(face_rect.x - FACE_ID_LABEL_BORDER, face_rect.y - text_size.height - FACE_ID_LABEL_BORDER),
                cvPoint(face_rect.x + text_size.width + FACE_ID_LABEL_BORDER, face_rect.y + FACE_ID_LABEL_BORDER),
                color, -1, 8, 0);
    cvPutText(image, face_id , cvPoint(face_rect.x, face_rect.y), &font, cvScalarAll(0));
}

// callbacks
static
gboolean face_events_cb(GstPad *pad, GstEvent *event, gpointer user_data)
{
    GstFaceMetrix      *filter;
    const GstStructure *structure;

    filter = GST_FACEMETRIX(user_data);

    // sanity checks
    g_return_val_if_fail(pad    != NULL, FALSE);
    g_return_val_if_fail(event  != NULL, FALSE);
    g_return_val_if_fail(filter != NULL, FALSE);

    structure = gst_event_get_structure(event);
    if ((structure != NULL) && (strcmp(gst_structure_get_name(structure), "haar-detect-roi") == 0)) {
        CvRect face;
        GstClockTime timestamp;

        gst_structure_get((GstStructure*) structure,
                          "x",         G_TYPE_UINT,   &face.x,
                          "y",         G_TYPE_UINT,   &face.y,
                          "width",     G_TYPE_UINT,   &face.width,
                          "height",    G_TYPE_UINT,   &face.height,
                          "timestamp", G_TYPE_UINT64, &timestamp, NULL);

        if (timestamp > filter->face_timestamp) {
            filter->face_timestamp = timestamp;
            g_array_free(filter->face_array, TRUE);
            filter->face_array = g_array_sized_new(FALSE, FALSE, sizeof(CvRect), 1);
        }
        g_array_append_val(filter->face_array, face);
    }

    return TRUE;
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features
gboolean
gst_facemetrix_plugin_init (GstPlugin * plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_facemetrix_debug, "facemetrix", 0,
                            "Performs face recognition using Vetta Labs' Facemetrix server");

    return gst_element_register(plugin, "facemetrix", GST_RANK_NONE, GST_TYPE_FACEMETRIX);
}
