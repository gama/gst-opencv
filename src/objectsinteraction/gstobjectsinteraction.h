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

#ifndef __GST_OBJECTSINTERACTION_H__
#define __GST_OBJECTSINTERACTION_H__

#include <gst/gst.h>
#include <cv.h>
#include <draw.h>

G_BEGIN_DECLS

#define GST_TYPE_OBJECTSINTERACTION            (gst_objectsinteraction_get_type())
#define GST_OBJECTSINTERACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OBJECTSINTERACTION,GstObjectsInteraction))
#define GST_OBJECTSINTERACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OBJECTSINTERACTION,GstObjectsInteractionClass))
#define GST_IS_OBJECTSINTERACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OBJECTSINTERACTION))
#define GST_IS_OBJECTSINTERACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OBJECTSINTERACTION))

#define PRINT_COLOR                        CV_RGB(100, 185, 185)

typedef struct _InstanceObjectIn InstanceObjectIn;
typedef struct _GstObjectsInteraction GstObjectsInteraction;
typedef struct _GstObjectsInteractionClass GstObjectsInteractionClass;

struct _InstanceObjectIn
{
    int                     id;
    CvRect               rect;
};

struct _GstObjectsInteraction
{
    GstElement        element;
    IplImage           *image;

    GstPad             *sinkpad;
    GstPad             *srcpad;

    gboolean            verbose;
    gboolean            display;

    GstClockTime      rect_timestamp;
    GArray              *object_in_array;
};

struct _GstObjectsInteractionClass
{
    GstElementClass parent_class;
};

GType    gst_objectsinteraction_get_type    (void);
gboolean gst_objectsinteraction_plugin_init (GstPlugin *plugin);

G_END_DECLS

#endif /* __GST_OBJECTSINTERACTION_H__ */
