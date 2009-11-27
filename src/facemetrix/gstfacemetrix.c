/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Michael Sheldon <mike@mikeasoft.com>
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 * Copyright (C) 2009 Erickson Nascimento <erickson@vettalabs.com>
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

#include "gstfacemetrix.h"

#include <identifier_motion.h>
#include <condensation.h>

#include "draw.h"
#include "kmeans.h"
#include "sglclient.h"


/*
#include <unistd.h>
#include <gst/gst.h>
#include <highgui.h>
#include <glib/gprintf.h>

#include "draw.h"
*/
#include "gstfacemetrix.h"
#include <sys/time.h>

// transition matrix F describes model parameters at and k and k+1
static const float F[] = { 1, 1, 0, 1 };

#define DEFAULT_MAX_POINTS          500
#define DEFAULT_MIN_POINTS          20
#define DEFAULT_WIN_SIZE            10
#define DEFAULT_MOVEMENT_THRESHOLD  2.0

#define DEFAULT_STATE_DIM           4
#define DEFAULT_MEASUREMENT_DIM     4
#define DEFAULT_SAMPLE_SIZE         50
#define DEFAULT_MAX_SAMPLE_SIZE     10*DEFAULT_SAMPLE_SIZE
#define MIN_FACE_NEIGHBORS          5    //2~5
#define MIN_FACEBOX_SIDE            20
//#define MAX_NFACES                10

#define DEFAULT_PROFILE             "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml"
#define DEFAULT_SGL_HOST            "localhost"
#define DEFAULT_SGL_PORT            1500
#define DEFAULT_SOURCE              "gstfacemetrix"

#define TIMESTAMP_FORMAT            "%H:%M:%S-%d/%m/%Y"
#define TIMESTAMP_EXAMPLE           "HH:MM:SS-DD/MM/YYYY"

enum {
    PROP_0,
    PROP_VERBOSE,
    PROP_MAX_POINTS,
    PROP_MIN_POINTS,
    PROP_WIN_SIZE,
    PROP_MOVEMENT_THRESHOLD,
    PROP_SHOW_PARTICLES,
    PROP_SHOW_FEATURES,
    PROP_SHOW_FEATURES_BOX,
    PROP_SHOW_BORDERS,
    PROP_SAMPLE_SIZE,
    PROP_FRAMES_LEARN_BG,
    PROP_DISPLAY,
    PROP_PROFILE,
    PROP_SGL_HOST,
    PROP_SGL_PORT,
    PROP_SOURCE,
    PROP_SAVE_FACES,
    PROP_SAVE_PREFIX
};

GST_DEBUG_CATEGORY_STATIC (gst_facemetrix_debug);
#define GST_CAT_DEFAULT gst_facemetrix_debug

// Filter signals and args
enum
{
    LAST_SIGNAL
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
static gchar*        build_timestamp             ();

// clean up
static void
gst_facemetrix_finalize (GObject * obj)
{
    GstFacemetrix *filter = GST_FACEMETRIX (obj);

    if (filter->cvImage) {
        cvReleaseImage (&filter->cvImage);
        cvReleaseImage (&filter->cvGray);
        cvReleaseImage (&filter->cvMotion);
    }
    if (filter->sgl != NULL) {
        sgl_client_close(filter->sgl);
        g_object_unref(filter->sgl);
        filter->sgl = NULL;
    }

    if (filter->profile)     g_free(filter->profile);
    if (filter->sgl_host)    g_free(filter->sgl_host);
    if (filter->source)      g_free(filter->source);
    if (filter->save_prefix) g_free(filter->save_prefix);

    // Tracker code
    if (filter->image)        cvReleaseImage(&filter->image);
    if (filter->grey)         cvReleaseImage(&filter->image);
    if (filter->prev_grey)    cvReleaseImage(&filter->image);
    if (filter->pyramid)      cvReleaseImage(&filter->image);
    if (filter->prev_pyramid) cvReleaseImage(&filter->image);
    if (filter->points[0])    cvFree(&filter->points[0]);
    if (filter->points[1])    cvFree(&filter->points[1]);
    if (filter->status)       cvFree(&filter->status);
    if (filter->verbose)      g_print("\n");

    // Multi tracker
    if (filter->points_cluster) cvFree(&filter->points_cluster);
    //if (filter->vet_faces)      cvFree(&filter->vet_faces);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
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

    g_object_class_install_property(gobject_class, PROP_SOURCE,
                                    g_param_spec_string("source", "ID of the video source",
                                                        "ID of the video source (camera, stream, etc). This will be forwarded to the facemetrix server to identify the face models associated with this video source",
                                                        DEFAULT_SOURCE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SAVE_FACES,
                                    g_param_spec_boolean("save-faces", "Save detected faces",
                                                         "Save detected faces as JPEG images",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SAVE_PREFIX,
                                    g_param_spec_string("save-prefix", "Filename prefix of the saved images",
                                                        "Use the given prefix to define the name of the files with the detected faces. The full file paths will be '<save-prefix>_<source>_<timestamp>.jpg'",
                                                        "/tmp/facemetrix", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_VERBOSE,
                                    g_param_spec_boolean("verbose", "Verbose", "Sets whether the movement direction should be printed to the standard output.",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SHOW_PARTICLES,
                                    g_param_spec_boolean("show-particles", "Show particles", "Sets whether particles location should be printed to the video.",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SHOW_FEATURES,
                                    g_param_spec_boolean("show-features", "Show features", "Sets whether features location should be printed to the video.",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SHOW_FEATURES_BOX,
                                    g_param_spec_boolean("show-features-box", "Show features box", "Sets whether features box should be printed to the video.",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SHOW_BORDERS,
                                    g_param_spec_boolean("show-borders", "Show borders in features box", "Sets whether borders in features box should be printed to the video.",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MAX_POINTS,
                                    g_param_spec_uint("max-points", "Max points", "Maximum number of feature points.",
                                                      0, 2 * DEFAULT_MAX_POINTS, DEFAULT_MAX_POINTS, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MIN_POINTS,
                                    g_param_spec_uint("min-points", "Min points", "Minimum number of feature points accepted. If the number of points falls belows this threshold, another feature-selection is attempted",
                                                      0, DEFAULT_MAX_POINTS, DEFAULT_MIN_POINTS, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_WIN_SIZE,
                                    g_param_spec_uint("win-size", "Window size", "Size of the corner-subpixels window.",
                                                      0, 2 * DEFAULT_WIN_SIZE, DEFAULT_WIN_SIZE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MOVEMENT_THRESHOLD,
                                    g_param_spec_float("movement-threshold", "Movement threshold", "Threshold that defines what constitutes a left (< -THRESHOLD) or right (> THRESHOLD) movement (in average # of pixels).",
                                                       0.0, 20 * DEFAULT_MOVEMENT_THRESHOLD, DEFAULT_MOVEMENT_THRESHOLD, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SAMPLE_SIZE,
                                    g_param_spec_uint("sample-size", "Sample size", "Number of particles used in Condensation", 0, DEFAULT_MAX_SAMPLE_SIZE, DEFAULT_SAMPLE_SIZE, G_PARAM_READWRITE));
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

    filter->profile     = g_strdup(DEFAULT_PROFILE);
    filter->display     = TRUE;
    filter->sgl_host    = g_strdup(DEFAULT_SGL_HOST);
    filter->sgl_port    = DEFAULT_SGL_PORT;
    filter->sgl         = NULL;
    filter->source      = g_strdup(DEFAULT_SOURCE);
    filter->image_idx   = 0;
    filter->save_faces  = FALSE;
    filter->save_prefix = g_strdup_printf("%s" G_DIR_SEPARATOR_S "facemetrix", g_get_tmp_dir());

    // opencv-related properties
    filter->verbose            = FALSE;
    filter->show_particles     = FALSE;
    filter->show_features      = FALSE;
    filter->show_features_box  = FALSE;
    filter->show_borders       = FALSE;
    filter->max_points         = DEFAULT_MAX_POINTS;
    filter->min_points         = DEFAULT_MIN_POINTS;
    filter->win_size           = DEFAULT_WIN_SIZE;
    filter->movement_threshold = DEFAULT_MOVEMENT_THRESHOLD;
    filter->state_dim          = DEFAULT_STATE_DIM;
    filter->measurement_dim    = DEFAULT_MEASUREMENT_DIM;
    filter->sample_size        = DEFAULT_SAMPLE_SIZE;
    filter->framesProcessed    = 0;

    // multi tracker
    filter->nFaces = 0;
    filter->init = 0;

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
            filter->sgl_host = g_value_dup_string(value);
            break;
        case PROP_SGL_PORT:
            filter->sgl_port = g_value_get_uint(value);
            break;
        case PROP_SOURCE:
            filter->source = g_value_dup_string(value);
            break;
        case PROP_SAVE_FACES:
            filter->save_faces = g_value_get_boolean(value);
            break;
        case PROP_SAVE_PREFIX:
            filter->save_prefix = g_value_dup_string(value);
            break;
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean(value);
            break;
        case PROP_MAX_POINTS:
            filter->max_points = g_value_get_uint(value);
            break;
        case PROP_MIN_POINTS:
            filter->min_points = g_value_get_uint(value);
            break;
        case PROP_WIN_SIZE:
            filter->win_size = g_value_get_uint(value);
            break;
        case PROP_MOVEMENT_THRESHOLD:
            filter->win_size = g_value_get_float(value);
            break;
        case PROP_SHOW_PARTICLES:
            filter->show_particles = g_value_get_boolean(value);
            break;
        case PROP_SHOW_FEATURES:
            filter->show_features = g_value_get_boolean(value);
            break;
        case PROP_SHOW_FEATURES_BOX:
            filter->show_features_box = g_value_get_boolean(value);
            break;
        case PROP_SHOW_BORDERS:
            filter->show_borders = g_value_get_boolean(value);
            break;
        case PROP_SAMPLE_SIZE:
            filter->sample_size = g_value_get_uint(value);
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
            g_value_set_string(value, filter->sgl_host);
            break;
        case PROP_SGL_PORT:
            g_value_set_uint(value, filter->sgl_port);
            break;
        case PROP_SOURCE:
            g_value_set_string(value, filter->source);
            break;
        case PROP_SAVE_FACES:
            g_value_set_boolean(value, filter->save_faces);
            break;
        case PROP_SAVE_PREFIX:
            g_value_set_string(value, filter->save_prefix);
            break;
        case PROP_VERBOSE:
            g_value_set_boolean(value, filter->verbose);
            break;
        case PROP_MAX_POINTS:
            g_value_set_uint(value, filter->max_points);
            break;
        case PROP_MIN_POINTS:
            g_value_set_uint(value, filter->min_points);
            break;
        case PROP_WIN_SIZE:
            g_value_set_uint(value, filter->win_size);
            break;
        case PROP_MOVEMENT_THRESHOLD:
            g_value_set_float(value, filter->movement_threshold);
            break;
        case PROP_SHOW_PARTICLES:
            g_value_set_boolean(value, filter->show_particles);
            break;
        case PROP_SHOW_FEATURES:
            g_value_set_boolean(value, filter->show_features);
            break;
        case PROP_SHOW_FEATURES_BOX:
            g_value_set_boolean(value, filter->show_features_box);
            break;
        case PROP_SHOW_BORDERS:
            g_value_set_boolean(value, filter->show_borders);
            break;
        case PROP_SAMPLE_SIZE:
            g_value_set_uint(value, filter->sample_size);
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
    filter->cvMotion  = cvCreateImage(cvSize (width, height), IPL_DEPTH_8U, 1);
    filter->cvStorage = cvCreateMemStorage(0);

    // initialize sgl connection
    if ((filter->sgl = g_object_new(SGL_CLIENT_TYPE, NULL)) == NULL) {
        GST_WARNING("unable to create sgl client instance");
    } else {
        if (sgl_client_open(filter->sgl, filter->sgl_host, filter->sgl_port) == FALSE) {
            GST_WARNING("unable to connect to sgl server (%s:%u)", filter->sgl_host, filter->sgl_port);
            g_object_unref(filter->sgl);
            filter->sgl = NULL;
        }
    }

    // Tracker code
    filter->width_image   = width;
    filter->height_image  = height;
    filter->image         = cvCreateImage(cvSize(width, height), 8, 3);
    filter->background    = cvCreateImage(cvSize(width, height), 8, 3);
    filter->grey          = cvCreateImage(cvSize(width, height), 8, 1);
    filter->prev_grey     = cvCreateImage(cvSize(width, height), 8, 1);
    filter->pyramid       = cvCreateImage(cvSize(width, height), 8, 1);
    filter->prev_pyramid  = cvCreateImage(cvSize(width, height), 8, 1);
    filter->points[0]     = (CvPoint2D32f*) cvAlloc(filter->max_points * sizeof(filter->points[0][0]));
    filter->points[1]     = (CvPoint2D32f*) cvAlloc(filter->max_points * sizeof(filter->points[0][0]));
    filter->status        = (char*) cvAlloc(filter->max_points);
    filter->flags         = 0;
    filter->initialized   = FALSE;
    filter->ConDens = initCondensation(filter->state_dim, filter->measurement_dim, filter->sample_size, filter->width_image, filter->height_image);

    // Multi tracker
    filter->points_cluster  = (int*) cvAlloc(filter->max_points * sizeof(int));

    // setup font scaling according to the frame size
    filter->font_scaling = ((width * height) > (320 * 240)) ? 0.5 : 0.3;

    otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
    gst_object_unref(filter);
    return gst_pad_set_caps(otherpad, caps);
}

// chain function - this function does the actual processing
static GstFlowReturn
gst_facemetrix_chain(GstPad *pad, GstBuffer *buf)
{
    GstFacemetrix *filter;

    filter = GST_FACEMETRIX (GST_OBJECT_PARENT (pad));
    filter->cvImage->imageData = (char *) GST_BUFFER_DATA (buf);

    cvCvtColor(filter->cvImage, filter->cvGray, CV_RGB2GRAY);
    cvClearMemStorage(filter->cvStorage);

    // Tracker code
    IplImage *swap_temp;
    CvPoint2D32f *swap_points;
    float avg_x = 0.0;
    filter->image->imageData = (char *) GST_BUFFER_DATA(buf);
    cvCvtColor(filter->image, filter->grey, CV_BGR2GRAY);
    
    // Detect frames rect motion
    int i, j;
    CvSeq *motions = motion_detect_mult(filter->cvImage, filter->cvMotion);
    for (i = 0; i < motions->total; i++) {

        CvRect rect_motion = ((CvConnectedComp*) cvGetSeqElem(motions, i))->rect;

        // If already tracker face in this motionBox, discard then...
        /*
        int m;
        int existFaceInThisMotionRect = 0;
        for (m=0; m<filter->nFaces; ++m)
            if(rectIntercept(&rect_motion, &filter->vet_faces[m].rect)){
                existFaceInThisMotionRect = 1;
                break;
            }
        if(existFaceInThisMotionRect) continue;
        */

        // display motion box
        cvRectangle(filter->cvImage,
                    cvPoint(rect_motion.x, rect_motion.width),
                    cvPoint(rect_motion.x + rect_motion.width, rect_motion.y + rect_motion.height),
                    COLOR_WHITE, 1, 8, 0);

        // For each motion box, detect his faces
        if (filter->cvCascade && (rect_motion.width + rect_motion.height != 0)) {

            cvSetImageROI(filter->cvGray, rect_motion);
            CvSeq *faces = cvHaarDetectObjects(
                filter->cvGray,
                filter->cvCascade,
                filter->cvStorage,
                1.1,
                MIN_FACE_NEIGHBORS,
                //0|CV_HAAR_FIND_BIGGEST_OBJECT,
                0|CV_HAAR_SCALE_IMAGE,
                cvSize (MIN_FACEBOX_SIDE, MIN_FACEBOX_SIDE));
            cvResetImageROI(filter->cvGray);

            for (j = 0; j < (faces ? faces->total : 0); j++) {
                int     m;
                CvMat   face;
                CvRect *rect = (CvRect*) cvGetSeqElem(faces, j);
                gchar  *id   = SGL_UNKNOWN_FACE_ID;

                // adjust rectangle coordinates due to ROI
                rect->x += rect_motion.x;
                rect->y += rect_motion.y;

                cvGetSubRect(filter->cvImage, &face, *rect);

                if (!CV_IS_MAT(&face)) {
                    GST_WARNING("CvGetSubRect: unable to grab face sub-image");
                    break;
                }

                if (filter->save_faces) {
                    gchar *filename, *timestamp;

                    timestamp = build_timestamp();
                    filename = g_strdup_printf("%s_%s_%s.jpg", filter->save_prefix, filter->source, timestamp);

                    if (cvSaveImage(filename, &face, 0) == FALSE)
                        GST_ERROR("unable to save detected face image to file '%s'", filename);
                    else if (filter->verbose)
                        g_print(">> face detected (saved as %s)\n", filename);

                    g_free(timestamp);
                    g_free(filename);
                }

                // if a face has already been recognized within the same region, skip
                // the request to the facemetrix server
                gboolean has_face = FALSE;
                for (m = 0; m < filter->nFaces; ++m)
                    if (rectIntercept(rect, &filter->vet_faces[m].rect)) {
                        has_face = TRUE;
                        break;
                    }
                if (has_face) {
                    cvDecRefData(&face);
                    continue;
                }

                if (filter->sgl != NULL) {
                    GstStructure *structure;
                    GstMessage   *message;
                    CvMat        *jpegface;

                    jpegface= cvEncodeImage(".jpg", &face, NULL);
                    if (!CV_IS_MAT(jpegface)) {
                        GST_WARNING("CvGetSubRect: unable to convert face sub-image to jpeg format");
                        cvDecRefData(&face);
                        break;
                    }
                    if ((id = sgl_client_recognize(filter->sgl, filter->source, FALSE, (gchar*) jpegface->data.ptr,
                                                   jpegface->rows * jpegface->step, NULL, NULL, NULL, NULL)) == NULL)
                        GST_WARNING("[sgl] unable to get user id");
                    else if (filter->verbose)
                        g_print("[sgl] id: %s\n", id);

                    // send bus message with the face info
                    structure = gst_structure_new("face-detected",
                                                  "x",       G_TYPE_UINT,   rect->x,
                                                  "y",       G_TYPE_UINT,   rect->y,
                                                  "width",   G_TYPE_UINT,   rect->width,
                                                  "height",  G_TYPE_UINT,   rect->height,
                                                  "face-id", G_TYPE_STRING, id, NULL);

                    message = gst_message_new_element(GST_OBJECT(filter), structure);
                    gst_element_post_message(GST_ELEMENT(filter), message);

                    // release/free/unref alloced variables
                    cvReleaseMat(&jpegface);
                }

                cvDecRefData(&face);

                // draw first face box
                draw_face_id(filter->cvImage, id, rect, COLOR_GREEN, filter->font_scaling, TRUE);

                // save the face if it has been identified
                if (filter->nFaces < MAX_NFACES && strcmp(id, SGL_UNKNOWN_FACE_ID) != 0) {
                    filter->vet_faces[filter->nFaces].rect = *rect;
                    sprintf(filter->vet_faces[filter->nFaces].name, "%s", id);
                    filter->nFaces++;

                    // reset the tracker
                    filter->init = 0;
                }

            } // for faces
        } // if cascade
    } // for motions


    // if the face has been located, then starts tracker
    if (filter->nFaces) {
        int u;

        // Init multi tracker
        if(!filter->init){
            filter->init = 1;

            CvPoint2D32f *points_temp = (CvPoint2D32f*) cvAlloc(filter->max_points * sizeof(points_temp[0]));
            int count_temp;
            int count_n_agreg = 0;
            int i_faces;
            for(i_faces = 0; i_faces < filter->nFaces; ++i_faces){

                // automatic initialization
                IplImage* eig         = cvCreateImage(cvGetSize(filter->grey), 32, 1);
                IplImage* temp        = cvCreateImage(cvGetSize(filter->grey), 32, 1);

                int i;
                double quality      = 0.01;
                double min_distance = 10;

                cvSetImageROI( filter->grey, filter->vet_faces[i_faces].rect );

                filter->count = filter->max_points;
                filter->prev_avg_x = -1.0;
                cvGoodFeaturesToTrack(filter->grey, eig, temp, points_temp, &(filter->count), quality,
                                      min_distance, 0, 3, 0, 0.04);

                count_temp = filter->count;

                int win_size;
                if (filter->vet_faces[i_faces].rect.width <= (10*2+5) || filter->vet_faces[i_faces].rect.height <= (10*2+5)){
                    win_size = filter->vet_faces[i_faces].rect.width < filter->vet_faces[i_faces].rect.height ? filter->vet_faces[i_faces].rect.width : filter->vet_faces[i_faces].rect.height;
                    win_size = (win_size-5)/2;
                }else win_size = filter->win_size;

                cvFindCornerSubPix(filter->grey, points_temp, count_temp, cvSize(win_size, win_size),
                                   cvSize(-1, -1), cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03));

                // Displacement coordinates according ROI
                for(i = 0; i < count_temp; i++){
                    filter->points[1][count_n_agreg].x = points_temp[i].x + filter->vet_faces[i_faces].rect.x;
                    filter->points[1][count_n_agreg].y = points_temp[i].y + filter->vet_faces[i_faces].rect.y;
                    ++count_n_agreg;
                }

                // Storage the first features centroid of this face
                CvRect rect = rectBoudingIdx(points_temp, count_temp, -1, NULL);
                CvPoint point = cvPoint(rect.x+(rect.width/2), rect.y+(rect.height/2));
                filter->vet_faces[i_faces].point.x = point.x + filter->vet_faces[i_faces].rect.x;
                filter->vet_faces[i_faces].point.y = point.y + filter->vet_faces[i_faces].rect.y;
                filter->vet_faces[i_faces].nPoints = count_temp;
                filter->vet_faces[i_faces].nPoints_orig = count_temp;

                cvResetImageROI( filter->grey );
                cvReleaseImage(&eig);
                cvReleaseImage(&temp);

            }//for faces

            cvFree(&points_temp);
            filter->count = count_n_agreg;

        }else{

            int n_points_delete = 0;
            CvPoint2D32f *points_delete = (CvPoint2D32f*) cvAlloc(filter->max_points * sizeof(points_delete[0]));

            int i, k, j, m;
            cvCalcOpticalFlowPyrLK(filter->prev_grey, filter->grey, filter->prev_pyramid, filter->pyramid,
                                   filter->points[0], filter->points[1], filter->count, cvSize(filter->win_size, filter->win_size),
                                   3, filter->status, 0, cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03),
                                   filter->flags);
            filter->flags |= CV_LKFLOW_PYR_A_READY;

            doKmeans(filter->nFaces, filter->count, filter->points[1], filter->points_cluster);

            CvPoint vetCentroids[MAX_NFACES];
            for (m=0; m<filter->nFaces; ++m){
                CvRect rect = rectBoudingIdx(filter->points[1], filter->count, m, filter->points_cluster);
                vetCentroids[m].x = rect.x + rect.width/2;
                vetCentroids[m].y = rect.y + rect.height/2;

                if (filter->show_particles)
                    cvCircle( filter->cvImage, vetCentroids[m], 3, CV_RGB(255,0,0), -1, 8,0);
            }

            //double predicted_x, predicted_y;
            CvRect *particleRects = (CvRect *)malloc(filter->nFaces * sizeof (CvRect));
            updateCondensation(filter->cvImage, filter->ConDens, vetCentroids, filter->nFaces, filter->show_particles); // points is the measured points

            int *vetParticlesIdx = (int *)malloc(filter->ConDens->SamplesNum * sizeof (int));
            floatDoKmeans(filter->nFaces, filter->ConDens->SamplesNum, filter->ConDens->flSamples, vetParticlesIdx);

            for(i = 0; i < filter->nFaces; ++i)
                particleRects[i] = floatRectBoudingIdx(filter->ConDens->flSamples, filter->ConDens->SamplesNum, i, vetParticlesIdx);
            free(vetParticlesIdx);

            for (j = i = k = 0; i < filter->count; ++i) {

                for(m = 0; m < filter->nFaces; ++m){
                    if (pointIntoRect(particleRects[m], cvPointFrom32f(filter->points[1][i])) && filter->status[i])
                        break;
                }

                if(m == filter->nFaces || !filter->status[i]) {
                    points_delete[j] = filter->points[1][i];
                    ++n_points_delete;
                    continue;
                }

                filter->points[1][k++] = filter->points[1][i];
                avg_x += (float) filter->points[1][i].x;

                if (filter->show_features)
                    cvCircle(filter->cvImage, cvPointFrom32f(filter->points[1][i]), 3, CV_RGB(255, 255, 0), -1, 8, 0);
            }
            free(particleRects);

            filter->count = k;
            avg_x /= (float) filter->count;

            // Decrements delete points of respective faces
            for (m=0; m<n_points_delete; ++m){
                int closerIdx = -1;
                float minDistTemp, minDist = -1;
                int h;
                for(h = 0; h < filter->nFaces; ++h){
                    minDistTemp = distRectToPoint(filter->vet_faces[h].rect, cvPointFrom32f(points_delete[m]));
                    if(minDist == -1 || minDistTemp < minDist){
                        minDist = minDistTemp;
                        closerIdx = h;
                    }
                }
                filter->vet_faces[closerIdx].nPoints--;
            }
            
            // Delete points if them rectFace with great loss of points off.
            for (i = k = 0; i < filter->count; ++i) {
                int thisToDel = 0;
                for (m=0; m<filter->nFaces; ++m){
                    if(filter->vet_faces[m].nPoints/filter->vet_faces[m].nPoints_orig < MINPOINTSKEEPFACE_PERC
                            && pointIntoRect(filter->vet_faces[m].rect, cvPointFrom32f(filter->points[1][i])))
                        ++thisToDel;
                    break;
                }
                if(thisToDel) continue;
                filter->points[1][k++] = filter->points[1][i];
            }
            filter->count = k;
            
            //Deletes rectFace with great loss of points or no points
            for(j = m = 0; j < filter->nFaces; ++j){
                if(!filter->vet_faces[j].nPoints ||
                        filter->vet_faces[j].nPoints/filter->vet_faces[j].nPoints_orig < MINPOINTSKEEPFACE_PERC ){
                    for(i = j; i < filter->nFaces; ++i)
                        filter->vet_faces[i] = filter->vet_faces[i+1];
                    ++m;
                }else{
                    filter->vet_faces[j].nPoints_orig = filter->vet_faces[j].nPoints;
                }
            }
            filter->nFaces -= m;

            // Locale update of rest faces
            if(filter->nFaces){

                // Find and update reference FaceRect
                doKmeans(filter->nFaces, filter->count, filter->points[1], filter->points_cluster);
                for (m=0; m<filter->nFaces; ++m){

                    // Get centroid of this face points
                    CvRect rect = rectBoudingIdx(filter->points[1], filter->count, m, filter->points_cluster);
                    CvPoint point = cvPoint(rect.x+(rect.width/2), rect.y+(rect.height/2));

                    // Locates the reference FaceRect (more closer)
                    int closerIdx = -1;
                    float minDistTemp, minDist = -1;
                    int h;
                    for(h = 0; h < filter->nFaces; ++h){
                        minDistTemp = distRectToPoint(filter->vet_faces[h].rect, point);
                        if(minDist == -1 || minDistTemp < minDist){
                            minDist = minDistTemp;
                            closerIdx = h;
                        }
                    }

                    // Update the FaceRect coordinates
                    filter->vet_faces[closerIdx].rect.x += point.x - filter->vet_faces[closerIdx].point.x;
                    filter->vet_faces[closerIdx].rect.y += point.y - filter->vet_faces[closerIdx].point.y;
                    filter->vet_faces[closerIdx].point.x = point.x;
                    filter->vet_faces[closerIdx].point.y = point.y;
                }
            }

            cvFree(&points_delete);
        }//init

        // Draw identified faces
        for (u = 0; u < filter->nFaces; ++u)
            draw_face_id(filter->cvImage, filter->vet_faces[u].name, &filter->vet_faces[u].rect,
                         COLOR_MAGENTA, filter->font_scaling, FALSE);

        filter->prev_avg_x = avg_x;

        CV_SWAP(filter->prev_grey, filter->grey, swap_temp);
        CV_SWAP(filter->prev_pyramid, filter->pyramid, swap_temp);
        CV_SWAP(filter->points[0], filter->points[1], swap_points);
        filter->initialized = TRUE;

    }// if nFaces
    gst_buffer_set_data(buf, (guint8*) filter->image->imageData, (guint) filter->image->imageSize);
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

static
gchar* build_timestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return g_strdup_printf("%ld%ld", tv.tv_sec, tv.tv_usec / 1000);
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
