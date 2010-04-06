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
 *                 bgfgcodebook                                !
 *                 haardetect                                  !
 *                 haaradjust                                  !
 *                 objectstracker                              !
 *                 objectsareainteraction verbose=true
 *                                        display=true
 *                                        display-area=true
 *                                        display-object=true
 *                                        contours=390x126,390x364,400x393,400x126
 *                                        homography_matrix=-0.041083,-0.013256,15.446693,0.000000,-0.213482,68.383575,0.000000,-0.005763,1.000000,0.5 !
 *                 ffmpegcolorspace ! xvimagesink sync=false
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

#define PRINT_COLOR_AREACONTOUR     CV_RGB(255, 0, 0)
#define PRINT_LINE_SIZE_AREACONTOUR 1
#define PRINT_COLOR_OBJCONTOUR      CV_RGB(0, 0, 255)
#define PRINT_LINE_SIZE_OBJCONTOUR  1
#define AREA_INTERACTION_COLOR      CV_RGB(19, 69,139 )
#define PRINT_LINE_SIZE_AI_ARROW    2

GST_DEBUG_CATEGORY_STATIC(gst_objectsareainteraction_debug);
#define GST_CAT_DEFAULT gst_objectsareainteraction_debug

enum {
    PROP_0,
    PROP_VERBOSE,
    PROP_CONTOURS,
    PROP_HOMOGRAPHY_MATRIX,
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

static void          gst_objectsareainteraction_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void          gst_objectsareainteraction_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean      gst_objectsareainteraction_set_caps     (GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_objectsareainteraction_chain        (GstPad *pad, GstBuffer *buf);
static gboolean      events_cb                               (GstPad *pad, GstEvent *event, gpointer user_data);
static CvPoint       make_contour                            (const gchar *str, CvSeq **seq, CvMemStorage* storage);
static CvPoint       point_convert                           (CvPoint pixel_point, CvMat *img2obj);

static void
gst_objectsareainteraction_finalize(GObject *obj)
{
    GstObjectsAreaInteraction *filter = GST_OBJECTSAREAINTERACTION(obj);
    if (filter->image) cvReleaseImage(&filter->image);
    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

// gobject vmethod implementations

static void
gst_objectsareainteraction_base_init(gpointer gclass)
{
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
gst_objectsareainteraction_class_init(GstObjectsAreaInteractionClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass*) klass;
    parent_class  = g_type_class_peek_parent(klass);

    gobject_class->finalize     = GST_DEBUG_FUNCPTR(gst_objectsareainteraction_finalize);
    gobject_class->set_property = gst_objectsareainteraction_set_property;
    gobject_class->get_property = gst_objectsareainteraction_get_property;

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose",
                                                         "Sets whether the movement direction should be printed to the standard output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY,
                                    g_param_spec_boolean("display", "Display",
                                                         "Highligh the interations in the video output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY_AREA,
                                    g_param_spec_boolean("display-area", "Display area",
                                                         "Highligh the settled areas in the video output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY_OBJECT,
                                    g_param_spec_boolean("display-object", "Display object",
                                                         "Highligh the objects tracker contours in the video output",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CONTOURS,
                                    g_param_spec_string("contours", "Contours",
                                                        "Settled contours",
                                                         NULL, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_HOMOGRAPHY_MATRIX,
                                    g_param_spec_string("homography_matrix", "Homography matrix",
                                                        "Homography matrix for conversion of 3d to 2d",
                                                         NULL, G_PARAM_READWRITE));
}

// initialize the new element
// instantiate pads and add them to element
// set pad calback functions
// initialize instance structure

static void
gst_objectsareainteraction_init(GstObjectsAreaInteraction *filter, GstObjectsAreaInteractionClass *gclass)
{
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_objectsareainteraction_set_caps));
    gst_pad_set_getcaps_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_objectsareainteraction_chain));

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_set_getcaps_function(filter->srcpad, GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    filter->verbose                      = FALSE;
    filter->display                      = FALSE;
    filter->display_area                 = FALSE;
    filter->display_object               = FALSE;
    filter->contours                     = NULL;
    filter->homography_matrix            = NULL;
    filter->img2obj                      = NULL;
    filter->distance_ratio_onepx_nmeters = 1.0f;
    filter->timestamp                    = 0;
    filter->contours_area_settled        = g_array_new(FALSE, FALSE, sizeof(InstanceObjectAreaContour));
    filter->contours_area_in             = g_array_new(FALSE, FALSE, sizeof(InstanceObjectAreaContour));
}

static void
gst_objectsareainteraction_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
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
        case PROP_HOMOGRAPHY_MATRIX:
            filter->homography_matrix = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_objectsareainteraction_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
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
            break;
        case PROP_HOMOGRAPHY_MATRIX:
            g_value_take_string(value, filter->homography_matrix);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static gboolean
gst_objectsareainteraction_set_caps(GstPad *pad, GstCaps *caps)
{
    GstObjectsAreaInteraction *filter;
    GstPad                    *other_pad;
    GstStructure              *structure;
    gint                       width, height, depth, i;

    filter    = GST_OBJECTSAREAINTERACTION(gst_pad_get_parent(pad));
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    gst_structure_get_int(structure, "depth", &depth);

    filter->image = cvCreateImage(cvSize(width, height), depth / 3, 3);

    // add roi event probe on the sinkpad
    gst_pad_add_event_probe(filter->sinkpad, (GCallback) events_cb, filter);

    other_pad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);

    // Set homography matrix
    if (filter->homography_matrix != NULL) {
        gchar **matrix;

        g_strcanon(filter->homography_matrix, "0123456789x-,.", ' ');
        filter->img2obj     = cvCreateMat(3, 3, CV_32F);
        matrix              = g_strsplit(filter->homography_matrix, ",", 10);

        for (i = 0; matrix[i] != NULL; ++i);
        if (i < 10)
            GST_ERROR_OBJECT(filter, "no homography_matrix[%i] have been set", i);

        CV_MAT_ELEM(*filter->img2obj, float, 0, 0) = atof(matrix[0]);
        CV_MAT_ELEM(*filter->img2obj, float, 0, 1) = atof(matrix[1]);
        CV_MAT_ELEM(*filter->img2obj, float, 0, 2) = atof(matrix[2]);
        CV_MAT_ELEM(*filter->img2obj, float, 1, 0) = atof(matrix[3]);
        CV_MAT_ELEM(*filter->img2obj, float, 1, 1) = atof(matrix[4]);
        CV_MAT_ELEM(*filter->img2obj, float, 1, 2) = atof(matrix[5]);
        CV_MAT_ELEM(*filter->img2obj, float, 2, 0) = atof(matrix[6]);
        CV_MAT_ELEM(*filter->img2obj, float, 2, 1) = atof(matrix[7]);
        CV_MAT_ELEM(*filter->img2obj, float, 2, 2) = atof(matrix[8]);
        filter->distance_ratio_onepx_nmeters       = atof(matrix[9]);

    } else {
        GST_ERROR_OBJECT(filter, "no homography_matrix have been set");
    }

    // Set settled contours
    if (filter->contours != NULL) {
        gchar   **str_area;

        str_area = g_strsplit(filter->contours, "-", -1);
        for (i = 0; str_area[i] != NULL; ++i) {
            gchar **str_labelpts;

            str_labelpts = g_strsplit(str_area[i], ":", 2);
            if ((str_labelpts[0] != NULL) && (str_labelpts[1] != NULL)) {
                InstanceObjectAreaContour contour_temp;

                contour_temp.id                         = i;
                contour_temp.name                       = g_strdup_printf("%s", str_labelpts[0]);
                contour_temp.mem_storage                = cvCreateMemStorage(0);
                contour_temp.centroid                   = make_contour(str_labelpts[1], &contour_temp.contour, contour_temp.mem_storage);
                contour_temp.area_contours_distance     = NULL; //does not apply

                g_array_append_val(filter->contours_area_settled, contour_temp);

            } else {
                GST_WARNING_OBJECT(filter, "unable to parse contour string: \"%s\"", str_area[i]);
            }

            g_strfreev(str_labelpts);
        }

        g_strfreev(str_area);
    }

    return gst_pad_set_caps(other_pad, caps);
}

// chain function; this function does the actual processing

static GstFlowReturn
gst_objectsareainteraction_chain(GstPad *pad, GstBuffer *buf)
{
    GstObjectsAreaInteraction *filter;
    guint                      i, j;

    // sanity checks
    g_return_val_if_fail(pad != NULL, GST_FLOW_ERROR);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    filter = GST_OBJECTSAREAINTERACTION(GST_OBJECT_PARENT(pad));
    filter->image->imageData = (char*) GST_BUFFER_DATA(buf);

    // Draw objects contour
    if ((filter->display_object) && (filter->contours_area_in != NULL) && (filter->contours_area_in->len > 0)) {
        InstanceObjectAreaContour *obj;
        for (i = 0; i < filter->contours_area_in->len; ++i) {
            obj = &g_array_index(filter->contours_area_in, InstanceObjectAreaContour, i);
            cvDrawContours(filter->image, obj->contour, PRINT_COLOR_OBJCONTOUR, PRINT_COLOR_OBJCONTOUR, 0, PRINT_LINE_SIZE_OBJCONTOUR, 8, cvPoint(0, 0));
        }
    }

    // Draw settled area contour
    if ((filter->display_area) && (filter->contours_area_settled != NULL) && (filter->contours_area_settled->len > 0)) {
        InstanceObjectAreaContour *obj;
        for (i = 0; i < filter->contours_area_settled->len; ++i) {
            obj = &g_array_index(filter->contours_area_settled, InstanceObjectAreaContour, i);
            cvDrawContours(filter->image, obj->contour, PRINT_COLOR_AREACONTOUR, PRINT_COLOR_AREACONTOUR, 0, PRINT_LINE_SIZE_AREACONTOUR, 8, cvPoint(0, 0));
        }
    }

    // Process all objects with settled areas and other objects
    if ((filter->contours_area_in != NULL) && (filter->contours_area_in->len > 0) && (filter->contours_area_settled != NULL) && (filter->contours_area_settled->len > 0)) {

        InstanceObjectAreaContour *obj_a, *obj_b;

        for (j = 0; j < filter->contours_area_in->len; ++j) {

            int i_area, i_obj;

            i_area  = 0;
            i_obj   = j + 1;

            while (i_area < filter->contours_area_settled->len || i_obj < filter->contours_area_in->len) {

                int              relationship_type_1objojb_0objarea;
                float            dist_m;
                char            *label;
                GstEvent        *event;
                GstMessage      *message;
                GstStructure    *structure;

                // Get object A to compare
                obj_a = &g_array_index(filter->contours_area_in, InstanceObjectAreaContour, j);

                // Get object B to compare
                if (i_area < filter->contours_area_settled->len) { //obj - area
                    obj_b = &g_array_index(filter->contours_area_settled, InstanceObjectAreaContour, i_area);
                    relationship_type_1objojb_0objarea = 0;
                    i_area++;
                } else { //obj - obj
                    obj_b = &g_array_index(filter->contours_area_in, InstanceObjectAreaContour, i_obj + j);
                    relationship_type_1objojb_0objarea = 1;
                    i_obj++;
                }

                // TODO calculate the sign to relationship obj<->obj (or delete the sign calculate)

                // calculates the sign (if area) and the distance
                dist_m = filter->distance_ratio_onepx_nmeters * euclid_dist_cvpoints(point_convert(obj_a->centroid, filter->img2obj), point_convert(obj_b->centroid, filter->img2obj));
                if(!relationship_type_1objojb_0objarea)
                    dist_m *= (obj_a->area_contours_distance[i_area] < dist_m) ? 1 : -1;

                // create the label of relationship
                label  = g_strdup_printf("'%s' %1.2f meters from the '%s'", obj_a->name, dist_m, obj_b->name);

                if (filter->display) {
                    cvLine(filter->image, obj_a->centroid, obj_b->centroid, AREA_INTERACTION_COLOR, PRINT_LINE_SIZE_AI_ARROW, 8, 0);
                    cvCircle(filter->image, obj_b->centroid, 4*PRINT_LINE_SIZE_AI_ARROW, AREA_INTERACTION_COLOR, -1, 8, 0);
                    printText(filter->image, obj_a->centroid, label, AREA_INTERACTION_COLOR, .4, 1);
                }

                if (filter->verbose) {
                    GST_INFO("%s\n", label);
                }

                g_free(label);

                // Send downstream event
                structure = gst_structure_new("object-areainteraction",

                        "obj_a_id",     G_TYPE_UINT,    obj_a->id,
                        "obj_a_name",   G_TYPE_STRING,  obj_a->name,
                        "obj_a_x",      G_TYPE_UINT,    obj_a->centroid.x,
                        "obj_a_y",      G_TYPE_UINT,    obj_a->centroid.y,

                        "obj_b_id",     G_TYPE_UINT,    obj_b->id,
                        "obj_b_name",   G_TYPE_STRING,  obj_b->name,
                        "obj_b_x",      G_TYPE_UINT,    obj_b->centroid.x,
                        "obj_b_y",      G_TYPE_UINT,    obj_b->centroid.y,

                        "distance",     G_TYPE_FLOAT,   dist_m,
                        "type",         G_TYPE_UINT,    relationship_type_1objojb_0objarea,
                        "timestamp",    G_TYPE_UINT64,  GST_BUFFER_TIMESTAMP(buf),

                        NULL);
                message = gst_message_new_element(GST_OBJECT(filter), gst_structure_copy(structure));
                gst_element_post_message(GST_ELEMENT(filter), message);
                event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure);
                gst_pad_push_event(filter->srcpad, event);
            }
        }
    }

    // Clean old objects
    if ((filter->contours_area_in != NULL) && (filter->contours_area_in->len > 0)) {
        int k;
        for (k = filter->contours_area_in->len - 1; k >= 0; --k) {
            InstanceObjectAreaContour *object;
            object = &g_array_index(filter->contours_area_in, InstanceObjectAreaContour, k);

            if (object->timestamp >= filter->timestamp) continue;

            cvClearSeq(object->contour);
            cvReleaseMemStorage(&object->mem_storage);
            g_free(object->name);
            g_free(object->area_contours_distance);
            g_array_remove_index_fast(filter->contours_area_in, k);
        }
    }

    // Update timestamp
    filter->timestamp = GST_BUFFER_TIMESTAMP(buf);

    gst_buffer_set_data(buf, (guint8*) filter->image->imageData, (guint) filter->image->imageSize);
    return gst_pad_push(filter->srcpad, buf);
}


// callbacks

static gboolean
events_cb(GstPad *pad, GstEvent *event, gpointer user_data)
{
    GstObjectsAreaInteraction *filter;
    const GstStructure        *structure;

    filter = GST_OBJECTSAREAINTERACTION(user_data);

    // sanity checks
    g_return_val_if_fail(pad != NULL, FALSE);
    g_return_val_if_fail(event != NULL, FALSE);
    g_return_val_if_fail(filter != NULL, FALSE);

    structure = gst_event_get_structure(event);

    // Plugins possible: haar-detect-roi, haar-adjust-roi, object-tracking
    if ((structure != NULL) && (strcmp(gst_structure_get_name(structure), "object-tracking") == 0)) {

        CvRect                       rect;
        gint                         id;
        CvContour                    header;
        CvSeqBlock                   block;
        CvMat                       *vector;
        InstanceObjectAreaContour   *contour_temp;
        gboolean                     old_object;
        unsigned int                 i;

        gst_structure_get((GstStructure*) structure,
                          "id",        G_TYPE_UINT,   &id,
                          "x",         G_TYPE_UINT,   &rect.x,
                          "y",         G_TYPE_UINT,   &rect.y,
                          "width",     G_TYPE_UINT,   &rect.width,
                          "height",    G_TYPE_UINT,   &rect.height,
                          NULL);

        vector = cvCreateMat(1, 4, CV_32SC2); // rect = 4 points
        CV_MAT_ELEM(*vector, CvPoint, 0, 0) = cvPoint(rect.x, rect.y);
        CV_MAT_ELEM(*vector, CvPoint, 0, 1) = cvPoint(rect.x, rect.y + rect.height);
        CV_MAT_ELEM(*vector, CvPoint, 0, 2) = cvPoint(rect.x + rect.width, rect.y + rect.height);
        CV_MAT_ELEM(*vector, CvPoint, 0, 3) = cvPoint(rect.x + rect.width, rect.y);

        // Located between existing objects
        old_object = FALSE;
        contour_temp = NULL;
        if ((filter->contours_area_in != NULL) && (filter->contours_area_in->len > 0)) {
            for (i = 0; i < filter->contours_area_in->len; ++i) {
                contour_temp = &g_array_index(filter->contours_area_in, InstanceObjectAreaContour, i);
                if (contour_temp->id == id) {
                    old_object = TRUE;
                    break;
                }
            }
        }

        // If area settled exist, calcule distance
        if ((filter->contours_area_settled != NULL) && (filter->contours_area_settled->len > 0)) {
            InstanceObjectAreaContour *obj_settled;

            if (old_object && contour_temp) {
                cvClearSeq(contour_temp->contour);
                cvReleaseMemStorage(&contour_temp->mem_storage);
            } else {
                contour_temp = (InstanceObjectAreaContour *) g_malloc(sizeof (InstanceObjectAreaContour));
                contour_temp->id = id;
                contour_temp->name = g_strdup_printf("OBJ#%i", id);
                contour_temp->area_contours_distance = (float *) g_malloc(sizeof (float) * filter->contours_area_settled->len);
            }

            contour_temp->mem_storage = cvCreateMemStorage(0);
            contour_temp->contour = cvCloneSeq(cvPointSeqFromMat(CV_SEQ_KIND_CURVE + CV_SEQ_FLAG_CLOSED, vector, &header, &block), contour_temp->mem_storage);
            contour_temp->centroid = cvPoint(rect.x + (rect.width / 2), rect.y + (rect.height));
            contour_temp->timestamp = filter->timestamp;

            // Distance between this object and each area (history)
            for (i = 0; i < filter->contours_area_settled->len; ++i) {
                float current_distance;
                int signal;

                obj_settled = &g_array_index(filter->contours_area_settled, InstanceObjectAreaContour, i);

                // signal "+", if new object or distance increasing OR signal "-", if distance decreasing
                current_distance = euclid_dist_cvpoints(obj_settled->centroid, contour_temp->centroid);
                signal = ((old_object) && (contour_temp->area_contours_distance[i] > current_distance)) ? -1 : 1;

                contour_temp->area_contours_distance[i] = signal * current_distance;
            }
        }

        // Include in array if not exist
        if (!old_object)
            g_array_append_val(filter->contours_area_in, *contour_temp);

    }

    return TRUE;
}

static CvPoint
make_contour(const gchar *str, CvSeq **seq, CvMemStorage* storage)
{
    CvContour       header;
    CvSeqBlock      block;
    CvMat          *vector;
    GArray         *array;
    unsigned int    ln, n;
    CvPoint         point_centroid;
    gchar         **str_pt;

    // Parser string to points array
    g_assert(str);
    array   = g_array_new(FALSE, FALSE, sizeof (CvPoint));
    str_pt  = g_strsplit(str, ",", -1);
    g_assert(str_pt);

    // Initialization of the centroids coordenates
    point_centroid.x = point_centroid.y = -1;

    ln = 0;
    while (str_pt[ln]) {

        if (strlen(str_pt[ln]) > 1) {

            gchar **str_pt_coord;
            CvPoint point_temp;

            // If point is centoid
            gboolean is_centroid = (g_strrstr(str_pt[ln], "c") != NULL) ? TRUE : FALSE;

            g_strcanon(str_pt[ln], "0123456789x", ' ');
            str_pt_coord = g_strsplit(str_pt[ln], "x", 2);
            g_assert(str_pt_coord);
            point_temp = cvPoint(atoi(str_pt_coord[0]), atoi(str_pt_coord[1]));

            if (is_centroid) {
                point_centroid.x = point_temp.x;
                point_centroid.y = point_temp.y;
            }else{
                g_array_append_val(array, point_temp);
            }

            g_strfreev(str_pt_coord);
        }
        ln++;
    }
    g_strfreev(str_pt);

    // Convert points array in MAT
    vector = cvCreateMat(1, array->len, CV_32SC2);

    for (n = 0; n < array->len; ++n)
        CV_MAT_ELEM(*vector, CvPoint, 0, n) = g_array_index(array, CvPoint, n);

    g_array_free(array, TRUE);

    // Convert MAT in SEQ
    *seq = cvCloneSeq(cvPointSeqFromMat(CV_SEQ_KIND_CURVE + CV_SEQ_FLAG_CLOSED, vector, &header, &block), storage);

    // If not have centroid in string, calculate this
    if (point_centroid.x == -1 && point_centroid.y == -1) {
        CvRect rect     = cvBoundingRect(*seq, 1);
        point_centroid  = cvPoint(rect.x + (rect.width / 2), rect.y + (rect.height));
    }

    return point_centroid;
}

static CvPoint
point_convert(CvPoint pixel_point, CvMat *img2obj) {

    CvPoint3D32f p;

    p.x = CV_MAT_ELEM(*img2obj, float, 0, 0) * pixel_point.x +
            CV_MAT_ELEM(*img2obj, float, 0, 1) * pixel_point.x +
            CV_MAT_ELEM(*img2obj, float, 0, 2);
    p.y = CV_MAT_ELEM(*img2obj, float, 1, 0) * pixel_point.x +
            CV_MAT_ELEM(*img2obj, float, 1, 1) * pixel_point.y +
            CV_MAT_ELEM(*img2obj, float, 1, 2);
    p.z = CV_MAT_ELEM(*img2obj, float, 2, 0) * pixel_point.x +
            CV_MAT_ELEM(*img2obj, float, 2, 1) * pixel_point.y +
            CV_MAT_ELEM(*img2obj, float, 2, 2);

    p.x /= (fabs(p.z) > 0.0) ? p.z : INFINITY;
    p.y /= (fabs(p.z) > 0.0) ? p.z : INFINITY;

    return cvPoint(p.x, p.y);
}

// entry point to initialize the plug-in; initialize the plug-in itself
// and registers the element factories and other features

gboolean
gst_objectsareainteraction_plugin_init(GstPlugin *plugin)
{
    // debug category for filtering log messages
    GST_DEBUG_CATEGORY_INIT(gst_objectsareainteraction_debug, "objectsareainteraction", 0,
            "Performs objects interaction");

    return gst_element_register(plugin, "objectsareainteraction", GST_RANK_NONE, GST_TYPE_OBJECTSAREAINTERACTION);
}
