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
 * SECTION:element-bgfg_acmmm2003
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc ! decodebin ! ffmpegcolorspace ! bgfgacmmm2003 ! xvimagesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstbgfgacmmm2003.h"

#define DEFAULT_PERIMETER_SCALE         4.0f
#define DEFAULT_MIN_AREA              100.0f
#define DEFAULT_NUM_ERODE_ITERATIONS    1
#define DEFAULT_NUM_DILATE_ITERATIONS   3

enum {
    PROP_0,
    PROP_VERBOSE,
    PROP_DISPLAY,
    PROP_MASK,
    PROP_ROI,
    PROP_CONVEX_HULL,
    PROP_PERIMETER_SCALE,
    PROP_MIN_AREA,
    PROP_NUM_ERODE_ITERATIONS,
    PROP_NUM_DILATE_ITERATIONS
};

static const CvRect NULL_RECT = {0, 0, 0, 0};

GST_DEBUG_CATEGORY_STATIC (gst_bgfg_acmmm2003_debug);
#define GST_CAT_DEFAULT gst_bgfg_acmmm2003_debug

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

GST_BOILERPLATE (GstBgFgACMMM2003, gst_bgfg_acmmm2003, GstElement, GST_TYPE_ELEMENT);

static void          gst_bgfg_acmmm2003_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void          gst_bgfg_acmmm2003_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean      gst_bgfg_acmmm2003_set_caps     (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_bgfg_acmmm2003_chain        (GstPad * pad, GstBuffer * buf);
static gboolean      rect_overlap                   (const CvRect r1, const CvRect r2);
static CvRect        rect_collapse                  (const CvRect r1, const CvRect r2);

static void
set_model_array(guchar *array, guint value)
{
    array[0] = array[1] = array[2] = value;
}

// clean up
static void
gst_bgfg_acmmm2003_finalize(GObject *obj)
{
    GstBgFgACMMM2003 *filter = GST_BGFG_ACMMM2003 (obj);
    if (filter->image) cvReleaseImage(&filter->image);
    if (filter->model) cvReleaseBGStatModel(&filter->model);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

// gobject vmethod implementations
static void
gst_bgfg_acmmm2003_base_init(gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "bgfgacmmm2003",
                                         "Filter/Effect/Video",
                                         "Performs background detection on videos using the paper 'Foreground Object Detection from Videos Containing Complex Background', by Li, Huan, Gu, Tian, which was published on the ACM Multimedia 2003 Conference",
                                         "Gustavo Gama <gama@vettalabs.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the bgfg_acmmm2003's class
static void
gst_bgfg_acmmm2003_class_init(GstBgFgACMMM2003Class *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class  = g_type_class_peek_parent(klass);

    gobject_class->finalize     = GST_DEBUG_FUNCPTR(gst_bgfg_acmmm2003_finalize);
    gobject_class->set_property = gst_bgfg_acmmm2003_set_property;
    gobject_class->get_property = gst_bgfg_acmmm2003_get_property;

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

    g_object_class_install_property(gobject_class, PROP_CONVEX_HULL,
                                    g_param_spec_boolean("convex-hull", "Convex hull segmentation", "Use a 'convex hull' to find the ROIs",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_PERIMETER_SCALE,
                                    g_param_spec_float("permiter-scale", "Perimeter scale", "Perimeter scale used to find the ROI segments",
                                                       0.0f, 128.0f, DEFAULT_PERIMETER_SCALE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MIN_AREA,
                                    g_param_spec_float("min-area", "Minimal Area", "Foreground regions with an area below this threshold will be discarded",
                                                       0.0f, FLT_MAX, DEFAULT_MIN_AREA, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NUM_ERODE_ITERATIONS,
                                    g_param_spec_uint("num-erode-iterations", "Number of erode iterations", "Number of times that an 'erode' filter should be applied to the foreground mask",
                                                       0, INT_MAX, DEFAULT_NUM_ERODE_ITERATIONS, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NUM_DILATE_ITERATIONS,
                                    g_param_spec_uint("num-dilate-iterations", "Number of dilate iterations", "Number of times that an 'dilate' filter should be applied to the foreground mask. Note that the 'dilate' filter is applied *after* the 'erode' filter",
                                                       0, INT_MAX, DEFAULT_NUM_DILATE_ITERATIONS, G_PARAM_READWRITE));
}

//initialize the new element
//instantiate pads and add them to element
//set pad calback functions
//initialize instance structure
static void
gst_bgfg_acmmm2003_init(GstBgFgACMMM2003 *filter, GstBgFgACMMM2003Class *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR (gst_bgfg_acmmm2003_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR (gst_bgfg_acmmm2003_chain));

    filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT (filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT (filter), filter->srcpad);

    // set defaults
    filter->model               = NULL;
    filter->verbose             = FALSE;
    filter->display             = FALSE;
    filter->send_mask_events    = FALSE;
    filter->send_roi_events     = TRUE;
    filter->convex_hull         = FALSE;
    filter->perimeter_scale     = DEFAULT_PERIMETER_SCALE;
    filter->n_erode_iterations  = DEFAULT_NUM_ERODE_ITERATIONS;
    filter->n_dilate_iterations = DEFAULT_NUM_DILATE_ITERATIONS;
}

static void
gst_bgfg_acmmm2003_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstBgFgACMMM2003 *filter = GST_BGFG_ACMMM2003(object);

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
        case PROP_CONVEX_HULL:
            filter->convex_hull = g_value_get_boolean(value);
            break;
        case PROP_PERIMETER_SCALE:
            filter->perimeter_scale = g_value_get_float(value);
            break;
        case PROP_MIN_AREA:
            filter->min_area = g_value_get_float(value);
            if (filter->model != NULL)
                ((CvFGDStatModel*)filter->model)->params.minArea = filter->min_area;
            break;
        case PROP_NUM_ERODE_ITERATIONS:
            filter->n_erode_iterations = g_value_get_uint(value);
            if (filter->model != NULL)
                ((CvFGDStatModel*)filter->model)->params.erode_iterations = filter->n_erode_iterations;
            break;
        case PROP_NUM_DILATE_ITERATIONS:
            filter->n_dilate_iterations = g_value_get_uint(value);
            if (filter->model != NULL)
                ((CvFGDStatModel*)filter->model)->params.dilate_iterations = filter->n_dilate_iterations;
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_bgfg_acmmm2003_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstBgFgACMMM2003 *filter = GST_BGFG_ACMMM2003(object);

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
        case PROP_CONVEX_HULL:
            g_value_set_boolean(value, filter->convex_hull);
            break;
        case PROP_PERIMETER_SCALE:
            g_value_set_float(value, filter->perimeter_scale);
            break;
        case PROP_MIN_AREA:
            g_value_set_float(value, filter->min_area);
            break;
        case PROP_NUM_ERODE_ITERATIONS:
            g_value_set_uint(value, filter->n_erode_iterations);
            break;
        case PROP_NUM_DILATE_ITERATIONS:
            g_value_set_uint(value, filter->n_dilate_iterations);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

// GstElement vmethod implementations
// this function handles the link with other elements
static gboolean
gst_bgfg_acmmm2003_set_caps(GstPad *pad, GstCaps *caps)
{
    GstBgFgACMMM2003 *filter;
    GstPad          *otherpad;
    GstStructure    *structure;
    gint             width, height, depth;

    filter = GST_BGFG_ACMMM2003(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth", &depth);

    // set default model parameters

    // initialize mask
    filter->image = cvCreateImage(cvSize(width, height), depth/3, 3);

    otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);
    return gst_pad_set_caps(otherpad, caps);
}

// chain function - this function does the actual processing
static GstFlowReturn
gst_bgfg_acmmm2003_chain(GstPad *pad, GstBuffer *buf)
{
    GstBgFgACMMM2003 *filter;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_BGFG_ACMMM2003(GST_OBJECT_PARENT(pad));

    filter->image->imageData = (gchar*) GST_BUFFER_DATA(buf);

    // the bg model must be initialized with a valid image; thus we delay its
    // creation until the chain function
    if (filter->model == NULL) {
        filter->model = cvCreateFGDStatModel(filter->image, NULL);

        ((CvFGDStatModel*)filter->model)->params.minArea           = filter->min_area;
        ((CvFGDStatModel*)filter->model)->params.erode_iterations  = filter->n_erode_iterations;
        ((CvFGDStatModel*)filter->model)->params.dilate_iterations = filter->n_dilate_iterations;

        return gst_pad_push(filter->srcpad, buf);
    }

    cvUpdateBGStatModel(filter->image, filter->model, -1);

    // send mask event, if requested
    if (filter->send_mask_events) {
        GstStructure *structure;
        GstEvent     *event;
        GArray       *data_array;
        IplImage     *mask;

        // prepare and send custom event with the mask surface
        mask = filter->model->foreground;
        data_array = g_array_sized_new(FALSE, FALSE, sizeof(mask->imageData[0]), mask->imageSize);
        g_array_append_vals(data_array, mask->imageData, mask->imageSize);

        structure = gst_structure_new("bgfg-mask",
                                      "data",      G_TYPE_POINTER, data_array,
                                      "width",     G_TYPE_UINT,    mask->width,
                                      "height",    G_TYPE_UINT,    mask->height,
                                      "depth",     G_TYPE_UINT,    mask->depth,
                                      "channels",  G_TYPE_UINT,    mask->nChannels,
                                      "timestamp", G_TYPE_UINT64,  GST_BUFFER_TIMESTAMP(buf),
                                      NULL);

        event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
        gst_pad_push_event(filter->srcpad, event);
        g_array_unref(data_array);

        if (filter->display) {
            // shade the regions not selected by the acmmm2003 algorithm
            cvXorS(mask,          CV_RGB(255, 255, 255), mask,          NULL);
            cvSubS(filter->image, CV_RGB(191, 191, 191), filter->image, mask);
            cvXorS(mask,          CV_RGB(255, 255, 255), mask,          NULL);
        }
    }

    if (filter->send_roi_events) {
        CvSeq        *contour;
        CvRect       *bounding_rects;
        guint         i, j, n_rects;

        // count # of contours, allocate array to store the bounding rectangles
        for (contour = filter->model->foreground_regions, n_rects = 0;
             contour != NULL;
             contour = contour->h_next, ++n_rects);

        bounding_rects = g_new(CvRect, n_rects);

        for (contour = filter->model->foreground_regions, i = 0; contour != NULL; contour = contour->h_next, ++i)
            bounding_rects[i] = cvBoundingRect(contour, 0);

        for (i = 0; i < n_rects; ++i) {
            // skip collapsed rectangles
            if ((bounding_rects[i].width == 0) || (bounding_rects[i].height == 0)) continue;

            for (j = (i + 1); j < n_rects; ++j) {
                // skip collapsed rectangles
                if ((bounding_rects[j].width == 0) || (bounding_rects[j].height == 0)) continue;

                if (rect_overlap(bounding_rects[i], bounding_rects[j])) {
                    bounding_rects[i] = rect_collapse(bounding_rects[i], bounding_rects[j]);
                    bounding_rects[j] = NULL_RECT;
                }
            }
        }

        for (i = 0; i < n_rects; ++i) {
            GstEvent     *event;
            GstStructure *structure;
            CvRect        r;

            // skip collapsed rectangles
            r = bounding_rects[i];
            if ((r.width == 0) || (r.height == 0)) continue;

            structure = gst_structure_new("bgfg-roi",
                                          "x",         G_TYPE_UINT,   r.x,
                                          "y",         G_TYPE_UINT,   r.y,
                                          "width",     G_TYPE_UINT,   r.width,
                                          "height",    G_TYPE_UINT,   r.height,
                                          "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP(buf),
                                          NULL);

            event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
            gst_pad_send_event(filter->sinkpad, event);

            if (filter->verbose)
                GST_INFO("[roi] x: %d, y: %d, width: %d, height: %d\n",
                         r.x, r.y, r.width, r.height);

            if (filter->display)
                cvRectangle(filter->image, cvPoint(r.x, r.y), cvPoint(r.x + r.width, r.y + r.height),
                            CV_RGB(0, 0, 255), 1, 0, 0);
        }

        g_free(bounding_rects);
    }

    if (filter->display)
        gst_buffer_set_data(buf, (guchar*) filter->image->imageData, filter->image->imageSize);

    return gst_pad_push(filter->srcpad, buf);
}

static inline gboolean
rect_overlap(const CvRect r1, const CvRect r2)
{
    guint r1_top, r1_bottom, r1_left, r1_right;
    guint r2_top, r2_bottom, r2_left, r2_right;

    r1_top  = r1.y; r1_bottom = r1.y + r1.height;
    r1_left = r1.x; r1_right  = r1.x + r1.width;

    r2_top  = r2.y; r2_bottom = r2.y + r2.height;
    r2_left = r2.x; r2_right  = r2.x + r2.width;

    return ((r1_top  <= r2_bottom) &&
            (r2_top  <= r1_bottom) &&
            (r1_left <= r2_right)  &&
            (r2_left <= r1_right));
}

static inline CvRect
rect_collapse(const CvRect r1, const CvRect r2)
{
    CvRect r = cvRect(MIN(r1.x, r2.x), MIN(r1.y, r2.y), 0, 0);

    r.width  = MAX(r1.x + r1.width, r2.x + r2.width) - r.x;
    r.height = MAX(r1.y + r1.height, r2.y + r2.height) - r.y;

    return r;
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features
gboolean
gst_bgfg_acmmm2003_plugin_init (GstPlugin * plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_bgfg_acmmm2003_debug, "bgfgacmmm2003", 0,
                            "Performs background detection on videos using the paper 'Foreground Object Detection from Videos Containing Complex Background', by Li, Huan, Gu, Tian, which was published on the ACM Multimedia 2003 Conference");

    return gst_element_register(plugin, "bgfgacmmm2003", GST_RANK_NONE, GST_TYPE_BGFG_ACMMM2003);
}
