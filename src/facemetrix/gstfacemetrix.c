/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
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
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace ! facemetrix ! ffmpegcolorspace ! xvimagesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include <highgui.h>

#include "gstfacemetrix.h"

GST_DEBUG_CATEGORY_STATIC (gst_facemetrix_debug);
#define GST_CAT_DEFAULT gst_facemetrix_debug

#define DEFAULT_PROFILE  "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml"
#define DEFAULT_SGL_HOST "localhost"
#define DEFAULT_SGL_PORT 1500

// Filter signals and args
enum
{
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_DISPLAY,
    PROP_PROFILE,
    PROP_SGL_HOST,
    PROP_SGL_PORT
};

// the capabilities of the inputs and outputs.
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-rgb")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-rgb")
    );

GST_BOILERPLATE (GstFacemetrix, gst_facemetrix, GstElement, GST_TYPE_ELEMENT);

static void          gst_facemetrix_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void          gst_facemetrix_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean      gst_facemetrix_set_caps     (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_facemetrix_chain        (GstPad * pad, GstBuffer * buf);
static void          gst_facemetrix_load_profile (GstFacemetrix * filter);

// clean up
static void
gst_facemetrix_finalize (GObject * obj)
{
    GstFacemetrix *filter = GST_FACEMETRIX (obj);

    if (filter->cvImage) {
        cvReleaseImage (&filter->cvImage);
        cvReleaseImage (&filter->cvGray);
    }
    if (filter->sgl != NULL) {
        sgl_client_close(filter->sgl);
        g_object_unref(filter->sgl);
        filter->sgl = NULL;
    }

    G_OBJECT_CLASS (parent_class)->finalize(obj);
}


// gobject vmethod implementations
static void
gst_facemetrix_base_init (gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

    gst_element_class_set_details_simple(element_class,
                                         "facemetrix",
                                         "Filter/Effect/Video",
                                         "Performs face recognition on videos and images, providing positions through buffer metadata",
                                         "Michael Sheldon <mike@mikeasoft.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get (&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get (&sink_factory));
}

// initialize the facemetrix's class
static void
gst_facemetrix_class_init(GstFacemetrixClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class  = g_type_class_peek_parent(klass);

    gobject_class->finalize     = GST_DEBUG_FUNCPTR(gst_facemetrix_finalize);
    gobject_class->set_property = gst_facemetrix_set_property;
    gobject_class->get_property = gst_facemetrix_get_property;

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display",
                                                         "Sets whether the metrixed faces should be highlighted in the output",
                                                         TRUE, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_PROFILE,
                                    g_param_spec_string("profile", "Profile",
                                                        "Location of Haar cascade file to use for face detection",
                                                        DEFAULT_PROFILE, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_SGL_HOST,
                                    g_param_spec_string("sgl-host", "SGL server hostname",
                                                        "Hostname/IP of the SGL server",
                                                        DEFAULT_SGL_HOST, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_SGL_PORT,
                                    g_param_spec_uint("sgl-port", "SGL server port",
                                                      "TCP port used by the SGL server",
                                                      0, USHRT_MAX, DEFAULT_SGL_PORT, G_PARAM_READWRITE));
}

//initialize the new element
//instantiate pads and add them to element
//set pad calback functions
//initialize instance structure
static void
gst_facemetrix_init(GstFacemetrix *filter, GstFacemetrixClass *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR (gst_facemetrix_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR (gst_facemetrix_chain));

    filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT (filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT (filter), filter->srcpad);

    filter->profile = DEFAULT_PROFILE;
    filter->display = TRUE;
    filter->sglhost = DEFAULT_SGL_HOST;
    filter->sglport = DEFAULT_SGL_PORT;
    filter->sgl     = NULL;

    gst_facemetrix_load_profile(filter);
}

static void
gst_facemetrix_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstFacemetrix *filter = GST_FACEMETRIX (object);

    switch (prop_id) {
        case PROP_PROFILE:
            filter->profile = g_value_dup_string(value);
            gst_facemetrix_load_profile(filter);
            break;
        case PROP_DISPLAY:
            filter->display = g_value_get_boolean(value);
            break;
        case PROP_SGL_HOST:
            filter->sglhost = g_value_dup_string(value);
            break;
        case PROP_SGL_PORT:
            filter->sglport = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_facemetrix_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstFacemetrix *filter = GST_FACEMETRIX (object);

    switch (prop_id) {
        case PROP_PROFILE:
            g_value_set_string(value, g_strdup(filter->profile));
            break;
        case PROP_DISPLAY:
            g_value_set_boolean(value, filter->display);
            break;
        case PROP_SGL_HOST:
            g_value_set_string(value, filter->sglhost);
            break;
        case PROP_SGL_PORT:
            g_value_set_uint(value, filter->sglport);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

// GstElement vmethod implementations

// this function handles the link with other elements
static gboolean
gst_facemetrix_set_caps (GstPad * pad, GstCaps * caps)
{
    GstFacemetrix *filter;
    GstPad *otherpad;
    gint width, height;
    GstStructure *structure;

    filter = GST_FACEMETRIX (gst_pad_get_parent (pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    filter->cvImage   = cvCreateImage(cvSize (width, height), IPL_DEPTH_8U, 3);
    filter->cvGray    = cvCreateImage(cvSize (width, height), IPL_DEPTH_8U, 1);
    filter->cvStorage = cvCreateMemStorage(0);

    // initialize sgl connection
    if ((filter->sgl = g_object_new(SGL_CLIENT_TYPE, NULL)) == NULL) {
        GST_WARNING("unable to create sgl client instance");
    } else {
        if (sgl_client_open(filter->sgl, filter->sglhost, filter->sglport) == FALSE) {
            GST_WARNING("unable to connect to sgl server (%s:%u)", filter->sglhost, filter->sglport);
            g_object_unref(filter->sgl);
            filter->sgl = NULL;
        }
    }

    otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);

    return gst_pad_set_caps(otherpad, caps);
}

// chain function - this function does the actual processing
static GstFlowReturn
gst_facemetrix_chain(GstPad *pad, GstBuffer *buf)
{
    GstFacemetrix *filter;
    CvSeq *faces;
    int i;

    filter = GST_FACEMETRIX (GST_OBJECT_PARENT (pad));

    filter->cvImage->imageData = (char *) GST_BUFFER_DATA (buf);

    cvCvtColor(filter->cvImage, filter->cvGray, CV_RGB2GRAY);
    cvClearMemStorage(filter->cvStorage);

    if (filter->cvCascade) {
        faces = cvHaarDetectObjects(filter->cvGray, filter->cvCascade,
                                    filter->cvStorage, 1.1, 2, 0, cvSize (30, 30));

        for (i = 0; i < (faces ? faces->total : 0); i++) {
            // FIXME: select the largest face instead of processing just the first one
            gchar *id = "__NOID__";
            CvRect *r = (CvRect *) cvGetSeqElem(faces, i);

            if (filter->sgl != NULL) {
                CvMat face;

                // FIXME: I don't think the code below works; replace with something more appropriate
                cvGetSubRect(filter->cvImage, &face, *r);
                if (CV_IS_MAT(&face)) {
                    GST_WARNING("CvGetSubRect: unable to grab face sub-image");
                    break;
                }
                CvMat *jpegface = cvEncodeImage("jpg", &face, NULL);
                if (CV_IS_MAT(jpegface)) {
                    GST_WARNING("CvGetSubRect: unable to convert face sub-image to jpeg format");
                    cvDecRefData(&face);
                    break;
                }
                size_t size = jpegface->rows * jpegface->cols * jpegface->step;

                if ((id = sgl_client_recognize(filter->sgl, (gchar*) jpegface->data.ptr, size)) == NULL) {
                    GST_WARNING("[sgl] unable to get user id");
                }

                cvReleaseMat(&jpegface);
                cvDecRefData(&face);
            }

            GstStructure *s = gst_structure_new("face",
                                                "x", G_TYPE_UINT, r->x,
                                                "y", G_TYPE_UINT, r->y,
                                                "width", G_TYPE_UINT, r->width,
                                                "height", G_TYPE_UINT, r->height,
                                                "id", G_TYPE_STRING, id, NULL);

            GstMessage *m = gst_message_new_element(GST_OBJECT (filter), s);
            gst_element_post_message(GST_ELEMENT (filter), m);

            if (filter->display) {
                CvPoint center, center_bottom;
                CvFont font;
                int radius;

                center.x = cvRound((r->x + r->width * 0.5));
                center.y = cvRound((r->y + r->height * 0.5));
                radius = cvRound((r->width + r->height) * 0.25);
                cvCircle(filter->cvImage, center, radius, CV_RGB (255, 32, 32), 3, 8, 0);

                // FIXME: fix the code below
                center_bottom.x = center.x;
                center_bottom.y = center.y > 10 ? center.y - 10 : 0;
                cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 1.0f, 1.0f, 0.0f, 1, 8);
                cvPutText(filter->cvImage, id, center_bottom, &font , CV_RGB(255, 32, 32));
            }
        }
    }

    gst_buffer_set_data(buf, (guchar*) filter->cvImage->imageData, filter->cvImage->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}

static void
gst_facemetrix_load_profile (GstFacemetrix * filter)
{
    filter->cvCascade = (CvHaarClassifierCascade *) cvLoad(filter->profile, 0, 0, 0);
    if (!filter->cvCascade) {
        GST_WARNING("Couldn't load Haar classifier cascade: %s.", filter->profile);
    }
}

// entry point to initialize the plug-in
// initialize the plug-in itself
// register the element factories and other features
gboolean
gst_facemetrix_plugin_init (GstPlugin * plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_facemetrix_debug, "facemetrix", 0,
                            "Performs face recognition on videos and images, providing positions through buffer metadata");

    return gst_element_register(plugin, "facemetrix", GST_RANK_NONE, GST_TYPE_FACEMETRIX);
}
