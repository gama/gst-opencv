/*
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

#ifndef __GST_TRACKED_OBJECT__
#define __GST_TRACKED_OBJECT__

#include <gst/gst.h>

typedef enum   _TrackedObjectType TrackedObjectType;
typedef struct _TrackedObject     TrackedObject;

enum _TrackedObjectType
{
    TRACKED_OBJECT_DYNAMIC, // person, car...
    TRACKED_OBJECT_STATIC   // door, table, swimming pool, stove...
};

struct _TrackedObject
{
    gchar             *id;
    TrackedObjectType  type;
    GArray            *point_array;
    guint              height;
    GstClockTime       timestamp;
};

TrackedObject* tracked_object_new            ();

void           tracked_object_free           (TrackedObject *object);

void           tracked_object_add_point      (TrackedObject *object, const guint x, const guint y);

GstStructure*  tracked_object_to_structure   (const TrackedObject *object,
                                              const gchar* name);

TrackedObject* tracked_object_from_structure (const GstStructure *structure);

gchar*         tracked_object_to_string      (const TrackedObject *tracked_object);

#endif // __GST_TRACKED_OBJECT__
