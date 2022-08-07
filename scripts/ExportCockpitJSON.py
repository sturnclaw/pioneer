# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# Export blender empties to a Pioneer cockpit json file as part of the creation
# workflow for clickable cockpits.

import bpy
import json
import os

bl_info = {
	"name": "Pioneer Cockpit Exporter",
	"author": "Axtel Sturnclaw",
	"version": (0,1,0),
	"blender": (3, 0, 0),
	"location": "File > Export",
	"description": "Export Pioneer cockpit prop placements as a JSON file",
	"warning": "",
	"category": "Import-Export"
}

COCKPIT_VERSION = 1

# ============================================================================

from mathutils import Matrix

# Transformation utils to convert blender's coordinate system to a right-handed OpenGL coordinate system
def export_location(loc):
    return [ loc[0], loc[2], -loc[1] ]

def export_orient(mat: Matrix):
    q = mat.to_3x3().to_quaternion()
    return [ q[0], q[1], q[3], -q[2] ]

# returns whether the given object is exportable as a cockpit prop instance
def is_exportable(obj):
    return "prop_id" in obj and type(obj["prop_id"]) == str

# ============================================================================

# ExportHelper is a helper class, defines filename and
# invoke() function which calls the file selector.
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.types import Operator

class ExportCockpitJSON(Operator, ExportHelper):
    """Export a Pioneer cockpit data file from specially-created Empties in the current scene"""
    bl_idname = "export_scene.cockpit_json"
    bl_label = "Export Cockpit JSON"

    # ExportHelper mixin class uses this
    filename_ext = ".json"

    filter_glob: StringProperty(
        default="*.json",
        options={'HIDDEN'},
        maxlen=255,  # Max internal buffer length, longer would be clamped.
    )

    # ========================================================================

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.

    active_collection: BoolProperty(
        name="Active Collection",
        description="Export only prop instances in the active collection or the entire file",
        default=True,
    )

    def export_obj(self, context, obj):
        return {
            "id": obj["prop_id"],
            "position": export_location(obj.location),
            "orient": export_orient(obj.matrix_world)
        }

    def execute(self, context):

        objects = []
        if self.active_collection:
            objects = context.collection.all_objects
        else:
            objects = context.scene.objects

        export_props = []

        for obj in objects:
            if is_exportable(obj):
                export_props.append(self.export_obj(context, obj))

        export = {
            "name": os.path.basename(bpy.data.filepath),
            "version": COCKPIT_VERSION,
            "model": context.scene.get('model_path', "default_cockpit"),
            "props": export_props
        }

        f = open(self.filepath, 'w', encoding='utf-8')
        f.write(json.dumps(export, indent=2))
        f.close()

        return {'FINISHED'}


# Only needed if you want to add into a dynamic menu
def menu_func_export(self, context):
    self.layout.operator(ExportCockpitJSON.bl_idname, text="Pioneer Cockpit (.json)")

# Register and add to the "file selector" menu (required to use F3 search "Text Export Operator" for quick access)
def register():
    bpy.utils.register_class(ExportCockpitJSON)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_class(ExportCockpitJSON)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()

    # test call
    # bpy.ops.export_scene.cockpit_json('INVOKE_DEFAULT')
