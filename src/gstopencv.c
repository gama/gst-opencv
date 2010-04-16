/* GStreamer
 * Copyright (C) <2009> Kapil Agrawal <kapil@mediamagictechnologies.com>
 *
 * gstopencv.c: plugin registering
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstbgfgacmmm2003.h"
#include "gstbgfgcodebook.h"
#include "gstedgedetect.h"
#include "gstfaceblur.h"
#include "gstfacemetrix.h"
#include "gsthaaradjust.h"
#include "gsthaardetect.h"
#include "gsthomography.h"
#include "gstmotiontemplate.h"
#include "gstlkopticalflow.h"
#include "gstobjectdistances.h"
#include "gstobjectsareainteraction.h"
#include "gstobjectsinteraction.h"
#include "gstinterpreterinteraction.h"
#include "gstoptflowtracker.h"
#include "gstpyramidsegment.h"
#include "gststaticobjects.h"
#include "gstsurftracker.h"
#include "gsttemplatematch.h"
#include "gsttracker.h"

static gboolean
plugin_init(GstPlugin *plugin)
{
  if (!gst_bgfg_acmmm2003_plugin_init (plugin))
    return FALSE;

  if (!gst_bgfg_codebook_plugin_init (plugin))
    return FALSE;

  if (!gst_edgedetect_plugin_init (plugin))
    return FALSE;

  if (!gst_faceblur_plugin_init (plugin))
    return FALSE;

  if (!gst_facemetrix_plugin_init (plugin))
    return FALSE;

  if (!gst_haar_adjust_plugin_init (plugin))
    return FALSE;

  if (!gst_haar_detect_plugin_init (plugin))
    return FALSE;

  if (!gst_homography_plugin_init (plugin))
    return FALSE;

  if (!gst_interpreter_interaction_plugin_init (plugin))
    return FALSE;

  if (!gst_lkopticalflow_plugin_init (plugin))
    return FALSE;

  if (!gst_motion_template_plugin_init (plugin))
    return FALSE;

  if (!gst_object_distances_plugin_init (plugin))
    return FALSE;

  if (!gst_objectsareainteraction_plugin_init (plugin))
    return FALSE;

  if (!gst_objectsinteraction_plugin_init (plugin))
    return FALSE;

  if (!gst_optical_flow_tracker_plugin_init (plugin))
    return FALSE;

  if (!gst_pyramidsegment_plugin_init (plugin))
    return FALSE;

  if (!gst_static_objects_plugin_init (plugin))
    return FALSE;

  if (!gst_surf_tracker_plugin_init (plugin))
    return FALSE;

  if (!gst_templatematch_plugin_init (plugin))
    return FALSE;

  if (!gst_tracker_plugin_init (plugin))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "opencv",
    "GStreamer OpenCV Plugins",
    plugin_init, VERSION, "LGPL", "OpenCv", "http://opencv.willowgarage.com")
