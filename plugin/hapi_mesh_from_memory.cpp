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

#include "hapi_mesh_from_memory.cma"
#include "hapi_mesh_from_memory.h"

#include <iostream>


IX_BEGIN_DECLARE_MODULE_CALLBACKS(HapiMeshFromMemory, ModuleGeometryCallbacks)
    static void init_class(OfClass& cls);
    static ResourceData* create_resource(OfObject& object, const int& resource_id, void* data);
    static void* create_module_data(const OfObject& object);
    static bool destroy_module_data(const OfObject& object, void* data);
IX_END_DECLARE_MODULE_CALLBACKS(HapiMeshFromMemory)

void hapi_mesh_from_memory_on_register_module(OfApp& app, CoreVector<OfClass *>& new_classes)
{
    OfClass *new_class = IX_DECLARE_MODULE_CLASS(HapiMeshFromMemory);
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
    attrs.add("file_path");
    attrs.add("shape_name");
    attrs.add("rebuild_trigger");
    cls.set_resource_attrs(ModuleGeometry::RESOURCE_ID_GEOMETRY, attrs);

    /*
    // Not needed at this point.
    CoreVector<int> deps;
    deps.add(ModuleGeometry::RESOURCE_ID_GEOMETRY);
    cls.set_resource_deps(ModuleGeometry::RESOURCE_ID_GEOMETRY_PROPERTIES, deps);
    */
}

ResourceData *
IX_MODULE_CLBK::create_resource(OfObject& object, const int& resource_id, void *data)
{
    if (resource_id == ModuleGeometry::RESOURCE_ID_GEOMETRY)
    { // create the geometry resource
        MeshFromMemoryData* mesh_data = reinterpret_cast<MeshFromMemoryData*>(object.get_module_data());
        if (mesh_data->vertices.get_count() == 0) return 0;

        PolyMesh *polymesh = new PolyMesh;
        polymesh->set(mesh_data->vertices, mesh_data->velocities, mesh_data->polygon_vertex_indices,
                mesh_data->polygon_vertex_count, mesh_data->shading_group_indices, mesh_data->shading_groups,
                mesh_data->uv_maps, mesh_data->normal_maps, mesh_data->color_maps, 0);
        mesh_data->clear();
        return polymesh;
    }
    else if (resource_id == ModuleGeometry::RESOURCE_ID_GEOMETRY_PROPERTIES)
    { // create the properties (optional)
        return 0;
    }
    else if (resource_id == ModuleGeometry::RESOURCE_ID_COUNT)
    {
        return 0;
    }
    else
    {
        return 0;
    }
}

void*
IX_MODULE_CLBK::create_module_data(const OfObject& object)
{
    return new MeshFromMemoryData;
}

bool
IX_MODULE_CLBK::destroy_module_data(const OfObject& object, void* data)
{
    delete reinterpret_cast<MeshFromMemoryData*>(data);
    return true;
}
