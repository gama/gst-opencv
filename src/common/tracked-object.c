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

#include "tracked-object.h"

#include <cv.h>

static const const gchar* type_strings[] = {"DYNAMIC", "STATIC"};

TrackedObject*
tracked_object_new()
{
    return g_new0(TrackedObject, 1);
}

void
tracked_object_free(TrackedObject *object)
{
    g_free(object->id);
    g_array_unref(object->point_array);
    g_free(object);
}

void
tracked_object_add_point(TrackedObject *object, const guint x, const guint y)
{
    CvPoint p = {x, y};

    if (object->point_array == NULL)
        object->point_array = g_array_sized_new(FALSE, FALSE, sizeof(CvPoint), 2);
    g_array_append_val(object->point_array, p);
}

GstStructure*
tracked_object_to_structure(const TrackedObject *object, const gchar* name)
{
    return gst_structure_new(name,
                             "id",          G_TYPE_STRING,  object->id,
                             "type",        G_TYPE_UINT,    object->type,
                             "point_array", G_TYPE_POINTER, object->point_array,
                             "height",      G_TYPE_UINT,    object->height,
                             "timestamp",   G_TYPE_UINT64,  object->timestamp,
                             NULL);
}

TrackedObject*
tracked_object_from_structure(const GstStructure *structure)
{
    TrackedObject *object;

    object = tracked_object_new();
    gst_structure_get((GstStructure*) structure,
                      "id",          G_TYPE_STRING,  &object->id,
                      "type",        G_TYPE_UINT,    &object->type,
                      "point_array", G_TYPE_POINTER, &object->point_array,
                      "height",      G_TYPE_UINT,    &object->height,
                      "timestamp",   G_TYPE_UINT64,  &object->timestamp,
                      NULL);

    // copy/ref fields that aren't passed by copy
    object->id = g_strdup(object->id);
    g_array_ref(object->point_array);

    return object;
}

gchar*
tracked_object_to_string(const TrackedObject *object)
{
    GString *buffer = g_string_new_len("", 64);
    guint    i;

    g_string_append_printf(buffer, "[id: %s, type: %s, points: [", object->id, type_strings[object->type]);
    for (i = 0; i < object->point_array->len; ++i) {
        CvPoint *p = &g_array_index(object->point_array, CvPoint, i);
        g_string_append_printf(buffer, "[%d, %d]", p->x, p->y);
    }
    g_string_append_printf(buffer, "], height: %d, timestamp: %lld", object->height, object->timestamp);

    return g_string_free(buffer, FALSE);
}
