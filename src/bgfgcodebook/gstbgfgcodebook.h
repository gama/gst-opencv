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

#ifndef __GST_BGFG_CODEBOOK_H__
#define __GST_BGFG_CODEBOOK_H__

#include <gst/gst.h>
#include <cv.h>
#include <cvaux.h>

G_BEGIN_DECLS

#define GST_TYPE_BGFG_CODEBOOK            (gst_bgfg_codebook_get_type())
#define GST_BGFG_CODEBOOK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BGFG_CODEBOOK,GstBgFgCodebook))
#define GST_BGFG_CODEBOOK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BGFG_CODEBOOK,GstBgFgCodebookClass))
#define GST_IS_BGFG_CODEBOOK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BGFG_CODEBOOK))
#define GST_IS_BGFG_CODEBOOK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BGFG_CODEBOOK))

typedef struct _GstBgFgCodebook GstBgFgCodebook;
typedef struct _GstBgFgCodebookClass GstBgFgCodebookClass;

struct _GstBgFgCodebook
{
    GstElement               element;

    GstPad                  *sinkpad;
    GstPad                  *srcpad;

    IplImage                *image;
    IplImage                *mask;
    CvBGCodeBookModel       *model;

    guint                    n_frames_learn_bg;
    guint                    n_frames;
    float                    perimeter_scale;
    guint                    n_erode_iterations;
    guint                    n_dilate_iterations;
    gboolean                 convex_hull;

    gboolean                 display;
    gboolean                 verbose;
    gboolean                 send_mask_events;
    gboolean                 send_roi_events;
};

struct _GstBgFgCodebookClass
{
    GstElementClass parent_class;
};

GType    gst_bgfg_codebook_get_type    (void);
gboolean gst_bgfg_codebook_plugin_init (GstPlugin *plugin);

G_END_DECLS

#endif /* __GST_BGFG_CODEBOOK_H__ */
