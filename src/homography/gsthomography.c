/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
 * Copyright (C) 2010 Lucas Pantuza Amorim <lucas@vettalabs.com>
 * Copyright (C) 2010 Gustavo Machado C. Gama <gama@vettalabs.com>
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
 * SECTION:element-homography
 *
 * Converts objects coordinates in the image plane to a reference plane using
 * supplied matrix.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-0.10 videotestsrc     !
 *                 decodebin        !
 *                 ffmpegcolorspace !
 *                 bgfgcodebook     !
 *                 haardetect       !
 *                 haaradjust       !
 *                 objectstracker   !
 *                 homography display=true verbose=true matrix=-0.041083,-0.013256,15.446693,0.000000,-0.213482,68.383575,0.000000,-0.005763,1.000000,0.5 !
 *                 ffmpegcolorspace !
 *                 autoimagesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsthomography.h"
#include "tracked-object.h"
#include "draw.h"

#include <gst/gst.h>
#include <gst/gststructure.h>
#include <cvaux.h>
#include <highgui.h>

GST_DEBUG_CATEGORY_STATIC(gst_homography_debug);
#define GST_CAT_DEFAULT gst_homography_debug

#define OBJECT_COLOR CV_RGB(31, 31, 127)

enum {
    PROP_0,
    PROP_VERBOSE,
    PROP_DISPLAY,
    PROP_MATRIX
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

GST_BOILERPLATE(GstHomography, gst_homography, GstElement, GST_TYPE_ELEMENT);

static void          gst_homography_set_property     (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_homography_get_property     (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean      gst_homography_set_caps         (GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_homography_chain            (GstPad *pad, GstBuffer *buf);
static gboolean      gst_homography_events_cb        (GstPad *pad, GstEvent *event, gpointer user_data);
static gboolean      gst_homography_parse_matrix_str (GstHomography *filter);
static void          gst_homography_draw_grid_mask   (GstHomography *filter);

#define convert_point_coordinates(src_point, dst_point, matrix)                 \
{                                                                               \
    float z;                                                                    \
    (dst_point)->x = CV_MAT_ELEM(*(matrix), float, 0, 0) * (src_point)->x +     \
                     CV_MAT_ELEM(*(matrix), float, 0, 1) * (src_point)->y +     \
                     CV_MAT_ELEM(*(matrix), float, 0, 2);                       \
    (dst_point)->y = CV_MAT_ELEM(*(matrix), float, 1, 0) * (src_point)->x +     \
                     CV_MAT_ELEM(*(matrix), float, 1, 1) * (src_point)->y +     \
                     CV_MAT_ELEM(*(matrix), float, 1, 2);                       \
    z              = CV_MAT_ELEM(*(matrix), float, 2, 0) * (src_point)->x +     \
                     CV_MAT_ELEM(*(matrix), float, 2, 1) * (src_point)->y +     \
                     CV_MAT_ELEM(*(matrix), float, 2, 2);                       \
    (dst_point)->x /= (fabs(z) > 0.0) ? z : INFINITY;                           \
    (dst_point)->y /= (fabs(z) > 0.0) ? z : INFINITY;                           \
}


static void
gst_homography_finalize(GObject *obj)
{
    GstHomography *filter = GST_HOMOGRAPHY(obj);

    if (filter->image)      cvReleaseImage(&filter->image);
    if (filter->grid_mask)  cvReleaseImage(&filter->grid_mask);
    if (filter->matrix)     cvReleaseMat(&filter->matrix);
    if (filter->matrix_str) g_free(filter->matrix_str);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

// gobject vmethod implementations
static void
gst_homography_base_init(gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_set_details_simple(element_class,
                                         "homography",
                                         "Filter/Video",
                                         "Converts objects coordinates in the image plane to a reference plane",
                                         "Lucas Pantuza Amorim <lucas@vettalabs.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}

// initialize the homography's class
static void
gst_homography_class_init(GstHomographyClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class  = g_type_class_peek_parent(klass);

    gobject_class->finalize     = GST_DEBUG_FUNCPTR(gst_homography_finalize);
    gobject_class->set_property = gst_homography_set_property;
    gobject_class->get_property = gst_homography_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose",
                                                         "Sets whether debugging info should be printed to the standard output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display",
                                                         "Highligh the reference plane and the transformed object coordinates in the video output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MATRIX,
                                    g_param_spec_string("matrix", "Homography matrix",
                                                        "A matrix that converts coordinates from the source plane (usualy the image/viewport plane) to the destination plane (usually the \"floor\" of the scene)",
                                                         NULL, G_PARAM_READWRITE));
}

// initialize the new element
// instantiate pads and add them to element
// set pad calback functions
// initialize instance structure
static void
gst_homography_init(GstHomography *filter, GstHomographyClass *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_homography_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad,   GST_DEBUG_FUNCPTR(gst_homography_chain));

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    filter->verbose      = FALSE;
    filter->display      = FALSE;
    filter->matrix       = NULL;
    filter->matrix_str   = NULL;
    filter->objects_list = NULL;
}

static void
gst_homography_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstHomography *filter = GST_HOMOGRAPHY(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_DISPLAY:
            filter->display = g_value_get_boolean(value);
            break;
        case PROP_MATRIX:
            filter->matrix_str = g_value_dup_string(value);
            if (gst_homography_parse_matrix_str(filter) == FALSE)
                GST_WARNING_OBJECT(filter, "unable to parse matrix string: %s", filter->matrix_str);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_homography_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstHomography *filter = GST_HOMOGRAPHY(object);

    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_DISPLAY:
            g_value_set_boolean(value, filter->display);
            break;
        case PROP_MATRIX:
            g_value_take_string(value, filter->matrix_str);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static gboolean
gst_homography_set_caps(GstPad *pad, GstCaps *caps)
{
    GstHomography *filter;
    GstPad        *other_pad;
    GstStructure  *structure;
    gint           width, height, depth;

    filter    = GST_HOMOGRAPHY(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width",  &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth",  &depth);

    filter->image     = cvCreateImage(cvSize(width, height), depth / 3, 3);
    filter->grid_mask = cvCreateImage(cvSize(width, height), depth / 3, 1);

    if (filter->display)
        gst_homography_draw_grid_mask(filter);

    // add roi event probe on the sinkpad
    gst_pad_add_event_probe(filter->sinkpad, (GCallback) gst_homography_events_cb, filter);

    other_pad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);

    return gst_pad_set_caps(other_pad, caps);
}

// chain function; this function does the actual processing
static GstFlowReturn
gst_homography_chain(GstPad *pad, GstBuffer *buf)
{
    GstHomography *filter;
    GList         *iter;
    GstClockTime   timestamp;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_HOMOGRAPHY(GST_OBJECT_PARENT(pad));
    filter->image->imageData = (char*) GST_BUFFER_DATA(buf);

    // cache timestamp
    timestamp = GST_BUFFER_TIMESTAMP(buf);

    if (filter->display)
        // darken grid on the output image
        cvSubS(filter->image, cvScalarAll(31), filter->image, filter->grid_mask);

    // calculate the object's destination coordinates using a perspective
    // transformation with the homography matrix
    for (iter = filter->objects_list; iter != NULL; iter = iter->next) {
        TrackedObject *object, *new_object;
        GstEvent      *event;
        GstStructure  *structure;
        guint          i;

        // double check the timestamp; not sure this is really necessary for
        // this plugin
        object = (TrackedObject*) iter->data;
        if (object->timestamp != timestamp)
            continue;

        // allocate a new tracker object (which will be published on
        // an event with the coordinates on the destination plane),
        // copy the properties from original object and allocate a new
        // point_array element
        new_object = tracked_object_new();
        memcpy(new_object, object, sizeof(TrackedObject));
        new_object->point_array = g_array_sized_new(FALSE, FALSE, sizeof(CvPoint3D32f), object->point_array->len);

        // the basic math is as follows:
        //    HomographyMatrix                       => 3x3 matrix (M)
        //    Point coordinates in source plane      => vector [x, y,   1] (P)
        //    Point coordinates in destination plane => vector [x',y', z'] (Q)
        //    Q = M * P
        //    Final coordinates                      => [x'/z, y'/z]
        for (i = 0; i < object->point_array->len; ++i) {
            CvPoint      *src_point;
            CvPoint3D32f *dst_point;

            src_point = &g_array_index(object->point_array, CvPoint, i);
            dst_point = &g_array_index(new_object->point_array, CvPoint3D32f, i);

            convert_point_coordinates(src_point, dst_point, filter->matrix);

            if (filter->verbose)
                GST_DEBUG_OBJECT(filter, "object coordinates of pixel [%d, %d]: [%.4f, %.4f]\n",
                                 src_point->x, src_point->y, dst_point->x, dst_point->y);

            if (filter->display) {
                gchar *label;

                // draw a circle at the point
                cvCircle(filter->image, *src_point, 4, OBJECT_COLOR, CV_FILLED, 8, 0);

                // then, the line segment between the current point and the previous one
                if (i > 0) {
                    CvPoint *previous_point = &g_array_index(object->point_array, CvPoint, i - 1);
                    cvLine(filter->image, *previous_point, *src_point, OBJECT_COLOR, 2, 8, 0);
                }

                // then the label with the coordinates
                label = g_strdup_printf("[%.2f, %.2f]", dst_point->x, dst_point->y);
                printText(filter->image, cvPoint(src_point->x, src_point->y - 12), label, OBJECT_COLOR, 0.3, TRUE);
                g_free(label);
            }
        }

        // now, send a new event with the new object
        if ((structure = tracked_object_to_structure(new_object, "homography-object")) == NULL) {
            GST_WARNING_OBJECT(filter, "unable to build structure from tracked object \"%d\"", new_object->id);
            continue;
        }

        event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
        gst_pad_push_event(filter->srcpad, event);
    }

    gst_buffer_set_data(buf, (guint8*) filter->image->imageData, (guint) filter->image->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}

// callbacks
static gboolean
gst_homography_events_cb(GstPad *pad, GstEvent *event, gpointer user_data)
{
    GstHomography      *filter;
    const GstStructure *structure;

    filter = GST_HOMOGRAPHY(user_data);

    // sanity checks
    g_return_val_if_fail(pad    != NULL, FALSE);
    g_return_val_if_fail(event  != NULL, FALSE);
    g_return_val_if_fail(filter != NULL, FALSE);

    structure = gst_event_get_structure(event);

    // plugins possible: haar-detect-roi, haar-adjust-roi, object-tracking
    if ((structure != NULL) && (strcmp(gst_structure_get_name(structure), "tracked-object") == 0)) {
        TrackedObject *object = tracked_object_from_structure(structure);
        filter->objects_list = g_list_prepend(filter->objects_list, object);
    }

    return TRUE;
}

// static auxiliary functions

// parse matrix string
// The format used is similar to MatLab's: columns separated by commas;
// rows separated by semi-colons. i.e.:
//   "1,2,3;4,5,6;7,8,9" 
// will be parsed as the following matrix:
//   |1 2 3|
//   |4 5 6|
//   |7 8 9|
static gboolean
gst_homography_parse_matrix_str(GstHomography *filter)
{
    gchar **rows, **cols;
    guint   i, j, nrows, ncols;

    // sanity checks
    g_return_val_if_fail(filter->matrix_str != NULL, FALSE);

    // pre-compute the number of rows and columns to allocate the matrix
    rows  = g_strsplit(filter->matrix_str, ";", 0);
    nrows = g_strv_length(rows);
    cols  = g_strsplit(rows[0], ",", 0);
    ncols = g_strv_length(cols);
    g_strfreev(cols);

    // abort when parsing fails
    if ((nrows != 3) || (ncols != 3)) {
        g_warning("invalid matrix string (it should be a 3x3 matrix): \"%s\"", filter->matrix_str);
        return FALSE;
    }

    // allocate matrix
    filter->matrix = cvCreateMat(nrows, ncols, CV_32F);

    // loop through string vectors, filling in the matrix
    for (i = 0; rows[i] != NULL; ++i) {
        cols = g_strsplit(rows[i], ",", 0);
        for (j = 0; cols[j] != NULL; ++j) {
            // double check the lengths because the matrix string may have
            // more columns on the second, third, etc rows, and we used the
            // number of columns of the first row to allocate the matrix
            if ((i < nrows) && (j < ncols)) {
                // strtod() sets errno to nonzero on error
                errno = 0;
                cvmSet(filter->matrix, i, j, strtod(cols[j], NULL));
                if (errno != 0)
                    g_warning("invalid matrix element \"%s\"", cols[j]);
            }
        }
        // ensure all rows have the same number of columns
        if (j != ncols) {
            g_warning("invalid matrix string (columns 0 and %d have different sizes)", i);
            return FALSE;
        }
        g_strfreev(cols);
    }
    g_strfreev(rows);

    return TRUE;
}

// setup the grid mask that'll be drawn on top of each frame
static void
gst_homography_draw_grid_mask(GstHomography *filter)
{
    CvMat *inverse_matrix;
    gint  i;

    // reset the mask to full black screen
    cvSet(filter->grid_mask, cvScalarAll(0), 0);

    // calculate the inverse matrix to transform coordinates from the
    // destination plane (floor) to the source plane (viewport)
    inverse_matrix = cvCloneMat(filter->matrix);
    cvInvert(filter->matrix, inverse_matrix, CV_LU);

    #define MIN_COORD -20
    #define MAX_COORD 20

    // draw grid lines along the 'x' and 'y' axes
    for (i = MIN_COORD; i < MAX_COORD; ++i) {
        CvPoint      src_point1, src_point2;
        CvPoint2D32f dst_point1, dst_point2;

        src_point1.x = MIN_COORD; src_point1.y = i;
        src_point2.x = MAX_COORD; src_point2.y = i;

        convert_point_coordinates(&src_point1, &dst_point1, inverse_matrix);
        convert_point_coordinates(&src_point2, &dst_point2, inverse_matrix);
        cvLine(filter->grid_mask, cvPointFrom32f(dst_point1), cvPointFrom32f(dst_point2),
               cvScalarAll(255), (i == 0) ? 2 : 1, 8, 0);

        src_point1.x = i; src_point1.y = MIN_COORD;
        src_point2.x = i; src_point2.y = MAX_COORD;

        convert_point_coordinates(&src_point1, &dst_point1, inverse_matrix);
        convert_point_coordinates(&src_point2, &dst_point2, inverse_matrix);
        cvLine(filter->grid_mask, cvPointFrom32f(dst_point1), cvPointFrom32f(dst_point2),
               cvScalarAll(255), (i == 0) ? 2 : 1, 8, 0);
    }

    cvReleaseMat(&inverse_matrix);
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features
gboolean
gst_homography_plugin_init(GstPlugin *plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_homography_debug, "homography", 0,
            "Performs objects interaction");

    return gst_element_register(plugin, "homography", GST_RANK_NONE, GST_TYPE_HOMOGRAPHY);
}
