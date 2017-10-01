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

#include "hapi_curve.cma"
#include "hapi_object.h"
#include "hapi_curve.h"

IX_BEGIN_DECLARE_MODULE_CALLBACKS(HapiCurve, ModuleGeometryCallbacks)
    static void init_class(OfClass& cls);
    static ResourceData *create_resource(OfObject& object, const int& resource_id, void *data);
    static void* create_module_data(const OfObject& object);
    static bool destroy_module_data(const OfObject& object, void* data);
IX_END_DECLARE_MODULE_CALLBACKS(HapiCurve)

void hapi_curve_on_register_module(OfApp& app, CoreVector<OfClass *>& new_classes)
{
    OfClass *new_class = IX_DECLARE_MODULE_CLASS(HapiCurve);
    new_classes.add(new_class);

    IX_MODULE_CLBK* module_callbacks;
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
    attrs.add("rebuild_trigger");
    attrs.add("file_path");
    attrs.add("rebuild_trigger");
    cls.set_resource_attrs(ModuleGeometry::RESOURCE_ID_GEOMETRY, attrs);
}

ResourceData*
IX_MODULE_CLBK::create_resource(OfObject& object, const int& resource_id, void* /*data*/)
{
    /*if (resource_id == ModuleGeometry::RESOURCE_ID_GEOMETRY) {
        auto* object_data = reinterpret_cast<HapiObjectData*>(object.get_module_data());
        if (object_data->asset_id == INT_MIN) {
            return nullptr;
        }

        auto& asset_id = object_data->asset_id;
        auto& object_id = object_data->object_id;
        auto& geo_id = object_data->geo_id;
        auto& part_id = object_data->part_id;
        const auto& curve_info = object_data->curve_info;
        const HAPI_Session* session = object_data->session;

        HAPI_AttributeInfo attr_info_p;
        if (HAPI_GetAttributeInfo(session, asset_id, object_id, geo_id, part_id, "P", HAPI_ATTROWNER_POINT, &attr_info_p) != HAPI_RESULT_SUCCESS) {
            return nullptr;
        }
        if ((attr_info_p.count == 0) || (attr_info_p.tupleSize != 3)) {
            return nullptr;
        }

        CoreArray<GMathVec3f> vertices(static_cast<unsigned int>(attr_info_p.count));
        HAPI_GetAttributeFloatData(session, asset_id, object_id, geo_id, part_id, "P", &attr_info_p, &vertices[0][0], 0, attr_info_p.count);

        const auto curve_count = curve_info.curveCount;
        CoreArray<int> curve_counts_int(static_cast<unsigned int>(curve_count));
        HAPI_GetCurveCounts(session, asset_id, object_id, geo_id, part_id, &curve_counts_int[0], 0, curve_count);
        CoreArray<unsigned int> curve_counts(static_cast<unsigned int>(curve_count));
        for (int i = 0; i < curve_count; ++i) {
            curve_counts[i] = static_cast<unsigned int>(curve_counts_int[i]);
        }

        auto* curve_mesh = new CurveMesh;
        curve_mesh->init(curve_counts, &vertices[0]);

        HAPI_AttributeInfo attr_info_pw;
        if (HAPI_GetAttributeInfo(session, asset_id, object_id, geo_id, part_id, "Pw", HAPI_ATTROWNER_POINT, &attr_info_pw) != HAPI_RESULT_SUCCESS) {
            return curve_mesh;
        }
        else if ((attr_info_pw.count == 0) || (attr_info_pw.tupleSize != 1) || (attr_info_pw.count != attr_info_p.count)) {
            return curve_mesh;
        }
        CoreArray<float> radii(static_cast<unsigned int>(attr_info_pw.count));
        HAPI_GetAttributeFloatData(session, asset_id, object_id, geo_id, part_id, "Pw", &attr_info_pw, &radii[0], 0, attr_info_pw.count);
        curve_mesh->set_radius(radii);

        return curve_mesh;
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
