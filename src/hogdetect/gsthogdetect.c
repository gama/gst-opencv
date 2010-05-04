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
 * SECTION:element-hogdetect
 *
 * This element uses the HOG (Histogram of Oriented Gradients) to detect regions
 * on each buffer where a trained pattern is found. The detected regions are
 * forwarded using downstream events.
 *
 * <refsect2>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace ! hogdetect ! ffmpegcolorspace ! autoimagesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsthogdetect.h"
#include "util.h"

#include <sys/time.h>
#include <gst/gst.h>
#include <cvaux.h>
#include <highgui.h>

GST_DEBUG_CATEGORY_STATIC (gst_hog_detect_debug);
#define GST_CAT_DEFAULT gst_hog_detect_debug

#define DEFAULT_SAVE_PREFIX                   "/tmp/hog"
#define DEFAULT_SCALE                         1.125f
#define DEFAULT_HIT_THRESHOLD                 0.0f
#define DEFAULT_GROUP_THRESHOLD               2
#define DEFAULT_CONFIDENCE_DENSITY_THRESHOLD -1.0f

enum
{
    PROP_0,
    PROP_VERBOSE,
    PROP_DISPLAY,
    PROP_CONFIDENCE_DENSITY,
    PROP_SCALE,
    PROP_HIT_THRESHOLD,
    PROP_GROUP_THRESHOLD,
    PROP_CONFIDENCE_DENSITY_THRESHOLD,
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

GST_BOILERPLATE (GstHogDetect, gst_hog_detect, GstElement, GST_TYPE_ELEMENT);

static void          gst_hog_detect_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_hog_detect_get_property (GObject * object, guint prop_id, GValue *value, GParamSpec *pspec);
static void          detect_hogs                 (GstHogDetect *filter, GstBuffer *buf);
static gboolean      gst_hog_detect_set_caps     (GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_hog_detect_chain        (GstPad * pad, GstBuffer * buf);
static gchar*        build_timestamp             ();

// clean up
static void
gst_hog_detect_finalize(GObject *obj)
{
    GstHogDetect *filter = GST_HOG_DETECT(obj);

    if (filter->image)       cvReleaseImage(&filter->image);
    if (filter->save_prefix) g_free(filter->save_prefix);
    if (filter->hog)         cvHogRelease(filter->hog);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

// gobject vmethod implementations
static void
gst_hog_detect_base_init (gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "hogdetect",
                                         "Filter/Video",
                                         "Performs hog detection on videos and images, providing detected positions via downstream events",
                                         "Gustavo Machado C. Gama <gama@vettalabs.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the hogdetect's class
static void
gst_hog_detect_class_init(GstHogDetectClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class  = g_type_class_peek_parent(klass);

    gobject_class->finalize     = GST_DEBUG_FUNCPTR(gst_hog_detect_finalize);
    gobject_class->set_property = gst_hog_detect_set_property;
    gobject_class->get_property = gst_hog_detect_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose", "Sets whether the movement direction should be printed to the standard output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display", "Highligh the detected regions in the video output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CONFIDENCE_DENSITY,
                                    g_param_spec_boolean("confidence-density", "Confidence Density", "Send the \"confidence density\" matrix downstream as custom events",
                                                         TRUE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SCALE,
                                    g_param_spec_float("scale", "Scale",
                                                       "Scale down each pyramid level by this amount (i.e. a scale of \"2\" will half the image size)",
                                                       1.0f, FLT_MAX, DEFAULT_SCALE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_HIT_THRESHOLD,
                                    g_param_spec_float("hit-threshold", "Hit Threshold",
                                                       "Windows whose matching \"score\" is above this value will be marked as a detection",
                                                       -FLT_MAX, FLT_MAX, DEFAULT_HIT_THRESHOLD, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_GROUP_THRESHOLD,
                                    g_param_spec_int("group-threshold", "Group Threshold",
                                                     "Threshold used to resolve duplicate detections by grouping overlapping regions",
                                                     INT_MIN, INT_MAX, DEFAULT_GROUP_THRESHOLD, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CONFIDENCE_DENSITY_THRESHOLD,
                                    g_param_spec_float("confidence-density-threshold", "Confidence Density Threshold",
                                                       "Disconsider all windows with score below this threshold when building the aggregated confidence density matrix",
                                                       -FLT_MAX, FLT_MAX, DEFAULT_CONFIDENCE_DENSITY_THRESHOLD, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SAVE_IMAGES,
                                    g_param_spec_boolean("save-images", "Save detected objects", "Save detected objects as JPEG images",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SAVE_PREFIX,
                                    g_param_spec_string("save-prefix", "Filename prefix of the saved images", "Use the given prefix to build the name of the files where the detected hogs will be saved. The full file path will be '<save-prefix>_<hog#>_<timestamp>.jpg'",
                                                        DEFAULT_SAVE_PREFIX, G_PARAM_READWRITE));
}

// initialize the new element
// instantiate pads and add them to element
// set pad calback functions
// initialize instance structure
static void
gst_hog_detect_init(GstHogDetect *filter, GstHogDetectClass *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_hog_detect_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_hog_detect_chain));

    filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    filter->image                        = NULL;
    filter->verbose                      = FALSE;
    filter->display                      = FALSE;
    filter->confidence_density           = TRUE;
    filter->scale                        = DEFAULT_SCALE;
    filter->hit_threshold                = DEFAULT_HIT_THRESHOLD;
    filter->group_threshold              = DEFAULT_GROUP_THRESHOLD;
    filter->confidence_density_threshold = DEFAULT_CONFIDENCE_DENSITY_THRESHOLD;
    filter->save_images                  = FALSE;
    filter->save_prefix                  = g_strdup(DEFAULT_SAVE_PREFIX);

    // TODO: turn the hardcoded HOG parameters below into gobject properties;
    // I'm not sure that's very useful until we convert the feature vector
    // (the HOG 'detector', which is set to the default 'people detector',
    // right below) into a property as well;
    //
    // FYI, the parameters below are respectively:
    //      window size
    //      block size
    //      block stride
    //      cell size
    //      number of histogram bins
    //      deriv. aperture,
    //      window sigma
    //      histogram normalization type (0 == L2)
    //      L2 histogram threshold,
    //      enable gamma correction,
    //
    // Of these, I could not find any use for the 'derivAperture', 'winSigma' and
    // 'histogramNormType' parameters
    filter->hog = cvHogCreate(cvSize(64, 128), cvSize(16, 16), cvSize(8, 8),
                              cvSize(8, 8), 9, 1, -1, 0, 0.2, TRUE);
    cvHogSetPeopleDetector(filter->hog);
}

static void
gst_hog_detect_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstHogDetect *filter = GST_HOG_DETECT(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY:
            filter->display = g_value_get_boolean(value);
            break;
        case PROP_CONFIDENCE_DENSITY:
            filter->confidence_density = g_value_get_boolean(value);
            break;
        case PROP_SCALE:
            filter->scale = g_value_get_float(value);
            break;
        case PROP_HIT_THRESHOLD:
            filter->hit_threshold = g_value_get_float(value);
            break;
        case PROP_GROUP_THRESHOLD:
            filter->group_threshold = g_value_get_int(value);
            break;
        case PROP_CONFIDENCE_DENSITY_THRESHOLD:
            filter->confidence_density_threshold = g_value_get_float(value);
            break;
        case PROP_SAVE_IMAGES:
            filter->save_images = g_value_get_boolean(value);
            break;
        case PROP_SAVE_PREFIX:
            if (filter->save_prefix) g_free(filter->save_prefix);
            filter->save_prefix = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gst_hog_detect_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstHogDetect *filter = GST_HOG_DETECT(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_DISPLAY:
            g_value_set_boolean(value, filter->display);
            break;
        case PROP_CONFIDENCE_DENSITY:
            g_value_set_boolean(value, filter->confidence_density);
            break;
        case PROP_SCALE:
            g_value_set_float(value, filter->scale);
            break;
        case PROP_HIT_THRESHOLD:
            g_value_set_float(value, filter->hit_threshold);
            break;
        case PROP_GROUP_THRESHOLD:
            g_value_set_int(value, filter->group_threshold);
            break;
        case PROP_CONFIDENCE_DENSITY_THRESHOLD:
            g_value_set_float(value, filter->confidence_density_threshold);
            break;
        case PROP_SAVE_IMAGES:
            g_value_set_boolean(value, filter->save_images);
            break;
        case PROP_SAVE_PREFIX:
            g_value_set_string(value, filter->save_prefix);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

// GstElement vmethod implementations

// this function handles the link with other elements
static gboolean
gst_hog_detect_set_caps (GstPad *pad, GstCaps *caps)
{
    GstHogDetect *filter;
    GstPad        *other_pad;
    GstStructure  *structure;
    gint           width, height, depth;

    filter = GST_HOG_DETECT (gst_pad_get_parent (pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width",  &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth",  &depth);

    filter->image = cvCreateImage(cvSize(width, height), depth/3, 3);

    other_pad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);
    return gst_pad_set_caps(other_pad, caps);
}

// chain function; this function does the actual processing
static GstFlowReturn
gst_hog_detect_chain(GstPad *pad, GstBuffer *buf)
{
    GstHogDetect *filter;
    CvMat         confidence_density;
    CvSeq        *found, *found_filtered;
    gint          i, j;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_HOG_DETECT(GST_OBJECT_PARENT(pad));
    filter->image->imageData = (char*) GST_BUFFER_DATA(buf);

    cvHogDetectMultiScale(filter->hog, filter->image, &found, &confidence_density,
                          filter->hit_threshold, cvSize(0, 0), cvSize(0, 0),
                          filter->scale, filter->group_threshold,
                          // we eliminate the overhead of computing the confidence density
                          // matrix when it is disabled by using a very low threshold
                          filter->confidence_density ? filter->confidence_density_threshold : FLT_MIN);

    // filter found rectangles to remove some of the duplicates
    found_filtered = cvCreateSeq(CV_SEQ_ELTYPE_GENERIC, sizeof(CvSeq), sizeof(CvRect), cvCreateMemStorage(0));

    // try to remove yet another set of duplicate detections (it seems the groupRectangles
    // used inside the detection function is not enough); this was taken from the
    // 'peopledetector' demo shipped by default with OpenCV
    for (i = 0; i < found->total; ++i) {
        CvRect *ri = (CvRect*) cvGetSeqElem(found, i);
        for (j = 0; j < found->total; j++) {
            CvRect *rj, r_intersect;

            rj          = (CvRect*) cvGetSeqElem(found, j);
            r_intersect = rect_intersection(ri, rj);
            if (j != i && (rect_equal(&r_intersect, ri)))
                break;
        }
        if (j == found->total)
            cvSeqPush(found_filtered, ri);
    }

    for (i = 0; i < found_filtered->total; ++i) {
        CvRect       *r;
        GstEvent     *event;
        GstStructure *structure;

        r = (CvRect*) cvGetSeqElem(found_filtered, i);

        // the HOG detector returns slightly larger rectangles than the real objects.
        // so we slightly shrink the rectangles to get a nicer output (also taken
        // from the 'peopledetector' demo app)
        r->x     += cvRound(r->width  * 0.10);
        r->width  = cvRound(r->width  * 0.80);
        r->y     += cvRound(r->height * 0.07);
        r->height = cvRound(r->height * 0.80);

        structure = gst_structure_new("hog-detect-roi",
                                      "x",      G_TYPE_UINT, r->x,
                                      "y",      G_TYPE_UINT, r->y,
                                      "width",  G_TYPE_UINT, r->width,
                                      "height", G_TYPE_UINT, r->height,
                                      "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP(buf),
                                      NULL);

        event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
        gst_pad_push_event(filter->srcpad, event);

        if (filter->verbose)
            GST_INFO_OBJECT(filter, "[hog] x: %d, y: %d, width: %d, height: %d",
                            r->x, r->y, r->width, r->height);

        if (filter->save_images) {
            IplImage *hog_image;
            gchar    *filename, *timestamp;

            hog_image = cvCreateImage(cvSize(r->width, r->height), IPL_DEPTH_8U, 3);
            cvSetImageROI(filter->image, cvRect(r->x, r->y, r->width, r->height));
            cvCopy(filter->image, hog_image, NULL);
            cvResetImageROI(filter->image);

            // the opencv load/saving functions only work on the BGR colorspace
            cvCvtColor(hog_image, hog_image, CV_RGB2BGR);

            timestamp = build_timestamp();
            if (found_filtered->total > 1)
                filename = g_strdup_printf("%s_%u_%s.jpg", filter->save_prefix, i, timestamp);
            else
                filename = g_strdup_printf("%s_%s.jpg", filter->save_prefix, timestamp);

            if (cvSaveImage(filename, hog_image, 0) == FALSE)
                GST_ERROR("unable to save detected hog image to file '%s'", filename);
            else if (filter->verbose)
                GST_INFO("[hog][saved] filename: \"%s\"", filename);

            g_free(timestamp);
            g_free(filename);
            cvReleaseImage(&hog_image);
        }

        if (filter->display) {
            cvRectangle(filter->image,
                        cvPoint(r->x, r->y),
                        cvPoint(r->x + r->width, r->y + r->height),
                        CV_RGB(255, 0, 0), 1, 8, 0);
        }
    }

    // release the 'cvSeq's and associated mem storage
    cvReleaseMemStorage(&found->storage);
    cvReleaseMemStorage(&found_filtered->storage);

    if (filter->confidence_density) { // emit an event with the confidence density matrix
        GstEvent     *event;
        GstStructure *structure;

        structure = gst_structure_new("hog-confidence-density",
                                      "matrix",    G_TYPE_POINTER, &confidence_density,
                                      "timestamp", G_TYPE_UINT64,  GST_BUFFER_TIMESTAMP(buf),
                                      NULL);

        event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
        gst_pad_push_event(filter->srcpad, event);

        if (filter->display) {
            // highlight windows with high confidence density on the output frame
            CvMat *uc1_norm, *uc3_norm;

            uc1_norm = cvCreateMat(filter->image->height, filter->image->width, CV_8UC1);
            uc3_norm = cvCreateMat(filter->image->height, filter->image->width, CV_8UC3);

            // convert density scale from [confidence_density_threshold, 1+] to [0, 255]
            cvAddS(&confidence_density, cvScalarAll(-filter->confidence_density_threshold),
                   &confidence_density, NULL);
            cvConvertScale(&confidence_density, &confidence_density,
                           (1 << filter->image->depth) - 1, 0);

            // convert density matrix to integers
            cvConvert(&confidence_density, uc1_norm);

            // copy single channel to the 3 RGB channels so we may display the output
            // on the image frame
            cvCvtColor(uc1_norm, uc3_norm, CV_GRAY2RGB);

            // then, "brighten" the high density areas by adding the matrixes
            cvAdd(filter->image, uc3_norm, filter->image, NULL);

            // and finally, release the temporary matrixes
            cvReleaseMat(&uc1_norm);
            cvReleaseMat(&uc3_norm);
        }
    }

    gst_buffer_set_data(buf, (guchar*) filter->image->imageData, filter->image->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}

static gchar*
build_timestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return g_strdup_printf("%ld%ld", tv.tv_sec, tv.tv_usec / 1000);
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features
gboolean
gst_hog_detect_plugin_init (GstPlugin * plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_hog_detect_debug, "hogdetect", 0,
                            "Performs hog detection on videos and images, providing detected positions via downstream events");

    return gst_element_register(plugin, "hogdetect", GST_RANK_NONE, GST_TYPE_HOG_DETECT);
}
