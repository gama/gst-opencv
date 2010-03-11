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

#ifndef __GST_OBJECTSTRACKER_H__
#define __GST_OBJECTSTRACKER_H__

#include <gst/gst.h>
#include <cv.h>
#include <draw.h>
#include <surf.h>

G_BEGIN_DECLS

#define GST_TYPE_OBJECTSTRACKER            (gst_objectstracker_get_type())
#define GST_OBJECTSTRACKER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OBJECTSTRACKER,GstObjectsTracker))
#define GST_OBJECTSTRACKER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OBJECTSTRACKER,GstObjectsTrackerClass))
#define GST_IS_OBJECTSTRACKER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OBJECTSTRACKER))
#define GST_IS_OBJECTSTRACKER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OBJECTSTRACKER))

#define PERC_RECT_TO_SAME_OBJECT           .6
#define PAIRS_PERC_CONSIDERATE             .6
#define PRINT_COLOR                        CV_RGB(205, 85, 85)
#define MIN_MATCH_OBJECT                   .15
#define DELOBJ_NFRAMES_IS_OLD              10
#define DELOBJ_COMBOFRAMES_IS_IRRELEVANT   3

typedef struct _InstanceObject InstanceObject;
typedef struct _GstObjectsTracker GstObjectsTracker;
typedef struct _GstObjectsTrackerClass GstObjectsTrackerClass;

struct _InstanceObject
{
    int                 id;
    int                 last_frame_viewed;
    int                 range_viewed;
    CvSeq              *surf_object_keypoints;
    CvSeq              *surf_object_descriptors;
    CvSeq              *surf_object_keypoints_last_match;
    CvSeq              *surf_object_descriptors_last_match;
    CvMemStorage       *mem_storage;
    CvRect              rect;
    CvRect              rect_estimated;
    GstClockTime        timestamp;
    GstClockTime        last_body_identify_timestamp;
};

struct _GstObjectsTracker
{
    GstElement          element;
    IplImage           *image;
    IplImage           *gray;

    GstPad             *sinkpad;
    GstPad             *srcpad;

    gboolean            verbose;
    gboolean            display;
    gboolean            display_features;

    int                 frames_processed;
    int                 static_count_objects;
    CvSURFParams        params;
    GstClockTime        rect_timestamp;
    GArray             *rect_array;
    GArray             *stored_objects;
};

struct _GstObjectsTrackerClass
{
    GstElementClass parent_class;
};

GType    gst_objectstracker_get_type    (void);
gboolean gst_objectstracker_plugin_init (GstPlugin *plugin);

G_END_DECLS

#endif /* __GST_OBJECTSTRACKER_H__ */
