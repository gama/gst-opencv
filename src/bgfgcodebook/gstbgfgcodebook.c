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

#include <highgui.h>

/**
 * SECTION:element-bgfg_codebook
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace ! bgfg_codebook ! xvimagesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstbgfgcodebook.h"

#define DEFAULT_NUM_FRAMES_LEARN_BG   10
#define DEFAULT_CODEBOOK_MODEL_MIN    10
#define DEFAULT_CODEBOOK_MODEL_MAX    20
#define DEFAULT_CODEBOOK_MODEL_BOUNDS 20
#define DEFAULT_PERIMETER_SCALE        4.0f

enum {
    PROP_0,
    PROP_VERBOSE,
    PROP_DISPLAY,
    PROP_MASK,
    PROP_ROI,
    PROP_NUM_FRAMES_LEARN_BG,
    PROP_MODEL_MIN,
    PROP_MODEL_MAX,
    PROP_MODEL_BOUNDS,
    PROP_CONVEX_HULL,
    PROP_PERIMETER_SCALE
};

static const CvRect DEFAULT_ROI = {0, 0, 0, 0};

GST_DEBUG_CATEGORY_STATIC (gst_bgfg_codebook_debug);
#define GST_CAT_DEFAULT gst_bgfg_codebook_debug

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

GST_BOILERPLATE (GstBgFgCodebook, gst_bgfg_codebook, GstElement, GST_TYPE_ELEMENT);

static void          gst_bgfg_codebook_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void          gst_bgfg_codebook_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean      gst_bgfg_codebook_set_caps     (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_bgfg_codebook_chain        (GstPad * pad, GstBuffer * buf);


static void
set_model_array(guchar *array, guint value)
{
    array[0] = array[1] = array[2] = value;
}

// clean up
static void
gst_bgfg_codebook_finalize(GObject *obj)
{
    GstBgFgCodebook *filter = GST_BGFG_CODEBOOK (obj);
    if (filter->image) cvReleaseImage(&filter->image);
    if (filter->mask)  cvReleaseImage(&filter->mask);
    if (filter->model) cvReleaseBGCodeBookModel(&filter->model);


    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

// gobject vmethod implementations
static void
gst_bgfg_codebook_base_init(gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "bgfgcodebook",
                                         "Filter/Effect/Video",
                                         "Performs background detection on videos using the Codebook algorithm",
                                         "Gustavo Gama <gama@vettalabs.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the bgfg_codebook's class
static void
gst_bgfg_codebook_class_init(GstBgFgCodebookClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class  = g_type_class_peek_parent(klass);

    gobject_class->finalize     = GST_DEBUG_FUNCPTR(gst_bgfg_codebook_finalize);
    gobject_class->set_property = gst_bgfg_codebook_set_property;
    gobject_class->get_property = gst_bgfg_codebook_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose", "Sets whether the movement direction should be printed to the standard output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display", "Display the output of the background/foreground detection by shading the background mask (if the parameter 'mask' is set) and/or drawing rectangles delimiting the ROIs (if the parameter 'roi' is set)",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MASK,
                                    g_param_spec_boolean("mask", "Mask", "Send 'fg-mask' events downstream",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_ROI,
                                    g_param_spec_boolean("roi", "ROI - Region of Interest", "Send 'fg-roi' events downstream",
                                                         TRUE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NUM_FRAMES_LEARN_BG,
                                    g_param_spec_uint("build-bg-frames", "Background building frames", "Number of frames used to 'build'/'learn' the background image.",
                                                      0, 1024, DEFAULT_NUM_FRAMES_LEARN_BG, G_PARAM_READWRITE)); 

    g_object_class_install_property(gobject_class, PROP_MODEL_MIN,
                                    g_param_spec_uint("model-min", "Codebook model lower bound", "Minimum value",
                                                      0, 255, DEFAULT_CODEBOOK_MODEL_MIN, G_PARAM_READWRITE)); 

    g_object_class_install_property(gobject_class, PROP_MODEL_MAX,
                                    g_param_spec_uint("model-max", "Codebook model upper bound", "Maximum value",
                                                      0, 255, DEFAULT_CODEBOOK_MODEL_MAX, G_PARAM_READWRITE)); 

    g_object_class_install_property(gobject_class, PROP_MODEL_BOUNDS,
                                    g_param_spec_uint("model-bounds", "Codebook model bounds", "Bounds value",
                                                      0, 255, DEFAULT_CODEBOOK_MODEL_BOUNDS, G_PARAM_READWRITE)); 

    g_object_class_install_property(gobject_class, PROP_CONVEX_HULL,
                                    g_param_spec_boolean("convex-hull", "Convex hull segmentation", "Use a 'convex hull' to find the ROIs",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_PERIMETER_SCALE,
                                    g_param_spec_float("permiter-scale", "Perimeter scale", "Perimeter scale used to find the ROI segments",
                                                       0.0f, 128.0f, DEFAULT_PERIMETER_SCALE, G_PARAM_READWRITE)); 
}

//initialize the new element
//instantiate pads and add them to element
//set pad calback functions
//initialize instance structure
static void
gst_bgfg_codebook_init(GstBgFgCodebook *filter, GstBgFgCodebookClass *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR (gst_bgfg_codebook_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR (gst_bgfg_codebook_chain));

    filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT (filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT (filter), filter->srcpad);

    // set defaults
    filter->verbose           = FALSE;
    filter->display           = FALSE;
    filter->send_mask_events  = FALSE;
    filter->send_roi_events   = TRUE;
    filter->convex_hull       = FALSE;
    filter->perimeter_scale   = DEFAULT_PERIMETER_SCALE;

    filter->n_frames          = 0;
    filter->n_frames_learn_bg = DEFAULT_NUM_FRAMES_LEARN_BG;

    // create and setup model parameters
    filter->model = cvCreateBGCodeBookModel();
    set_model_array(filter->model->modMin, DEFAULT_CODEBOOK_MODEL_MIN);
    set_model_array(filter->model->modMax, DEFAULT_CODEBOOK_MODEL_MAX);
    set_model_array(filter->model->cbBounds, DEFAULT_CODEBOOK_MODEL_BOUNDS);
}

static void
gst_bgfg_codebook_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstBgFgCodebook *filter = GST_BGFG_CODEBOOK(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY:
            filter->display = g_value_get_boolean(value);
            break;
        case PROP_MASK:
            filter->send_mask_events = g_value_get_boolean(value);
            break;
        case PROP_ROI:
            filter->send_roi_events = g_value_get_boolean(value);
            break;
        case PROP_NUM_FRAMES_LEARN_BG:
            filter->n_frames_learn_bg = g_value_get_uint(value);
            break;
        case PROP_MODEL_MIN:
            set_model_array(filter->model->modMin, g_value_get_uint(value));
            break;
        case PROP_MODEL_MAX:
            set_model_array(filter->model->modMax, g_value_get_uint(value));
            break;
        case PROP_MODEL_BOUNDS:
            set_model_array(filter->model->cbBounds, g_value_get_uint(value));
            break;
        case PROP_CONVEX_HULL:
            filter->convex_hull = g_value_get_boolean(value);
            break;
        case PROP_PERIMETER_SCALE:
            filter->perimeter_scale = g_value_get_float(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_bgfg_codebook_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstBgFgCodebook *filter = GST_BGFG_CODEBOOK(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_DISPLAY:
            g_value_set_boolean(value, filter->display);
            break;
        case PROP_MASK:
            g_value_set_boolean(value, filter->send_mask_events);
            break;
        case PROP_ROI:
            g_value_set_boolean(value, filter->send_roi_events);
            break;
        case PROP_NUM_FRAMES_LEARN_BG:
            g_value_set_uint(value, filter->n_frames_learn_bg);
            break;
        case PROP_MODEL_MIN:
            g_value_set_uint(value, filter->model->modMin[0]);
            break;
        case PROP_MODEL_MAX:
            g_value_set_uint(value, filter->model->modMax[0]);
            break;
        case PROP_MODEL_BOUNDS:
            g_value_set_uint(value, filter->model->cbBounds[0]);
            break;
        case PROP_CONVEX_HULL:
            g_value_set_boolean(value, filter->convex_hull);
            break;
        case PROP_PERIMETER_SCALE:
            g_value_set_float(value, filter->perimeter_scale);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

// GstElement vmethod implementations
// this function handles the link with other elements
static gboolean
gst_bgfg_codebook_set_caps(GstPad *pad, GstCaps *caps)
{
    GstBgFgCodebook *filter;
    GstPad          *otherpad;
    GstStructure    *structure;
    gint             width, height, depth;

    filter = GST_BGFG_CODEBOOK(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth", &depth);

    // initialize mask
    filter->image  = cvCreateImage(cvSize(width, height), depth/3, 3);
    filter->mask   = cvCreateImage(cvSize(width, height), depth/3, 1);
    cvSet(filter->mask, cvScalar(255, 255, 255, 0), 0); // draw black mask

    otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);
    return gst_pad_set_caps(otherpad, caps);
}

// chain function - this function does the actual processing
static GstFlowReturn
gst_bgfg_codebook_chain(GstPad *pad, GstBuffer *buf)
{
    GstBgFgCodebook *filter;
    IplImage        *yuv_image;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_BGFG_CODEBOOK(GST_OBJECT_PARENT(pad));

    filter->image->imageData = (gchar*) GST_BUFFER_DATA(buf);

    // clear stale models right after we stop learning the background
    if (filter->n_frames == (filter->n_frames_learn_bg + 1))
        cvBGCodeBookClearStale(filter->model, filter->model->t / 2, DEFAULT_ROI, 0);

    yuv_image = cvCloneImage(filter->image);
    cvCvtColor(filter->image, yuv_image, CV_BGR2YCrCb); //YUV For codebook method

    if (filter->n_frames <= filter->n_frames_learn_bg) {
        cvBGCodeBookUpdate(filter->model, yuv_image, DEFAULT_ROI, 0);
        filter->n_frames++;
        if (filter->verbose)
            GST_INFO("[build background] %d frames", filter->n_frames);
    } else {
        GstStructure *structure;
        GstEvent     *event;

        cvBGCodeBookDiff(filter->model, yuv_image, filter->mask, DEFAULT_ROI);
        cvDilate(filter->mask, filter->mask, NULL, 1);
        cvErode(filter->mask, filter->mask, NULL, 1);

        // send mask event, if requested
        if (filter->send_mask_events) {
            GArray       *data_array;

            // prepare and send custom event with the mask surface
            data_array = g_array_sized_new(FALSE, FALSE, sizeof(filter->mask->imageData[0]), filter->mask->imageSize);
            g_array_append_vals(data_array, filter->mask->imageData, filter->mask->imageSize);

            structure = gst_structure_new("mask",
                                          "data",      G_TYPE_POINTER, data_array,
                                          "width",     G_TYPE_UINT, filter->mask->width,
                                          "height",    G_TYPE_UINT, filter->mask->height,
                                          "depth",     G_TYPE_UINT, filter->mask->depth,
                                          "channels",  G_TYPE_UINT, filter->mask->nChannels,
                                          "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP(buf),
                                          NULL);

            event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
            gst_pad_push_event(filter->srcpad, event);
            g_array_unref(data_array);

            if (filter->display) {
                // shade the regions not selected by the codebook algorithm
                cvXorS(filter->mask,  CV_RGB(255, 255, 255), filter->mask,  NULL);
                cvSubS(filter->image, CV_RGB(191, 191, 191), filter->image, filter->mask);
                cvXorS(filter->mask,  CV_RGB(255, 255, 255), filter->mask,  NULL);

            }
        }

        if (filter->send_roi_events) {
            CvSeq        *contour, *contours;
            CvMemStorage *storage;

            storage  = cvCreateMemStorage(0);
            contours = cvSegmentFGMask(filter->mask, filter->convex_hull ? 0 : 1,
                                       filter->perimeter_scale, storage, cvPoint(0, 0));

            for (contour = contours; contour != NULL; contour = contour->h_next) {
                GstEvent     *event;
                GstStructure *structure;
                CvRect        r = cvBoundingRect(contour, 0);

                structure = gst_structure_new("roi",
                                              "x",         G_TYPE_UINT,   r.x,
                                              "y",         G_TYPE_UINT,   r.y,
                                              "width",     G_TYPE_UINT,   r.width,
                                              "height",    G_TYPE_UINT,   r.height,
                                              "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP(buf),
                                              NULL);

                event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
                gst_pad_send_event(filter->sinkpad, event);

                if (filter->verbose)
                    fprintf(stdout, "[roi] x: %d, y: %d, width: %d, height: %d\n", r.x, r.y, r.width, r.height);

                if (filter->display)
                    cvRectangle(filter->image, cvPoint(r.x, r.y), cvPoint(r.x + r.width, r.y + r.height),
                                CV_RGB(0, 0, 255), 1, 0, 0);
            }

            cvReleaseMemStorage(&storage);
        }

        if (filter->display)
            gst_buffer_set_data(buf, (guchar*) filter->image->imageData, filter->image->imageSize);
    }

    cvReleaseImage(&yuv_image);

    return gst_pad_push(filter->srcpad, buf);
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features
gboolean
gst_bgfg_codebook_plugin_init (GstPlugin * plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_bgfg_codebook_debug, "bgfgcodebook", 0,
                            "Performs background detection on videos using the Codebook algorithm");

    return gst_element_register(plugin, "bgfgcodebook", GST_RANK_NONE, GST_TYPE_BGFG_CODEBOOK);
}
