/*
 *  TODO :
 *      * Handle user parameters
 *      * Instancing support (need scenes)
 *      * Support volume data
 *      * Read images?
 *      * Marshal geometry in
 *      * Marshal curves in
 *      * Setup timeline properly (find the right combination of clarisse functions)
 *      * Use less string conversions between hapi string, std::string and CoreString
 *      * Check if context operations are thread safe, if not add additional layer to handle that
 */


#include <of_object.h>
#include <of_attr.h>
#include <of_app.h>
#include <dso_export.h>
#include <of_ref_context_engine.h>
#include <of_context.h>
#include <of_object_factory.h>

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
#include <module_scene_object_tree.h>
#include <module_polymesh.h>
#include <of_time.h>
#include <app_object.h>
#include <app_builtin_commands.h>

#include <HAPI.h>

#include <iostream>
#include <string>
#include <vector>
#include <math.h>
#include <map>
#include <limits>

#include <unistd.h>
#include <signal.h>
#include <ctime>
#include <sstream>

#include <tbb/tbb_thread.h>
#include <tbb/tick_count.h>
#include <tbb/atomic.h>

#include "hapi_mesh_from_memory.h"
#include "hapi_curve_from_memory.h"

#include "hapi_object.h"
#include "hapi_utils.hpp"

namespace {
    bool g_is_interactive = true;
    unsigned int g_license_retry_wait = 5;
    double g_license_timeout = 60.0;
    double g_license_release = 30.0;

    enum BuiltinParameters{
        BUILTIN_FILENAME = 0,
        BUILTIN_RELOAD,
        BUILTIN_CLEAR,
        BUILTIN_CLEANUP,
        BUILTIN_GENERATING_ATTRIBUTES,
        BUILTIN_LAST_LOADED_FILE,
        BUILTIN_REBUILD_ON_PARAMETER_CHANGE,
        BUILTIN_LOADED_ASSET_ID,
        NUMBER_OF_BUILTIN_PARAMETERS
    };

    GMathTransform convert_hapi_transform(HAPI_Session* session, HAPI_Transform* transform)
    {
        GMathTransform out_transform;

        float matrix[16];
        HAPI_ConvertTransformQuatToMatrix(session, transform, matrix);

        GMathMatrix4x4d g_mat;
        for (int x = 0; x < 4; ++x)
        {
            for (int y = 0; y < 4; ++y)
                g_mat[x][y] = matrix[x + y * 4];
        }
        out_transform.set_global_matrix(g_mat);
        return out_transform;
    }

    template <OfAttr::Type param_type>
    void check_attribute(OfContext& context, OfAttr*& orig_attr, int param_size, OfAttr::VisualHint visual_hint)
    {
        if (orig_attr != 0)
        {
            if ((orig_attr->get_type() != param_type) ||
                (orig_attr->get_value_count() != param_size) ||
                (orig_attr->get_visual_hint() != visual_hint))
            {
                context.remove_attribute(orig_attr->get_name()); // TODO do I have to delete the attr?
                orig_attr = 0;
            }
        }
    }

    template <OfAttr::Type param_type>
    OfAttr* create_attribute(OfContext& context, const CoreString& param_name, int param_size, OfAttr::VisualHint visual_hint, const CoreString& group_name)
    {
        OfAttr* attr = 0;
        if (param_size == 1)
            attr = context.add_attribute(param_name, param_type, OfAttr::CONTAINER_SINGLE, visual_hint, group_name);
        else
        {
            attr = context.add_attribute(param_name, param_type, OfAttr::CONTAINER_ARRAY, visual_hint, group_name);
            attr->set_value_count(static_cast<unsigned int>(param_size));
        }
        return attr;
    }

    void generate_params(HAPI_Session* session, OfContext& context, HAPI_NodeId node_id)
    {
        // so we can track down which attributes haven't changed
        // or recreated and remove the ones that are not needed anymore
        // even though I'm deleting attributes when the filename changes,
        // it's possible that the OTL file changes between two reloads
        // thus changing the number and type of the parameters
        // so just to be safe clear out what we don't need
        std::map<std::string, bool> existing_attributes;
        const unsigned int attribute_count = context.get_attribute_count();
        for (unsigned int i = NUMBER_OF_BUILTIN_PARAMETERS; i < attribute_count; ++i)
        {
            OfAttr* attr = context.get_attribute(i);
            existing_attributes[attr->get_name().get_data()] = false;
        }

        // generating parameters
        HAPI_NodeInfo node_info;
        if (HAPI_GetNodeInfo(session, node_id, &node_info) != HAPI_RESULT_SUCCESS) return;
        if (node_info.parmCount == 0) return;
        std::vector<HAPI_ParmInfo> param_infos(static_cast<unsigned int>(node_info.parmCount));
        if (HAPI_GetParameters(session, node_info.id, &param_infos[0], 0, node_info.parmCount) != HAPI_RESULT_SUCCESS) return;
        const int param_count = node_info.parmCount;
        std::vector<int> param_ids(param_count, 0); // zero is probably root
        for (int i = 0; i < param_count; ++i)
            param_ids[param_infos[i].id] = i;
        OfAttr* generating_attributes = context.get_attribute(BUILTIN_GENERATING_ATTRIBUTES);
        generating_attributes->set_bool(true);
        for (int i = 0; i < param_count; ++i)
        {
            const HAPI_ParmInfo& param_info = param_infos[i];
            if (param_info.invisible || param_info.spare || param_info.disabled || HAPI_ParmInfo_IsNonValue(&param_info))
                continue;
            const CoreString param_name = convert_hapi_string(session, param_info.nameSH).c_str();

            OfAttr* orig_attr = 0;
            OfAttr* attr = 0;
            OfAttr::VisualHint visual_hint = OfAttr::VISUAL_HINT_DEFAULT;
            short attr_flags = OfAttr::FLAG_SAVEABLE;
            if (context.attribute_exists(param_name))
                orig_attr = context.get_attribute(param_name);

            const int param_size = param_info.size;

            CoreString group_name = "";
            if ((param_info.parentId >= 0) && (param_info.parentId < param_count))
                group_name = convert_hapi_string(session, param_infos[param_ids[param_info.parentId]].labelSH).c_str();

            if (HAPI_ParmInfo_IsInt(&param_info))
            {
                if ((param_size == 1) && (param_info.type == HAPI_PARMTYPE_TOGGLE))
                { // 1 integer with a type of toggle is a bool
                    check_attribute<OfAttr::TYPE_BOOL>(context, orig_attr, param_size, visual_hint);
                    if (orig_attr == 0)
                    {
                        attr = context.add_attribute(param_name, OfAttr::TYPE_BOOL, OfAttr::CONTAINER_SINGLE, visual_hint, group_name);

                        int param_value = 0;
                        if (HAPI_GetParmIntValues(session, node_id, &param_value, param_info.intValuesIndex, 1) != HAPI_RESULT_SUCCESS) continue;
                        attr->set_bool(param_value == 1);
                    }
                    else attr = orig_attr;
                }
                else
                {
                    check_attribute<OfAttr::TYPE_LONG>(context, orig_attr, param_size, visual_hint);
                    if (orig_attr == 0)
                    {
                        attr = create_attribute<OfAttr::TYPE_LONG>(context, param_name, param_size, visual_hint, group_name);

                        for (int si = 0; si < param_size; ++si)
                        {
                            int param_value = 0;
                            if (HAPI_GetParmIntValues(session, node_id, &param_value, param_info.intValuesIndex + si, 1) != HAPI_RESULT_SUCCESS) continue;
                            attr->set_long(param_value, static_cast<unsigned int>(si));
                        }
                    }
                    else attr = orig_attr;
                }
            }
            else if (HAPI_ParmInfo_IsFloat(&param_info))
            {
                if (param_info.type == HAPI_PARMTYPE_COLOR) visual_hint = OfAttr::VISUAL_HINT_COLOR;
                else if (param_size == 3) visual_hint = OfAttr::VISUAL_HINT_DISTANCE; // TODO how to differentiate between scale and rotation?
                check_attribute<OfAttr::TYPE_DOUBLE>(context, orig_attr, param_size, visual_hint);

                if (orig_attr == 0)
                { // no need to recreate if already there, though it would retain it's original value
                    attr = create_attribute<OfAttr::TYPE_DOUBLE>(context, param_name, param_size, visual_hint, group_name);

                    for (int si = 0; si < param_size; ++si)
                    {
                        float param_value = 0;
                        if (HAPI_GetParmFloatValues(session, node_id, &param_value, param_info.floatValuesIndex + si, 1) != HAPI_RESULT_SUCCESS) continue;
                        attr->set_double(param_value, static_cast<unsigned int>(si));
                    }
                }
                else attr = orig_attr;
            }
            else if (HAPI_ParmInfo_IsString(&param_info))
            { // an attribute can be a file and a string at the same time
                if (HAPI_ParmInfo_IsFilePath(&param_info) && false)
                {
                    // by default everything is read_write, so we should ask the
                    // asset creators to setup these properly
                    if (param_info.permissions == HAPI_PERMISSIONS_READ_ONLY) visual_hint = OfAttr::VISUAL_HINT_FILENAME_OPEN;
                    else if (param_info.permissions == HAPI_PERMISSIONS_READ_WRITE ||
                             param_info.permissions == HAPI_PERMISSIONS_WRITE_ONLY)
                        visual_hint = OfAttr::VISUAL_HINT_FILENAME_SAVE;
                    check_attribute<OfAttr::TYPE_FILE>(context, orig_attr, param_size, visual_hint);
                    if (orig_attr == 0)
                    {
                        attr = create_attribute<OfAttr::TYPE_FILE>(context, param_name, param_size, visual_hint, group_name);

                        for (int si = 0; si < param_size; ++si)
                        {
                            HAPI_StringHandle string_handle;
                            HAPI_GetParmStringValues(session, node_id, true, &string_handle, param_info.stringValuesIndex + si, 1);
                            attr->set_string(convert_hapi_string(session, string_handle).c_str(), static_cast<unsigned int>(si));
                        }
                    }
                    else attr = orig_attr;

                    CoreVector<CoreString> filename_extensions;
                    CoreString type_info = convert_hapi_string(session, param_info.typeInfoSH).c_str();
                    if (type_info.get_count() > 0)
                    {
                        CoreVector<CoreString> type_info_split;
                        type_info.split(" ", type_info_split);
                        const unsigned int type_info_split_count = type_info_split.get_count();
                        if (type_info_split_count > 0)
                        {

                            filename_extensions.reserve(type_info_split_count);
                            for (unsigned int si = 0; si < type_info_split_count; ++si)
                            {
                                const CoreString ty_info = type_info_split[si];
                                if (ty_info.get_count() < 3)
                                    continue;
                                if (ty_info.sub_string(0, 2) == "*.")
                                    filename_extensions.add(ty_info.sub_string(2, ty_info.get_count() - 2));
                            }
                        }
                    }
                    attr->set_filename_extensions(filename_extensions); // always have to set it to reset the value
                }
                else
                {
                    check_attribute<OfAttr::TYPE_STRING>(context, orig_attr, param_size, visual_hint);
                    if (orig_attr == 0)
                    {
                        attr = create_attribute<OfAttr::TYPE_STRING>(context, param_name, param_size, visual_hint, group_name);

                        for (int si = 0; si < param_size; ++si)
                        {
                            HAPI_StringHandle string_handle;
                            HAPI_GetParmStringValues(session, node_id, true, &string_handle, param_info.stringValuesIndex + si, 1);
                            attr->set_string(convert_hapi_string(session, string_handle).c_str(), static_cast<unsigned int>(si));
                        }
                    }
                    else attr = orig_attr;
                }
            }
            else continue;

            // only works with floating points!
            if (HAPI_ParmInfo_IsFloat(&param_info))
            {
                // probably don't have to worry about strings or vectors
                if (param_info.hasMin || param_info.hasMax)
                {
                    attr_flags |= OfAttr::FLAG_NUMERIC_RANGE;
                    attr->set_numeric_range(param_info.hasMin ? param_info.min : DBL_MIN, param_info.hasMax ? param_info.max : DBL_MAX);
                }

                if (param_info.hasUIMin && param_info.hasUIMax)
                {
                    attr_flags |= OfAttr::FLAG_UI_RANGE | OfAttr::FLAG_SLIDER;
                    attr->set_numeric_ui_range(param_info.UIMin, param_info.UIMax);
                }
            }

            attr->set_flags(attr_flags);
            existing_attributes[param_name.get_data()] = true;
        }
        generating_attributes->set_bool(false);

        for (std::map<std::string, bool>::const_iterator it = existing_attributes.begin(); it != existing_attributes.end(); ++it)
        {
            if (!it->second)
                context.remove_attribute(it->first.c_str());
        }
    }

    void set_parameter(HAPI_Session* session, OfContext& context, HAPI_NodeId node_id, const HAPI_ParmInfo& param_info, const OfAttr* attr)
    {
        const unsigned int param_size = std::min((unsigned int)param_info.size, attr->get_value_count());
        // one by one because that's what we need to
        // convert most of the times
        const OfAttr::Type attr_type = attr->get_type();
        if (attr_type == OfAttr::TYPE_LONG)
        {
            for (unsigned int si = 0; si < param_size; ++si)
            {
                const int param_value = static_cast<int>(attr->get_long(si));
                HAPI_SetParmIntValues(session, node_id, &param_value, param_info.intValuesIndex + si, 1);
            }
        }
        else if (attr_type == OfAttr::TYPE_DOUBLE)
        {
            for (unsigned int si = 0; si < param_size; ++si)
            {
                const float param_value = static_cast<float>(attr->get_double(si));
                HAPI_SetParmFloatValues(session, node_id, &param_value, param_info.floatValuesIndex + si, 1);
            }
        }
        else if (attr_type == OfAttr::TYPE_BOOL)
        {
            const int param_value = attr->get_bool() ? 1 : 0;
            HAPI_SetParmIntValues(session, node_id, &param_value, param_info.intValuesIndex, 1);
        }
        else if (attr_type == OfAttr::TYPE_STRING)
        {
            for (unsigned int si = 0; si < param_size; ++si)
            {
                CoreString param_value = attr->get_string(si);
                HAPI_SetParmStringValue(session, node_id, param_value.get_data(), param_info.id, si);
            }
        }
    }

    bool read_params(HAPI_Session* session, OfContext& context, const OfAttr* changed_attr = 0)
    {
        HAPI_AssetId asset_id = static_cast<HAPI_AssetId>(context.get_attribute(BUILTIN_LOADED_ASSET_ID)->get_long());
        if (asset_id == LONG_MIN)
            return false;
        HAPI_AssetInfo asset_info  = HAPI_AssetInfo_Create();
        HAPI_GetAssetInfo(session, asset_id, &asset_info);
        const HAPI_NodeId& node_id = asset_info.nodeId;
        // setup time parameters
        OfApp& app = context.get_application();
        HAPI_TimelineOptions timeline_options = HAPI_TimelineOptions_Create();
        HAPI_TimelineOptions_Init(&timeline_options);
        OfTime& time = app.get_factory().get_time();
        timeline_options.fps = static_cast<float>(time.get_fps());
        HAPI_SetTimelineOptions(session, &timeline_options);
        HAPI_SetTime(session, static_cast<float>(time.get_current_frame()));

        if (changed_attr)
        {
            HAPI_ParmInfo param_info;
            if (HAPI_GetParmInfoFromName(session, node_id, changed_attr->get_name().get_data(), &param_info) == HAPI_RESULT_SUCCESS)
            {
                set_parameter(session, context, node_id, param_info, changed_attr);
                return true;
            }
            else return false;
        }
        else
        {
            HAPI_NodeInfo node_info;
            if (HAPI_GetNodeInfo(session, node_id, &node_info) != HAPI_RESULT_SUCCESS) return false;
            if (node_info.parmCount == 0) return false;

            std::vector<HAPI_ParmInfo> param_infos(static_cast<unsigned int>(node_info.parmCount));
            if (HAPI_GetParameters(session, node_info.id, &param_infos[0], 0, node_info.parmCount) != HAPI_RESULT_SUCCESS) return false;
            for (int i = 0; i < node_info.parmCount; ++i)
            {
                const HAPI_ParmInfo& param_info = param_infos[i];
                const CoreString param_name = convert_hapi_string(session, param_info.nameSH).c_str();

                if (!context.attribute_exists(param_name)) continue;
                const OfAttr* attr = context.get_attribute(param_name);

                set_parameter(session, context, node_id, param_info, attr);
            }
            return true;
        }
    }

    bool init_HAPI(HAPI_Session* session, HAPI_CookOptions& cook_options)
    {
        HAPI_Result result = HAPI_IsInitialized(session);
        if (result == HAPI_RESULT_NOT_INITIALIZED)
        {
            result = HAPI_Initialize(session, 0, 0, &cook_options, false, 0);
            if (result != HAPI_RESULT_SUCCESS)
            {
                std::cout << "[hapi] Failed to initialize HAPI : " << result << std::endl;
                return false;
            }
            else
                return true;
        }
        else if (result != HAPI_RESULT_SUCCESS)
        {
            std::cout << "[hapi] Return value from HAPI_IsInitialized : " << result << std::endl;
            return false;
        }
        else
            return true;
    }

    std::map<std::string, HAPI_AssetLibraryId> g_asset_library_ids;

    HAPI_AssetLibraryId load_asset_library(HAPI_Session* session, const std::string& filename)
    {
        HAPI_AssetLibraryId library_id = -1;

        // probably you can't load asset libs on multiple threads
        //ScopedLock scoped_lock(g_asset_library_mutex);
        std::map<std::string, HAPI_AssetLibraryId>::iterator it = g_asset_library_ids.find(filename);
        if (it == g_asset_library_ids.end())
        {
            HAPI_Result result = HAPI_RESULT_SUCCESS;
            for (tbb::tick_count retry_tc = tbb::tick_count::now(); (tbb::tick_count::now() - retry_tc).seconds() < g_license_timeout;)
            {
                result = HAPI_LoadAssetLibraryFromFile(session, filename.c_str(), true, &library_id);
                if (result == HAPI_RESULT_NO_LICENSE_FOUND)
                {
                    library_id = -2;
                    sleep(g_license_retry_wait);
                    continue;
                }
                else if (result != HAPI_RESULT_SUCCESS)
                {
                    std::cout << "[hapi] Error loading library from file : " << result << std::endl;
                    library_id = -1; // just be sure
                }
                break;
            }
            g_asset_library_ids.insert(std::pair<std::string, HAPI_AssetLibraryId>(filename, library_id));
        }
        else
            library_id = it->second;

        return library_id;
    }

    void clear_asset_library(const std::string& filename)
    {
        //ScopedLock scoped_lock(g_asset_library_mutex);
        g_asset_library_ids.erase(filename);
    }

    OfObject* create_object(OfContext& context, const CoreString& object_name, const CoreString& object_type, bool check_existing)
    {
        if (check_existing && context.object_exists(object_name))
        {
            OfObject* object = context.get_object(object_name);
            if (object->get_class().get_name() != object_type)
            {
                context.remove_object(object_name);
                object = context.add_object(object_name, object_type);
            }
            return object;
        }
        else
            return context.add_object(object_name, object_type);
    }

    void cook_asset_objects(HAPI_Session* session, const CoreString& filename, OfContext& context, CoreVector<OfObject* >* loaded_objects = 0)
    {
        HAPI_AssetId asset_id = static_cast<int>(context.get_attribute(BUILTIN_LOADED_ASSET_ID)->get_long());
        if (asset_id == LONG_MIN) return;
        HAPI_CookOptions cook_options = HAPI_CookOptions_Create();
        cook_options.maxVerticesPerPrimitive = -1;
        cook_options.refineCurveToLinear = true;
        //cook_options.packedPrimInstancingMode = HAPI_PACKEDPRIM_INSTANCING_MODE_DISABLED;
        HAPI_Result result = HAPI_RESULT_SUCCESS;
        for (tbb::tick_count retry_tc = tbb::tick_count::now(); (tbb::tick_count::now() - retry_tc).seconds() < g_license_timeout;)
        {
            tbb::tick_count tc = tbb::tick_count::now();
            result = HAPI_CookAsset(session, asset_id, &cook_options);
            if (result == HAPI_RESULT_NO_LICENSE_FOUND)
            {
                sleep(g_license_retry_wait);
                continue;
            }
            else if (result != HAPI_RESULT_SUCCESS)
            {
                std::cout << "[hapi] Error cooking assets : " << result << std::endl;
                return;
            }
            else
            {
                std::cout << "[hapi] Cooking assets took " << (tbb::tick_count::now() - tc).seconds() << " seconds" << std::endl;
                break;
            }
        }
        if (result == HAPI_RESULT_NO_LICENSE_FOUND)
        {
            std::cout << "[hapi] No license available." << std::endl;
            context.get_application().quit();
            return;
        }

        HAPI_AssetInfo asset_info  = HAPI_AssetInfo_Create();

        HAPI_GetAssetInfo(session, asset_id, &asset_info);
        if (asset_info.objectCount == 0) return;

        std::vector<HAPI_Transform> transforms(static_cast<unsigned int>(asset_info.objectCount));
        std::vector<HAPI_ObjectInfo> objects(static_cast<unsigned int>(asset_info.objectCount));

        result = HAPI_GetObjectTransforms(session, asset_id, HAPI_RST, &transforms[0], 0, asset_info.objectCount);
        if (result != HAPI_RESULT_SUCCESS)
        {
            std::cout << "[hapi] Error accessing object transforms.\n";
            context.remove_all_objects();
            return;
        }

        result = HAPI_GetObjects(session, asset_id, &objects[0], 0, asset_info.objectCount);
        if (result != HAPI_RESULT_SUCCESS)
        {
            std::cout << "[hapi] Error accessing objects.\n";
            context.remove_all_objects();
            return;
        }

        std::map<std::string, bool> existing_objects_changed;
        // can this change when we just re-cook the asset?
        if (!loaded_objects) // no need to remove them when reloading the whole asset
        {
            CoreArray<OfObject*> existing_objects;
            context.get_objects(existing_objects);
            const unsigned int existing_object_count = existing_objects.get_count();
            for (unsigned int i = 0; i < existing_object_count; ++i)
                existing_objects_changed.insert(std::pair<std::string, bool>(existing_objects[i]->get_name().get_data(), false));
        }

        for (int obj_counter = 0; obj_counter < asset_info.objectCount; ++obj_counter)
        {
            HAPI_ObjectInfo object_info = objects[obj_counter];
            if (!object_info.isVisible)
                continue;
            if (object_info.isInstancer)
                continue;
            const bool transform_changed = object_info.hasTransformChanged || true;
            const bool object_changed = object_info.haveGeosChanged || true;

            const CoreString object_name = convert_hapi_string(session, object_info.nameSH).c_str();
            HAPI_Transform transform = transforms[obj_counter];
            GMathTransform g_transform = convert_hapi_transform(session, &transform);

            for (int geo_counter = 0; geo_counter < object_info.geoCount; ++geo_counter)
            {
                HAPI_GeoInfo geo_info;
                if (HAPI_GetGeoInfo(session, asset_id, object_info.id, geo_counter, &geo_info) != HAPI_RESULT_SUCCESS) continue;
                    // HAPI_GeoInfo_Init(&geo_info);
                // it seems when the call failed, there still could be a valid
                // part inside the geoinfo
                if (geo_info.type == HAPI_GEOTYPE_INVALID ||
                    geo_info.type == HAPI_GEOTYPE_INPUT ||
                    geo_info.type == HAPI_GEOTYPE_INTERMEDIATE)
                    continue;
                if (!geo_info.isDisplayGeo || geo_info.isTemplated)
                    continue;
                const CoreString geo_name = convert_hapi_string(session, geo_info.nameSH).c_str();
                for (int part_counter = 0; part_counter < geo_info.partCount; ++part_counter)
                {
                    HAPI_PartInfo part_info;
                    if (HAPI_GetPartInfo(session, asset_id, object_info.id, geo_info.id, part_counter, &part_info) != HAPI_RESULT_SUCCESS) continue;
                    // if is instanced then hide then instance at the right place
                    if (g_is_interactive)
                    {
                        const CoreString part_name = object_name + "__" + geo_name + "__" + CoreString(convert_hapi_string(session, part_info.nameSH).c_str());
                        // TODO : Add volumes
                        // TODO : check validity before creating the clarisse object
                        // so we won't create empty ones
                        OfObject* object = 0;
                        HapiObjectData* object_data = 0;
                        if (part_info.type == HAPI_PARTTYPE_CURVE)
                        {
                            HAPI_CurveInfo curve_info;
                            if (HAPI_GetCurveInfo(session, asset_id, object_info.id, geo_info.id, part_counter, &curve_info) != HAPI_RESULT_SUCCESS) continue;
                            if (curve_info.curveCount < 1) continue;

                            object = create_object(context, part_name, "HapiCurve", !loaded_objects);

                            object_data = reinterpret_cast<HapiObjectData*>(object->get_module_data());
                            object_data->curve_info = curve_info;
                        }
                        else if (part_info.type == HAPI_PARTTYPE_MESH)
                        {
                            if ((part_info.vertexCount == 0) || (part_info.faceCount == 0) || (part_info.pointAttributeCount == 0) ||
                                 (part_info.pointCount == 0) || (part_info.instancedPartCount > 0)|| (part_info.instanceCount > 0)) continue;

                            HAPI_AttributeInfo P_attrib_info;
                            P_attrib_info.exists = false;
                            // should we really checking for this?
                            // the previous quick check probably takes care of this
                            if (HAPI_GetAttributeInfo(session, asset_id, object_info.id, geo_info.id, part_counter, "P", HAPI_ATTROWNER_POINT , &P_attrib_info) != HAPI_RESULT_SUCCESS) continue;
                            if ((P_attrib_info.count == 0) || (P_attrib_info.tupleSize != 3) || !P_attrib_info.exists) continue;

                            object = create_object(context, part_name, "HapiMesh", !loaded_objects);

                            object_data = reinterpret_cast<HapiObjectData*>(object->get_module_data());
                            object_data->part_info = part_info;
                        }
                        else
                            continue;

                        existing_objects_changed[part_name.get_data()] = true;

                        object_data->session = session;
                        object_data->asset_id = asset_id;
                        object_data->object_id = object_info.id;
                        object_data->geo_id = geo_info.id;
                        object_data->part_id = part_counter;

                        object->get_attribute("file_path")->set_string(filename);
                        object->get_attribute("shape_name")->set_string(part_name);

                        if (loaded_objects)
                            loaded_objects->add(object);

                        if (transform_changed)
                        {
                            ModuleGeometry* module = reinterpret_cast<ModulePolymesh*>(object->get_module());
                            module->set_transform(g_transform);
                        }

                        if (object_changed)
                        {
                            // we want the change to be triggered here
                            OfAttr* rebuild_trigger = object->get_attribute("rebuild_trigger");
                            rebuild_trigger->set_long(rebuild_trigger->get_long() + 1);
                        }


                        /*if (part_info.isInstanced)
                            object->get_attribute("display_visible")->set_bool(false);
                        else
                            object->get_attribute("display_visible")->set_bool(true);*/
                    }
                    else
                    {
                        if (part_info.type == HAPI_PARTTYPE_CURVE)
                        {
                            HAPI_CurveInfo curve_info;
                            if (HAPI_GetCurveInfo(session, asset_id, object_info.id, geo_info.id, part_counter, &curve_info) != HAPI_RESULT_SUCCESS) continue;
                            if (curve_info.curveCount < 1) continue;
                            const CoreString part_name = object_name + "__" + geo_name + "__" + CoreString(convert_hapi_string(session, part_info.nameSH).c_str());
                            existing_objects_changed[part_name.get_data()] = true;

                            OfObject* curves_object = create_object(context, part_name, "HapiCurveFromMemory", !loaded_objects);

                            if (loaded_objects)
                                loaded_objects->add(curves_object);

                            ModuleGeometry* curve_module = reinterpret_cast<ModuleGeometry*>(curves_object->get_module());
                            CurveFromMemoryData* curve_data = reinterpret_cast<CurveFromMemoryData*>(curves_object->get_module_data());
                            curve_data->clear();

                            HAPI_AttributeInfo attr_info_p;
                            if (HAPI_GetAttributeInfo(session, asset_id, object_info.id, geo_info.id, part_counter, "P", HAPI_ATTROWNER_POINT, &attr_info_p) != HAPI_RESULT_SUCCESS) continue;
                            if ((attr_info_p.count == 0) || (attr_info_p.tupleSize != 3)) continue;
                            bool radii_valid = false;
                            HAPI_AttributeInfo attr_info_pw;
                            if (HAPI_GetAttributeInfo(session, asset_id, object_info.id, geo_info.id, part_counter, "Pw", HAPI_ATTROWNER_POINT, &attr_info_pw) != HAPI_RESULT_SUCCESS) radii_valid = false;
                            else if ((attr_info_pw.count == 0) || (attr_info_pw.tupleSize != 1) || (attr_info_pw.count != attr_info_p.count)) radii_valid = false;

                            curve_data->vertices.resize(static_cast<unsigned int>(attr_info_p.count));
                            HAPI_GetAttributeFloatData(session, asset_id, object_info.id, geo_info.id, part_counter, "P", &attr_info_p, &curve_data->vertices[0][0], 0, attr_info_p.count);

                            if (radii_valid)
                            {
                                curve_data->radii.resize(static_cast<unsigned int>(attr_info_pw.count));
                                HAPI_GetAttributeFloatData(session, asset_id, object_info.id, geo_info.id, part_counter, "Pw", &attr_info_pw, &curve_data->radii[0], 0, attr_info_pw.count);
                            }

                            const int curve_count = curve_info.curveCount;
                            std::vector<int> vertex_counts(static_cast<unsigned int>(curve_count));
                            HAPI_GetCurveCounts(session, asset_id, object_info.id, geo_info.id, part_counter, &vertex_counts[0], 0, curve_count);
                            curve_data->vertex_count.resize(static_cast<unsigned int>(curve_count));
                            for (int i = 0; i < curve_count; ++i)
                                curve_data->vertex_count[i] = static_cast<unsigned int>(vertex_counts[i]);

                            curve_module->set_transform(g_transform);

                            curves_object->get_attribute("file_path")->set_string(filename);
                            curves_object->get_attribute("shape_name")->set_string(part_name);
                            OfAttr* rebuild_trigger = curves_object->get_attribute("rebuild_trigger");
                            rebuild_trigger->set_long(rebuild_trigger->get_long() + 1);
                        }
                        else if (part_info.type == HAPI_PARTTYPE_MESH)
                        {
                            if ((part_info.vertexCount == 0) || (part_info.faceCount == 0) || (part_info.pointAttributeCount == 0) ||
                                (part_info.pointCount == 0) || (part_info.instancedPartCount > 0)|| (part_info.instanceCount > 0)) continue;

                            HAPI_AttributeInfo P_attrib_info;
                            P_attrib_info.exists = false;
                            if (HAPI_GetAttributeInfo(session, asset_id, object_info.id, geo_info.id, part_counter, "P", HAPI_ATTROWNER_POINT , &P_attrib_info) != HAPI_RESULT_SUCCESS) continue;
                            if ((P_attrib_info.count == 0) || (P_attrib_info.tupleSize != 3) || !P_attrib_info.exists)
                            {
                                std::cout << "[hapi] Point attribute not in the right format : " << P_attrib_info.count << " : " << P_attrib_info.tupleSize << std::endl;
                                continue;
                            }

                            const CoreString part_name = object_name + "__" + geo_name + "__" + CoreString(convert_hapi_string(session, part_info.nameSH).c_str());
                            OfObject* polymesh_object = create_object(context, part_name, "HapiMeshFromMemory", !loaded_objects);

                            ModulePolymesh* polymesh_module = reinterpret_cast<ModulePolymesh*>(polymesh_object->get_module());
                            MeshFromMemoryData* mesh_data = reinterpret_cast<MeshFromMemoryData*>(polymesh_object->get_module_data());
                            mesh_data->clear();

                            if (!export_geometry(session, asset_id, object_info.id, geo_info.id, part_counter, part_info,
                                            mesh_data->vertices, mesh_data->polygon_vertex_count, mesh_data->polygon_vertex_indices))
                            {
                                std::cout << "[hapi] Failed exporting geometry.\n";
                                continue;                                
                            }

                            if (loaded_objects)
                                loaded_objects->add(polymesh_object);
                            existing_objects_changed[part_name.get_data()] = true;

                            export_normals(session, asset_id, object_info.id, geo_info.id, part_counter, part_info, mesh_data->polygon_vertex_indices, mesh_data->normal_maps);
                            export_uvs(session, asset_id, object_info.id, geo_info.id, part_counter, part_info, mesh_data->polygon_vertex_indices, mesh_data->uv_maps);
                            export_materials(session, asset_id, object_info.id, geo_info.id, part_counter, part_info, 0, mesh_data->shading_groups, mesh_data->shading_group_indices);

                            polymesh_module->set_transform(g_transform);

                            polymesh_object->get_attribute("file_path")->set_string(filename);
                            polymesh_object->get_attribute("shape_name")->set_string(part_name);
                            OfAttr* rebuild_trigger = polymesh_object->get_attribute("rebuild_trigger");
                            rebuild_trigger->set_long(rebuild_trigger->get_long() + 1);
                        }
                    }
                }
            }
        }

        for (std::map<std::string, bool>::iterator it = existing_objects_changed.begin(); it != existing_objects_changed.end(); ++it)
        {
            if (!it->second)
                context.remove_object(it->first.c_str());
        }
    }

    void load_asset_objects(HAPI_Session* session, const CoreString& filename, OfContext& context, CoreVector<OfObject *>* loaded_objects = 0)
    {
        if (loaded_objects)
            clear_asset_library(filename.get_data());
        // hapi will reload the file and create a new library id

        HAPI_Result result;
        HAPI_CookOptions cook_options = HAPI_CookOptions_Create();
        HAPI_CookOptions_Init(&cook_options);
        cook_options.splitGeosByGroup = false;
        cook_options.refineCurveToLinear = true;

        if (!init_HAPI(session, cook_options))
        {
            std::cout << "[hapi] Can't initialize HAPI.\n";
            context.remove_all_objects();
            return;
        }

        HAPI_AssetId previous_asset_id = static_cast<HAPI_AssetId>(context.get_attribute(BUILTIN_LOADED_ASSET_ID)->get_long());
        HAPI_AssetId asset_id;
        HAPI_AssetInfo asset_info;

        if (loaded_objects || (previous_asset_id == LONG_MIN))
        {
            if (previous_asset_id != LONG_MIN)
            {
                context.get_attribute(BUILTIN_LOADED_ASSET_ID)->set_long(LONG_MIN);
                HAPI_DestroyAsset(session, previous_asset_id);
            }
            HAPI_AssetLibraryId library_id = load_asset_library(session, filename.get_data());
            if (library_id == -1)
            {
                std::cout << "[hapi] Library cannot be loaded.\n";
                context.remove_all_objects();
                return;
            }
            else if (library_id == -2)
            {
                std::cout << "[hapi] No license available." << std::endl;
                context.get_application().quit();
                context.remove_all_objects();
                return;
            }

            int asset_count = 0;
            result = HAPI_GetAvailableAssetCount(session, library_id, &asset_count);
            if (result != HAPI_RESULT_SUCCESS || asset_count == 0)
            {
                std::cout << "[hapi] There are no assets in the library.\n";
                context.remove_all_objects();
                return;
            }

            std::vector<HAPI_StringHandle> asset_names_handle(static_cast<unsigned int>(asset_count));
            std::vector<std::string> asset_names;
            asset_names.reserve(static_cast<unsigned int>(asset_count));
            HAPI_GetAvailableAssets(session, library_id, &asset_names_handle[0], asset_count);

            for (std::vector<HAPI_StringHandle>::iterator it = asset_names_handle.begin(); it != asset_names_handle.end(); ++it)
            {
                std::string asset_name(convert_hapi_string(session, *it));
                asset_names.push_back(asset_name);
            }
            result = HAPI_InstantiateAsset(session, asset_names[0].c_str(), false, &asset_id);
            if (result != HAPI_RESULT_SUCCESS)
            {
                context.remove_all_objects();
                std::cout << "[hapi] Error instantiating asset : " << result << std::endl;
                return;
            }
            context.get_attribute(BUILTIN_LOADED_ASSET_ID)->set_long(asset_id);

            HAPI_GetAssetInfo(session, asset_id, &asset_info);
            // if loaded_objects is zero
            // then we are just changing attributes
            if (loaded_objects)
                generate_params(session, context, asset_info.nodeId);
        }
        else
        {
            asset_id = previous_asset_id;
            HAPI_GetAssetInfo(session, asset_id, &asset_info);
        }

        if (read_params(session, context))
            cook_asset_objects(session, filename, context, loaded_objects);
    }
}

class HapiContextEngine : public OfReferenceContextEngine {
public:
    static const Descriptor& get_descriptor()
    {
        static Descriptor desc = {
                &HapiContextEngine::class_info(),
                "hapi",
                HapiContextEngine::create_engine,
                OfReferenceContextEngine::parse_serial
        };
        return desc;
    }

    static void load_asset_objects(const CoreString& filename, OfContext& context, CoreVector<OfObject *>& loaded_objects, AppProgressBar *progress_bar)
    {
        // these are always called before any
        // on options change calls, so we
        // don't need to init anything there
        if (g_is_interactive)
        {
            init_inprocess_session();
            if (s_session.id != -1)
            {
                ::load_asset_objects(&s_session, filename, context, &loaded_objects);
                if (context.get_attribute_count() == NUMBER_OF_BUILTIN_PARAMETERS) // TODO : uaegh, super-ugly
                    reinterpret_cast<HapiContextEngine*>(&context)->m_all_params_read_after_load = true;
            }
        }
        else
        {
            init_outofprocess_session();
            if (s_thrift_handle == 0)
                return;
            else
            {
                ::load_asset_objects(&s_session, filename, context, &loaded_objects);

                // only close the session here if there are no attributes to read from
                // somehow we should detect if all the other contextes are already initialized
                if (context.get_attribute_count() == NUMBER_OF_BUILTIN_PARAMETERS)
                {
                    std::cout << "[hapi] Closing thrift server as the context has no extra attributes." << std::endl;
                    close_thrift_server();
                }
            }
            start_delayed_close();
        }
    }

    static void init_inprocess_session()
    {
        if (s_session.id == -1)
        {
            if (HAPI_CreateInProcessSession(&s_session) != HAPI_RESULT_SUCCESS)
            {
                s_session.type = HAPI_SESSION_MAX;
                s_session.id = -1;
                std::cout << "[hapi] Error initing in-process hapi session." << std::endl;
            }
        }
    }

    static void init_outofprocess_session()
    {
        if (s_thrift_handle == 0)
        {
            std::stringstream ss;
            ss << time(0);
            s_thrift_handle = popen((std::string("hapi_server ") + ss.str()).c_str(), "w");
            if (s_thrift_handle > 0)
            {
                const double RETRY_TIMEOUT = 100;
                const int SECS_BETWEEN_TRIES = 1;
                for (tbb::tick_count retry_tc = tbb::tick_count::now(); (tbb::tick_count::now() - retry_tc).seconds() < RETRY_TIMEOUT;)
                {
                    if (HAPI_CreateThriftNamedPipeSession(&s_session, (std::string("clarisse_pipe_thrift_") + ss.str()).c_str()) == HAPI_RESULT_SUCCESS)
                    {
                        std::cout << "[hapi] Successfully connected to the thrift server via a named pipe." << std::endl;
                        return;
                    }
                    sleep(SECS_BETWEEN_TRIES);
                }
            }
            else
                std::cout << "Error opening hapi server" << std::endl;
            std::cout << "[hapi] Error connecting to the thrift named pipe server, launching an in process session." << std::endl;
            init_inprocess_session();
        }
        else
            stop_delayed_close();
    }    

    static void start_delayed_close()
    {
        stop_delayed_close();
        s_delayed_release_active = true;
        s_delayed_release = new tbb::tbb_thread(delayed_thrift_close);
    }

    static void stop_delayed_close()
    {        
        if (s_delayed_release)
        {
            s_delayed_release_active = false;
            s_delayed_release->join();
            delete s_delayed_release;
            s_delayed_release = 0;
        }
    }

    static void setup_initial_params()
    {
        s_thrift_handle = 0;
        s_delayed_release_active = true;
    }
protected:
    static void delayed_thrift_close()
    {
        tbb::tick_count tc = tbb::tick_count::now();
        while (s_delayed_release_active)
        {
            if ((tbb::tick_count::now() - tc).seconds() < g_license_release)
                sleep(0);
            else
            {
                close_thrift_server();
                return;
            }
        }
    }

    static void close_thrift_server()
    {
        FILE* handle = s_thrift_handle.fetch_and_store(0);
        if (handle != 0)
        {
            const char* message = "close";
            fwrite(message, sizeof(char), strlen(message), handle);
            pclose(handle);
        }
    }

    virtual void populate_options()
    {
        OfReferenceContextEngine::populate_options(); // Mandatory call as OfReferenceContextEngine attributes must be created.
        OfContext& context = get_context();

        // don't save these attrs to the file
        OfAttr* attr = context.add_attribute("__generating_attributes__", OfAttr::TYPE_BOOL, OfAttr::CONTAINER_SINGLE);
        attr->set_hidden(true);
        // This is required because we fill attributes during loading asset objects,
        // so we can free the houdini "universe" after finished reading the data
        attr->set_bool(false);
        attr->set_flags(attr->get_flags() & (~OfAttr::FLAG_SAVEABLE));
        // we only remove attributes when the filename has changed
        // from something to something, not from empty to empty
        attr = context.add_attribute("__last_loaded_file__", OfAttr::TYPE_STRING, OfAttr::CONTAINER_SINGLE);
        attr->set_hidden(true);
        attr->set_flags(attr->get_flags() & (~OfAttr::FLAG_SAVEABLE));

        attr = context.add_attribute("rebuild_on_parameter_change", OfAttr::TYPE_BOOL, OfAttr::CONTAINER_SINGLE, OfAttr::VISUAL_HINT_DEFAULT, "file");
        attr->set_bool(true);

        attr = context.add_attribute("__loaded_asset_id__", OfAttr::TYPE_LONG, OfAttr::CONTAINER_SINGLE);
        attr->set_hidden(true);
        attr->set_flags(attr->get_flags() & (~OfAttr::FLAG_SAVEABLE));
        attr->set_long(LONG_MIN);
    }

    // This method is called when an option attribute has changed.
    virtual void on_options_changed(OfObject& context_options)
    {
        OfReferenceContextEngine::on_options_changed(context_options);

        const OfAttr* attr = context_options.get_changing_attr();

        if (attr == 0) return;
        const CoreString attr_name = attr->get_name();
        if (attr_name == "__generating_attributes__") return;
        if (attr_name == "__loaded_asset_id__") return;
        if (attr_name == "rebuild_on_parameter_change") return;
        if (context_options.get_attribute(BUILTIN_GENERATING_ATTRIBUTES)->get_bool()) return;
        if (attr_name == "filename")
        {
            context_options.get_attribute(BUILTIN_GENERATING_ATTRIBUTES)->set_bool(true); // do I need this?
            OfAttr* last_loaded_file_attr = context_options.get_attribute("__last_loaded_file__");
            const CoreString last_loaded_file = last_loaded_file_attr->get_string();
            const CoreString current_file = attr->get_string();
            last_loaded_file_attr->set_string(current_file);
            if (last_loaded_file != current_file)
            {
                context_options.get_attribute(BUILTIN_GENERATING_ATTRIBUTES)->set_bool(false); // do I need this?
                return;
            }

            // the attributes are stored in the order of creation
            const unsigned int attribute_count = context_options.get_attribute_count();
            for (unsigned int i = attribute_count - 1; i >= NUMBER_OF_BUILTIN_PARAMETERS; --i)
                context_options.remove_attribute(context_options.get_attribute(i)->get_name());
            context_options.get_attribute(BUILTIN_GENERATING_ATTRIBUTES)->set_bool(false); // do I need this?
        }
        else if (g_is_interactive)
        {
            init_inprocess_session();
            if (s_session.id != -1)
            {
                //if (m_all_params_read_after_load)
                {
                    if (context_options.get_attribute(BUILTIN_REBUILD_ON_PARAMETER_CHANGE)->get_bool())
                    {
                        if (read_params(&s_session, get_context(), attr))
                            cook_asset_objects(&s_session, context_options.get_attribute(BUILTIN_FILENAME)->get_string(), get_context(), 0);
                    }
                }
                /*else if (context_options.get_attribute(context_options.get_attribute_count() - 1) == attr)
                {
                    if (read_params(&s_session, get_context(), attr))
                        cook_asset_objects(&s_session, context_options.get_attribute(BUILTIN_FILENAME)->get_string(), get_context(), 0);
                    m_all_params_read_after_load = true;
                }*/
            }
        }
        else
        {
            init_outofprocess_session();
            // no need to init out of process
            // session here, because this is going to run after
            // the context loaded through load_asset_objects
            // but we need to close it down if this is the last context
            //if (m_all_params_read_after_load)
            {
                if (read_params(&s_session, get_context(), attr))
                    cook_asset_objects(&s_session, context_options.get_attribute(BUILTIN_FILENAME)->get_string(), get_context(), 0);
            }
            /*else if (context_options.get_attribute(context_options.get_attribute_count() - 1) == attr)
            {
                if (read_params(&s_session, get_context(), attr))
                    cook_asset_objects(&s_session, context_options.get_attribute(BUILTIN_FILENAME)->get_string(), get_context(), 0);
                m_all_params_read_after_load = true;
            }*/
            start_delayed_close();
        }
    }
private:
    static OfContextEngine *create_engine(OfContext& ctx)
    {
        return new HapiContextEngine(ctx);
    }

    HapiContextEngine(OfContext& ctx) : OfReferenceContextEngine(ctx), m_all_params_read_after_load(false)
    { }

    static HAPI_Session s_session;
    static tbb::atomic<FILE*> s_thrift_handle;
    static tbb::atomic<bool> s_delayed_release_active;
    static tbb::tbb_thread* s_delayed_release;

    bool m_all_params_read_after_load;

    DECLARE_CLASS;
};

IMPLEMENT_CLASS(HapiContextEngine, OfReferenceContextEngine);

HAPI_Session HapiContextEngine::s_session = {HAPI_SESSION_MAX, -1};
tbb::tbb_thread* HapiContextEngine::s_delayed_release = 0;
tbb::atomic<FILE*> HapiContextEngine::s_thrift_handle;
tbb::atomic<bool> HapiContextEngine::s_delayed_release_active;

void hapi_on_register_module(OfApp& app, CoreVector<OfClass *>& new_classes)
{
    OfContext::register_engine(HapiContextEngine::class_info(), HapiContextEngine::get_descriptor);

    OfReferenceContextEngine::add_file_format(HapiContextEngine::class_info(), "otl", &HapiContextEngine::load_asset_objects);
    OfReferenceContextEngine::add_file_format(HapiContextEngine::class_info(), "hda", &HapiContextEngine::load_asset_objects);

    g_is_interactive = app.get_type() != AppBase::TYPE_PROCESS; // && true;

    HapiContextEngine::setup_initial_params();
}
