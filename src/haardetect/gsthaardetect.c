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
 * SECTION:element-haardetect
 *
 * FIXME:Describe haardetect here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace ! haardetect ! ffmpegcolorspace ! xvimagesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gsthaardetect.h"

#include <sys/time.h>
#include <gst/gst.h>
#include <cvaux.h>
#include <highgui.h>

GST_DEBUG_CATEGORY_STATIC (gst_haar_detect_debug);
#define GST_CAT_DEFAULT gst_haar_detect_debug

#define DEFAULT_PROFILE       "/usr/share/opencv/haarcascades/haarcascade_frontalhaar_default.xml"
#define DEFAULT_SAVE_PREFIX   "/tmp/haarmetrix"
#define DEFAULT_MIN_NEIGHBORS  5
#define DEFAULT_MIN_SIZE      20

enum
{
    PROP_0,
    PROP_VERBOSE,
    PROP_DISPLAY,
    PROP_ROI_ONLY,
    PROP_PROFILE,
    PROP_MIN_NEIGHBORS,
    PROP_MIN_SIZE,
    PROP_SAVE_IMAGES,
    PROP_SAVE_PREFIX
};

/* the capabilities of the inputs and outputs.
 */
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

GST_BOILERPLATE (GstHaarDetect, gst_haar_detect, GstElement, GST_TYPE_ELEMENT);

static void          gst_haar_detect_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_haar_detect_get_property (GObject * object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean      roi_events_cb                (GstPad *pad, GstEvent *event, gpointer user_data);
static void          detect_haars                 (GstHaarDetect *filter, GstBuffer *buf);
static gboolean      gst_haar_detect_set_caps     (GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_haar_detect_chain        (GstPad * pad, GstBuffer * buf);
static void          gst_haar_detect_load_profile (GstHaarDetect *filter);
static gchar*        build_timestamp              ();


// clean up
static void
gst_haar_detect_finalize(GObject *obj)
{
    GstHaarDetect *filter = GST_HAAR_DETECT(obj);

    if (filter->image)   cvReleaseImage(&filter->image);
    if (filter->gray)    cvReleaseImage(&filter->gray);
    if (filter->storage) cvReleaseMemStorage(&filter->storage);
    if (filter->cascade) cvReleaseHaarClassifierCascade(&filter->cascade);
    if (filter->profile) g_free(filter->profile);

    G_OBJECT_CLASS (parent_class)->finalize(obj);
}

// gobject vmethod implementations
static void
gst_haar_detect_base_init (gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "haardetect",
                                         "Filter/Video",
                                         "Performs haar detection on videos and images, providing detected positions via downstream events",
                                         "Michael Sheldon <mike@mikeasoft.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the haardetect's class
static void
gst_haar_detect_class_init(GstHaarDetectClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class  = g_type_class_peek_parent(klass);

    gobject_class->finalize     = GST_DEBUG_FUNCPTR(gst_haar_detect_finalize);
    gobject_class->set_property = gst_haar_detect_set_property;
    gobject_class->get_property = gst_haar_detect_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose", "Sets whether the movement direction should be printed to the standard output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display", "Highligh the detected haars in the video output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_ROI_ONLY,
                                    g_param_spec_boolean("roi-only", "ROI only", "Only try to detect haars when one or more ROIs have been set",
                                                         TRUE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_PROFILE,
                                    g_param_spec_string("profile", "Profile", "Location of Haar cascade file",
                                                        DEFAULT_PROFILE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MIN_NEIGHBORS,
                                    g_param_spec_uint("min-neighbors", "Haar Minimum Neighbors", "Minimum number of neighbor pixels used to search for haar patterns",
                                                      0, UINT_MAX, DEFAULT_MIN_NEIGHBORS, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MIN_SIZE,
                                    g_param_spec_uint("min-size", "Haar Minimum Size", "Minimum size of a haar",
                                                      0, UINT_MAX, DEFAULT_MIN_SIZE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SAVE_IMAGES,
                                    g_param_spec_boolean("save-images", "Save detected objects", "Save detected objects as JPEG images",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SAVE_PREFIX,
                                    g_param_spec_string("save-prefix", "Filename prefix of the saved images", "Use the given prefix to build the name of the files where the detected haars will be saved. The full file path will be '<save-prefix>_<haar#>_<timestamp>.jpg'",
                                                        DEFAULT_SAVE_PREFIX, G_PARAM_READWRITE));
}

// initialize the new element
// instantiate pads and add them to element
// set pad calback functions
// initialize instance structure
static void
gst_haar_detect_init(GstHaarDetect *filter, GstHaarDetectClass *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_haar_detect_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_haar_detect_chain));

    filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    filter->verbose       = FALSE;
    filter->display       = FALSE;
    filter->roi_only      = FALSE;
    filter->roi_timestamp = 0;
    filter->roi_array     = g_array_sized_new(FALSE, FALSE, sizeof(CvRect), 4);
    filter->profile       = DEFAULT_PROFILE;
    filter->save_images   = FALSE;
    filter->save_prefix   = DEFAULT_SAVE_PREFIX;
    filter->min_neighbors = DEFAULT_MIN_NEIGHBORS;
    filter->min_size      = DEFAULT_MIN_SIZE;

    gst_haar_detect_load_profile(filter);
}

static void
gst_haar_detect_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstHaarDetect *filter = GST_HAAR_DETECT(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY:
            filter->display = g_value_get_boolean(value);
            break;
        case PROP_ROI_ONLY:
            filter->roi_only = g_value_get_boolean(value);
            break;
        case PROP_PROFILE:
            filter->profile = g_value_dup_string(value);
            gst_haar_detect_load_profile (filter);
            break;
        case PROP_MIN_NEIGHBORS:
            filter->min_neighbors = g_value_get_uint(value);
            break;
        case PROP_MIN_SIZE:
            filter->min_size = g_value_get_uint(value);
            break;
        case PROP_SAVE_IMAGES:
            filter->save_images = g_value_get_boolean(value);
            break;
        case PROP_SAVE_PREFIX:
            filter->save_prefix = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gst_haar_detect_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstHaarDetect *filter = GST_HAAR_DETECT(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_DISPLAY:
            g_value_set_boolean(value, filter->display);
            break;
        case PROP_ROI_ONLY:
            g_value_set_boolean(value, filter->roi_only);
            break;
        case PROP_PROFILE:
            g_value_take_string(value, filter->profile);
            break;
        case PROP_MIN_NEIGHBORS:
            g_value_set_uint(value, filter->min_neighbors);
            break;
        case PROP_MIN_SIZE:
            g_value_set_uint(value, filter->min_size);
            break;
        case PROP_SAVE_IMAGES:
            g_value_set_boolean(value, filter->save_images);
            break;
        case PROP_SAVE_PREFIX:
            g_value_take_string(value, filter->save_prefix);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

// GstElement vmethod implementations

// this function handles the link with other elements
static gboolean
gst_haar_detect_set_caps (GstPad *pad, GstCaps *caps)
{
    GstHaarDetect *filter;
    GstPad        *other_pad;
    GstStructure  *structure;
    gint           width, height, depth;

    filter = GST_HAAR_DETECT (gst_pad_get_parent (pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width",  &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth",  &depth);

    filter->image   = cvCreateImage(cvSize(width, height), depth/3, 3);
    filter->gray    = cvCreateImage(cvSize(width, height), depth/3, 1);
    filter->storage = cvCreateMemStorage(0);

    // add roi event probe on the sinkpad
    gst_pad_add_event_probe(filter->sinkpad, (GCallback) roi_events_cb, filter);

    other_pad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);
    return gst_pad_set_caps(other_pad, caps);
}

// chain function; this function does the actual processing
static GstFlowReturn
gst_haar_detect_chain(GstPad *pad, GstBuffer *buf)
{
    GstHaarDetect *filter;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_HAAR_DETECT(GST_OBJECT_PARENT(pad));

    // if no cascade could be loaded, let the pipeline continue as if
    // the haar detect element was not present
    g_return_val_if_fail(filter->cascade != NULL, GST_FLOW_OK);

    filter->image->imageData = (char*) GST_BUFFER_DATA(buf);

    cvCvtColor(filter->image, filter->gray, CV_RGB2GRAY);
    cvClearMemStorage(filter->storage);

    // check roi timestamps and roi array length; these should have been
    // set at the roi_events_cb callback
    if ((filter->roi_timestamp == GST_BUFFER_TIMESTAMP(buf)) &&
        (filter->roi_array != NULL) &&
        (filter->roi_array->len > 0)) {

        guint i;
        for (i = 0; i < filter->roi_array->len; ++i) {
            cvSetImageROI(filter->gray, g_array_index(filter->roi_array, CvRect, i));
            detect_haars(filter, buf);
            cvResetImageROI(filter->gray);
        }
    } else if (filter->roi_only == FALSE)
        detect_haars(filter, buf);

    gst_buffer_set_data(buf, (guchar*) filter->image->imageData, filter->image->imageSize);

    return gst_pad_push(filter->srcpad, buf);
}

static void
detect_haars(GstHaarDetect *filter, GstBuffer *buf)
{
    gint i;
    CvRect roi;
    CvSeq *haars;

    haars = cvHaarDetectObjects(filter->gray, filter->cascade, filter->storage,
                                1.1, filter->min_neighbors, CV_HAAR_DO_CANNY_PRUNING,
                                cvSize(filter->min_size, filter->min_size));

    if ((haars == NULL) || (haars->total <= 0))
        return;

    roi = cvGetImageROI(filter->gray);

    for (i = 0; i < haars->total; ++i) {
        CvRect       *r;
        GstEvent     *event;
        GstStructure *structure;

        r = (CvRect*) cvGetSeqElem(haars, i);
        structure = gst_structure_new("haar-detect-roi",
                                      "x",      G_TYPE_UINT, roi.x + r->x,
                                      "y",      G_TYPE_UINT, roi.y + r->y,
                                      "width",  G_TYPE_UINT, r->width,
                                      "height", G_TYPE_UINT, r->height,
                                      "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP(buf),
                                      NULL);

        event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
        gst_pad_push_event(filter->srcpad, event);

        if (filter->verbose)
            GST_INFO("[haar] x: %d, y: %d, width: %d, height: %d",
                    roi.x + r->x, roi.y + r->y, r->width, r->height);

        if (filter->save_images) {
            IplImage *haar_image;
            gchar    *filename, *timestamp;

            haar_image = cvCreateImage(cvSize(r->width, r->height), IPL_DEPTH_8U, 3);
            cvSetImageROI(filter->image, cvRect(roi.x + r->x, roi.y + r->y, r->width, r->height));
            cvCopy(filter->image, haar_image, NULL);
            cvResetImageROI(filter->image);

            // the opencv load/saving functions only work on the BGR colorspace
            cvCvtColor(haar_image, haar_image, CV_RGB2BGR);

            timestamp = build_timestamp();
            if (haars->total > 1)
                filename = g_strdup_printf("%s_%u_%s.jpg", filter->save_prefix, i, timestamp);
            else
                filename = g_strdup_printf("%s_%s.jpg", filter->save_prefix, timestamp);

            if (cvSaveImage(filename, haar_image, 0) == FALSE)
                GST_ERROR("unable to save detected haar image to file '%s'", filename);
            else if (filter->verbose)
                GST_INFO("[haar][saved] filename: \"%s\"", filename);

            g_free(timestamp);
            g_free(filename);
            cvReleaseImage(&haar_image);
        }

        if (filter->display) {
            cvRectangle(filter->image,
                        cvPoint(r->x + roi.x, r->y + roi.y),
                        cvPoint(r->x + roi.x + r->width, r->y + roi.y + r->height),
                        CV_RGB(255, 0, 0), 1, 8, 0);
        }
    }
}

static void
gst_haar_detect_load_profile(GstHaarDetect *filter)
{
    filter->cascade = (CvHaarClassifierCascade*) cvLoad(filter->profile, 0, 0, 0);
    if (filter->cascade == NULL)
        GST_WARNING("unable to load haar cascade: \"%s\"", filter->profile);
}

static gchar*
build_timestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return g_strdup_printf("%ld%ld", tv.tv_sec, tv.tv_usec / 1000);
}

// callbacks
static
gboolean roi_events_cb(GstPad *pad, GstEvent *event, gpointer user_data)
{
    GstHaarDetect      *filter;
    const GstStructure *structure;

    filter = GST_HAAR_DETECT(user_data);

    // sanity checks
    g_return_val_if_fail(pad    != NULL, FALSE);
    g_return_val_if_fail(event  != NULL, FALSE);
    g_return_val_if_fail(filter != NULL, FALSE);

    structure = gst_event_get_structure(event);
    if ((structure != NULL) && (strcmp(gst_structure_get_name(structure), "bgfg-roi") == 0)) {
        CvRect roi;
        GstClockTime timestamp;

        gst_structure_get((GstStructure*) structure,
                          "x",         G_TYPE_UINT,   &roi.x,
                          "y",         G_TYPE_UINT,   &roi.y,
                          "width",     G_TYPE_UINT,   &roi.width,
                          "height",    G_TYPE_UINT,   &roi.height,
                          "timestamp", G_TYPE_UINT64, &timestamp, NULL);

        if (timestamp > filter->roi_timestamp) {
            filter->roi_timestamp = timestamp;
            g_array_free(filter->roi_array, TRUE);
            filter->roi_array = g_array_sized_new(FALSE, FALSE, sizeof(CvRect), 4);
        }
        g_array_append_val(filter->roi_array, roi);
    }

    return TRUE;
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features
gboolean
gst_haar_detect_plugin_init (GstPlugin * plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_haar_detect_debug, "haardetect", 0,
                            "Performs haar detection on videos and images, providing detected positions via downstream events");

    return gst_element_register(plugin, "haardetect", GST_RANK_NONE, GST_TYPE_HAAR_DETECT);
}
