#include <of_object.h>
#include <of_attr.h>
#include <of_app.h>
#include <dso_export.h>

#include <gmath.h>
#include <module.h>
#include <resource_property.h>
#include <geometry_point_cloud.h>
#include <poly_mesh.h>
#include <poly_mesh_property.h>
#include <app_progress_bar.h>
#include <core_vector.h>
#include <resource_property.h>
#include <geometry_point_property.h>
#include <particle_cloud.h>

#include "hapi_mesh.cma"
#include "hapi_object.h"
#include "hapi_utils.hpp"
#include "hapi_mesh.h"

#include <iostream>
#include <algorithm>
#include <map>
#include <pthread.h>

#include <HAPI/HAPI.h>

namespace {
    pthread_mutex_t g_rec_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
}


IX_BEGIN_DECLARE_MODULE_CALLBACKS(HapiMesh, ModuleGeometryCallbacks)
    static void init_class(OfClass& cls);
    static ResourceData* create_resource(OfObject& object, const int& resource_id, void* data);
    static void* create_module_data(const OfObject& object);
    static bool destroy_module_data(const OfObject& object, void* data);
IX_END_DECLARE_MODULE_CALLBACKS(HapiMesh)

void hapi_mesh_on_register_module(OfApp& app, CoreVector<OfClass *>& new_classes)
{
    OfClass *new_class = IX_DECLARE_MODULE_CLASS(HapiMesh);
    new_classes.add(new_class);

    IX_MODULE_CLBK *module_callbacks;
    IX_CREATE_MODULE_CLBK(new_class, module_callbacks)
    IX_MODULE_CLBK::init_class(*new_class);
    module_callbacks->cb_create_resource = IX_MODULE_CLBK::create_resource;
    module_callbacks->cb_create_module_data = IX_MODULE_CLBK::create_module_data;
    module_callbacks->cb_destroy_module_data = IX_MODULE_CLBK::destroy_module_data;
}

void
IX_MODULE_CLBK::init_class(OfClass& cls)
{
    CoreVector<CoreString> attrs;
    attrs.add("rebuild_trigger");
    attrs.add("file_path");
    attrs.add("shape_name");
    cls.set_resource_attrs(ModuleGeometry::RESOURCE_ID_GEOMETRY, attrs);

    // setting this up in the cid file does not seem to work
    OfAttr* attr = cls.get_attribute("rebuild_trigger");
    attr->set_flags(attr->get_flags() & ~OfAttr::FLAG_SAVEABLE);

    attr = cls.get_attribute("file_path");
    attr->set_flags(attr->get_flags() & ~OfAttr::FLAG_SAVEABLE);

    attr = cls.get_attribute("shape_name");
    attr->set_flags(attr->get_flags() & ~OfAttr::FLAG_SAVEABLE);

    /*
    // Not needed at this point.
    CoreVector<int> deps;
    deps.add(ModuleGeometry::RESOURCE_ID_GEOMETRY);
    cls.set_resource_deps(ModuleGeometry::RESOURCE_ID_GEOMETRY_PROPERTIES, deps);
    */
}

ResourceData *
IX_MODULE_CLBK::create_resource(OfObject& object, const int& resource_id, void* /*data*/)
{
    /*if (resource_id == ModuleGeometry::RESOURCE_ID_GEOMETRY)
    {
        auto* object_data = reinterpret_cast<HapiObjectData*>(object.get_module_data());
        if ((object_data->asset_id == INT_MIN) || (object_data->session == 0)) {
            return nullptr;
        }

        auto& part_info = object_data->part_info;

        if ((part_info.vertexCount == 0) ||
            (part_info.faceCount == 0) ||
            (part_info.attributeCounts == 0) ||
            (part_info.pointCount == 0)) {
            return nullptr;
        }

        auto& asset_id = object_data->asset_id;
        auto& object_id = object_data->object_id;
        auto& geo_id = object_data->geo_id;
        auto& part_id = object_data->part_id;
        const auto* session = object_data->session;

        CoreArray<GMathVec3f> vertices;
        CoreArray<unsigned int> polygon_vertex_count;
        CoreArray<unsigned int> polygon_vertex_indices;
        if (!export_geometry(session, asset_id, object_id, geo_id, part_id, part_info,
                vertices, polygon_vertex_count, polygon_vertex_indices)) {
            return nullptr;
        }

        CoreArray<GMathVec3f> velocities;
        CoreArray<GeometryUvMap> uv_maps;
        CoreArray<GeometryNormalMap> normal_maps;
        CoreArray<GeometryColorMap> color_maps;

        export_normals(session, asset_id, object_id, geo_id, part_id, part_info, polygon_vertex_indices, normal_maps);
        export_uvs(session, asset_id, object_id, geo_id, part_id, part_info, polygon_vertex_indices, uv_maps);

        // reading material
        CoreArray<CoreString> shading_groups;
        CoreArray<unsigned int> shading_group_indices;
        export_materials(session, asset_id, object_id, geo_id, part_id, part_info, &g_rec_mutex, shading_groups, shading_group_indices);

        auto* polymesh = new PolyMesh;
        polymesh->set(vertices, velocities, polygon_vertex_indices,
                polygon_vertex_count, shading_group_indices, shading_groups,
                uv_maps, normal_maps, color_maps, 0);
        return polymesh;
    }*/
    return nullptr;
}

void*
IX_MODULE_CLBK::create_module_data(const OfObject& object)
{
    return new HapiObjectData();
}

bool
IX_MODULE_CLBK::destroy_module_data(const OfObject& object, void* data)
{
    delete reinterpret_cast<HapiObjectData*>(data);
    return true;
}
