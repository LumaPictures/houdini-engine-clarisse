#include <of_object.h>
#include <of_attr.h>
#include <of_app.h>
#include <dso_export.h>

#include <sys_thread_task_manager.h>
#include <geometry_point_cloud.h>
#include <curve_mesh.h>
#include <app_progress_bar.h>
#include <module_geometry.h>
#include <module_texture.h>

#include <iostream>

#include "hapi_curve_from_memory.cma"
#include "hapi_curve_from_memory.h"

IX_BEGIN_DECLARE_MODULE_CALLBACKS(HapiCurveFromMemory, ModuleGeometryCallbacks)
    static void init_class(OfClass& cls);
    static ResourceData *create_resource(OfObject& object, const int& resource_id, void *data);
    static void* create_module_data(const OfObject& object);
    static bool destroy_module_data(const OfObject& object, void* data);
IX_END_DECLARE_MODULE_CALLBACKS(HapiCurveFromMemory)

void hapi_curve_from_memory_on_register_module(OfApp& app, CoreVector<OfClass *>& new_classes)
{
    OfClass *new_class = IX_DECLARE_MODULE_CLASS(HapiCurveFromMemory);
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
    // Set which attributes the geometry resource depends on.
    CoreVector<CoreString> attrs;
    attrs.add("file_path");
    attrs.add("shape_name");
    attrs.add("rebuild_trigger");
    cls.set_resource_attrs(ModuleGeometry::RESOURCE_ID_GEOMETRY, attrs);
}

ResourceData *
IX_MODULE_CLBK::create_resource(OfObject& object, const int& resource_id, void* /*data*/)
{
    if (resource_id == ModuleGeometry::RESOURCE_ID_GEOMETRY) {
        auto* curve_data = reinterpret_cast<CurveFromMemoryData*>(object.get_module_data());

        if (curve_data->vertices.get_count() == 0) { return nullptr; }

        auto* curve_mesh = new CurveMesh;
        curve_mesh->init(curve_data->vertex_count, &curve_data->vertices[0]);
        if (curve_data->radii.get_count() == curve_data->vertices.get_count()) {
            curve_mesh->set_radius(curve_data->radii);
        }
        curve_data->clear();
        return curve_mesh;
    }
    return nullptr;
}

void*
IX_MODULE_CLBK::create_module_data(const OfObject& object)
{
    return new CurveFromMemoryData;
}

bool
IX_MODULE_CLBK::destroy_module_data(const OfObject& object, void* data)
{
    delete reinterpret_cast<CurveFromMemoryData*>(data);
    return true;
}
