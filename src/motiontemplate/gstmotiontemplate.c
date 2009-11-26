/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
 * Copyright (C) 2009 Gustavo Machado C. Gama <gama@vettalabs.com>
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
 * SECTION:element-motion_template
 *
 * FIXME:Describe motion_template here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace ! motiontemplate ! ffmpegcolorspace ! xvimagesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib/gprintf.h>
#include <gst/gst.h>

#include "gstmotiontemplate.h"

GST_DEBUG_CATEGORY_STATIC(gst_motion_template_debug);
#define GST_CAT_DEFAULT gst_motion_template_debug

// transition matrix F describes model parameters at and k and k+1
static const float F[] = { 1, 1, 0, 1 };

#define N_FRAMES_HISTORY 4  // (should, probably, depend on FPS)
#define DIFF_THRESHOLD   30
#define MHI_DURATION     1
#define MAX_TIME_DELTA   0.5
#define MIN_TIME_DELTA   0.05

enum {
    PROP_0,
    PROP_VERBOSE
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

GST_BOILERPLATE(GstMotionTemplate, gst_motion_template, GstElement, GST_TYPE_ELEMENT);

static void gst_motion_template_set_property(GObject * object, guint prop_id,
                                           const GValue * value, GParamSpec * pspec);
static void gst_motion_template_get_property(GObject * object, guint prop_id,
                                           GValue * value, GParamSpec * pspec);

static gboolean gst_motion_template_set_caps(GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_motion_template_chain(GstPad * pad, GstBuffer * buf);

/* Clean up */
static void
gst_motion_template_finalize(GObject * obj)
{
    guint i;
    GstMotionTemplate *filter = GST_MOTION_TEMPLATE(obj);

    if (filter->motion)  cvReleaseImage(&filter->motion);
    if (filter->mhi)     cvReleaseImage(&filter->mhi);
    if (filter->orient)  cvReleaseImage(&filter->orient);
    if (filter->segmask) cvReleaseImage(&filter->segmask);
    if (filter->mask)    cvReleaseImage(&filter->mask);
    if (filter->storage) cvReleaseMemStorage(&filter->storage);
    for (i = 0; i < N_FRAMES_HISTORY; ++i)
        if (filter->buf[i]) cvReleaseImage(&(filter->buf[i]));

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}


/* GObject vmethod implementations */
static void
gst_motion_template_base_init(gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "motiontemplate",
                                         "Filter/Effect/Video",
                                         "Track horizontal movement using the OpenCV motion template example",
                                         "Gustavo Machado C. Gama <gam@vettalabs.com>");

    gst_element_class_add_pad_template(element_class,
                                        gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class,
                                        gst_static_pad_template_get(&sink_factory));
}

/* initialize the motion_template's class */
static void
gst_motion_template_class_init(GstMotionTemplateClass * klass)
{
    GObjectClass *gobject_class = (GObjectClass *) klass;

    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_motion_template_finalize);
    gobject_class->set_property = gst_motion_template_set_property;
    gobject_class->get_property = gst_motion_template_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose", "Sets whether the movement direction should be printed to the standard output.",
                                                         TRUE, G_PARAM_READWRITE));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_motion_template_init(GstMotionTemplate * filter, GstMotionTemplateClass * gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad,
                                 GST_DEBUG_FUNCPTR(gst_motion_template_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad,
                                 GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad,
                               GST_DEBUG_FUNCPTR(gst_motion_template_chain));

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad,
                                 GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    // set default properties
    filter->verbose = TRUE;
    filter->buf_idx = 0;
}

static void
gst_motion_template_set_property(GObject *object, guint prop_id,
                               const GValue *value, GParamSpec *pspec)
{
    GstMotionTemplate *filter = GST_MOTION_TEMPLATE(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_motion_template_get_property(GObject * object, guint prop_id,
                               GValue * value, GParamSpec * pspec)
{
    GstMotionTemplate *filter = GST_MOTION_TEMPLATE(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_motion_template_set_caps(GstPad * pad, GstCaps * caps)
{
    GstMotionTemplate *filter;
    GstPad *otherpad;
    gint width, height, i;
    GstStructure *structure;

    filter = GST_MOTION_TEMPLATE(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    // initialize opencv data structures
    filter->motion  = cvCreateImage(cvSize(width, height), 8, 3);
    cvZero(filter->motion);

    filter->mhi     = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 1);
    cvZero(filter->mhi);

    filter->orient  = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 1);
    filter->segmask = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 1);
    filter->mask    = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U,  1);
    filter->storage = cvCreateMemStorage(0);

    filter->buf     = g_new0(IplImage*, N_FRAMES_HISTORY);
    for (i = 0; i < N_FRAMES_HISTORY; ++i) {
        filter->buf[i] = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1); 
        cvZero(filter->buf[i]);
    }

    otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);

    return gst_pad_set_caps(otherpad, caps);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_motion_template_chain(GstPad *pad, GstBuffer *buf)
{
    GstMotionTemplate *filter;
    double             timestamp, avg_x_delta, avg_y_delta;
    IplImage          *image, *silh;
    CvSeq             *seq;
    guint              prev_buf_idx, n_components;
    gint               i;

    filter = GST_MOTION_TEMPLATE(GST_OBJECT_PARENT(pad));

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(filter->mhi != NULL, GST_FLOW_ERROR);

    timestamp = ((double) clock()) / ((double) CLOCKS_PER_SEC); // get current time in seconds

    image = cvCreateImage(cvSize(filter->mhi->width, filter->mhi->height), 8, 3);
    image->imageData = (gchar*) GST_BUFFER_DATA(buf);

    prev_buf_idx = filter->buf_idx;
    filter->buf_idx = (filter->buf_idx + 1) % N_FRAMES_HISTORY;

    cvCvtColor(image, filter->buf[prev_buf_idx], CV_BGR2GRAY); // convert frame to grayscale

    silh = filter->buf[filter->buf_idx];
    cvAbsDiff(filter->buf[prev_buf_idx], filter->buf[filter->buf_idx], silh); // get difference between frames
    cvThreshold(silh, silh, DIFF_THRESHOLD, 1, CV_THRESH_BINARY);             // and threshold it
    cvUpdateMotionHistory(silh, filter->mhi, timestamp, MHI_DURATION);        // then update the motion history

    // convert MHI to blue 8u image
    cvCvtScale(filter->mhi, filter->mask, 255.0 / MHI_DURATION, (MHI_DURATION - timestamp) * 255.0 / MHI_DURATION);
    filter->motion->origin = image->origin;
    cvZero(filter->motion);
    cvMerge(filter->mask, 0, 0, 0, filter->motion);

    // calculate motion gradient orientation and valid orientation mask
    cvCalcMotionGradient(filter->mhi, filter->mask, filter->orient, MAX_TIME_DELTA, MIN_TIME_DELTA, 3);

    // segment motion: get sequence of motion components
    // segmask is marked motion components map. It is not used further
    cvClearMemStorage(filter->storage);
    seq = cvSegmentMotion(filter->mhi, filter->segmask, filter->storage, timestamp, MAX_TIME_DELTA);

    // iterate through the motion components,
    // one more iteration (i == -1) corresponds to the whole image (global motion)
    avg_x_delta = avg_y_delta = 0.0;
    n_components = 0;
    for (i = -1; i < seq->total; ++i) {
        CvRect comp_rect;
        double count, angle, magnitude, x_delta, y_delta;
        CvPoint center;
        CvScalar color;

        if (i < 0) {
            // the whole image
            comp_rect = cvRect(0, 0, image->width, image->height);
            color     = CV_RGB(255, 255, 255);
            magnitude = 100.0;
        } else {
            // i-th motion component
            comp_rect = ((CvConnectedComp*) cvGetSeqElem(seq, i))->rect;
            if (comp_rect.width + comp_rect.height < 100) // reject very small components
                continue;
            color = CV_RGB(255,0,0);
            magnitude = 30.0;
        }

        // select component ROI
        cvSetImageROI(silh,           comp_rect);
        cvSetImageROI(filter->mhi,    comp_rect);
        cvSetImageROI(filter->orient, comp_rect);
        cvSetImageROI(filter->mask,   comp_rect);

        // calculate orientation
        angle = cvCalcGlobalOrientation(filter->orient, filter->mask, filter->mhi, timestamp, MHI_DURATION);
        angle = 360.0 - angle;  // adjust for images with top-left origin

        count = cvNorm(silh, 0, CV_L1, 0); // calculate number of points within silhouette ROI

        cvResetImageROI(filter->mhi);
        cvResetImageROI(filter->orient);
        cvResetImageROI(filter->mask);
        cvResetImageROI(silh);

        // check for the case of little motion
        if (count < comp_rect.width * comp_rect.height * 0.05)
            continue;

        // draw a clock with arrow indicating the direction
        center = cvPoint((comp_rect.x + comp_rect.width / 2),
                         (comp_rect.y + comp_rect.height / 2));

        cvCircle(filter->motion, center, cvRound(magnitude * 1.2), color, 3, CV_AA, 0);

        x_delta = cos(angle * CV_PI / 180.0);
        y_delta = sin(angle * CV_PI / 180.0);
        n_components++;
        cvLine(filter->motion, center, cvPoint(cvRound(center.x + magnitude * x_delta), cvRound(center.y - magnitude * y_delta)),
               color, 3, CV_AA, 0);

        if (i >= 0) {
            avg_x_delta += x_delta;
            avg_y_delta += y_delta;
        }
    }
    if (n_components > 0) {
        avg_x_delta /= (double) n_components;
        avg_y_delta /= (double) n_components;
        if ((fabs(avg_x_delta) > 0.5) || (fabs(avg_y_delta) > 0.5)) {
            GstStructure *st  = gst_structure_new("motiontemplate-movement",
                                                  "x-delta", G_TYPE_DOUBLE, (double) avg_x_delta,
                                                  "y-delta", G_TYPE_DOUBLE, (double) avg_y_delta,
                                                  NULL);
            GstMessage *msg = gst_message_new_element(GST_OBJECT(filter), st);
            gst_element_post_message(GST_ELEMENT(gst_element_get_parent(GST_ELEMENT(filter))), msg);
        }
    }

    gst_buffer_set_data(buf, (guint8*) filter->motion->imageData, (guint) filter->motion->imageSize);

    cvReleaseImage(&image);

    return gst_pad_push(filter->srcpad, buf);
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
gboolean
gst_motion_template_plugin_init(GstPlugin * plugin)
{
    /* debug category for fltering log messages */
    GST_DEBUG_CATEGORY_INIT(gst_motion_template_debug, "motiontemplate", 0, "Track horizontal movement using the OpenCV motion template example");
    return gst_element_register(plugin, "motiontemplate", GST_RANK_NONE, GST_TYPE_MOTION_TEMPLATE);
}
