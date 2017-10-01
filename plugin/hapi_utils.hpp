#pragma once

#include <HAPI/HAPI.h>
#include <vector>
#include <string>
#include <core_string.h>
#include <pthread.h>
#include <map>
#include <iostream>

inline std::string convert_hapi_string(const HAPI_Session* session, HAPI_StringHandle string_handle)
{
    int buffer_length = 0;
    HAPI_GetStringBufLength(session, string_handle, &buffer_length);
    if (buffer_length <= 0) { return std::string(""); }
    std::vector<char> string_data(static_cast<unsigned int>(buffer_length));
    if (HAPI_GetString(session, string_handle, &string_data[0], buffer_length) != HAPI_RESULT_SUCCESS) {
        return std::string("");
    } else { return std::string(&string_data[0]); }
}

inline CoreString convert_hapi_corestring(const HAPI_Session* session, HAPI_StringHandle string_handle)
{
    int buffer_length = 0;
    HAPI_GetStringBufLength(session, string_handle, &buffer_length);
    if (buffer_length <= 0) { return ""; }
    std::vector<char> string_data(static_cast<unsigned int>(buffer_length));
    if (HAPI_GetString(session, string_handle, &string_data[0], buffer_length) != HAPI_RESULT_SUCCESS) {
        return "";
    } else { return CoreString(&string_data[0]); }
}
/*
inline CoreString get_material_name(const HAPI_Session* session, HAPI_AssetId asset, HAPI_MaterialId material)
{
    HAPI_MaterialInfo material_info;
    material_info.exists = false;
    if (HAPI_GetMaterialInfo(session, asset, material, &material_info) == HAPI_RESULT_SUCCESS)
    {
        if (material_info.exists)
        {
            HAPI_NodeInfo node_info;
            if (HAPI_GetNodeInfo(session, material_info.nodeId, &node_info) == HAPI_RESULT_SUCCESS)
                return convert_hapi_corestring(session, node_info.nameSH);
            else return "default_no_node_info";
        }
        else return "default_material_doesnt_exists";
    }
    else return "default_no_material_info";
}

inline bool export_geometry(const HAPI_Session* session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_PartInfo& part_info,
        CoreArray<GMathVec3f>& vertices, CoreArray<unsigned int>& polygon_vertex_count, CoreArray<unsigned int>& polygon_vertex_indices)
{
    HAPI_AttributeInfo P_attrib_info;
    P_attrib_info.exists = false;
    if (HAPI_GetAttributeInfo(session, asset_id, object_id, geo_id, part_id, "P", HAPI_ATTROWNER_POINT , &P_attrib_info) != HAPI_RESULT_SUCCESS) return 0;
    if ((P_attrib_info.count == 0) || (P_attrib_info.tupleSize != 3) || !P_attrib_info.exists)
    {
        std::cout << "[hapi_mesh] Point attribute not in the right format : " << P_attrib_info.count << " : " << P_attrib_info.tupleSize << std::endl;
        return false;
    }

    vertices.resize(P_attrib_info.count);
    HAPI_Result result = HAPI_GetAttributeFloatData(session, asset_id, object_id, geo_id, part_id, "P", &P_attrib_info, &vertices[0][0], 0, P_attrib_info.count);
    if (result != HAPI_RESULT_SUCCESS)
    {
        std::cout << "[hapi_mesh] Cannot query P attribute : " << result << std::endl;
        return false;
    }

    // HAPI writes some of the data into a different format,
    // ie array int instead of unsigned int
    CoreArray<int> face_counts(part_info.faceCount);
    if (HAPI_GetFaceCounts(session, asset_id, object_id, geo_id, part_id, &face_counts[0], 0, part_info.faceCount) != HAPI_RESULT_SUCCESS)
    {
        std::cout << "[hapi_mesh] Error accessing face counts" << std::endl;
        return false;
    }

    CoreArray<int> vertex_list(part_info.vertexCount); // equal to vidxs in Arnold
    result = HAPI_GetVertexList(session, asset_id, object_id, geo_id, part_id, &vertex_list[0], 0, part_info.vertexCount);
    if (result != HAPI_RESULT_SUCCESS)
    {
        std::cout << "[hapi_mesh] Cannot query vertex list : " << result << std::endl;
        return false;
    }

    polygon_vertex_count.resize(part_info.faceCount);
    for (int i = 0; i < part_info.faceCount; ++i)
        polygon_vertex_count[i] = face_counts[i];

    polygon_vertex_indices.resize(part_info.vertexCount);
    for (int i = 0; i < part_info.vertexCount; ++i)
        polygon_vertex_indices[i] = vertex_list[i];

    return true;
}

inline void export_normals(const HAPI_Session* session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_PartInfo& part_info,
        CoreArray<unsigned int>& polygon_vertex_indices, CoreArray<GeometryNormalMap>& normal_maps)
{
    HAPI_AttributeInfo N_attrib_info;
    N_attrib_info.exists = false;
    HAPI_AttributeOwner N_owner = HAPI_ATTROWNER_MAX;
    if ((HAPI_GetAttributeInfo(session, asset_id, object_id, geo_id, part_id, HAPI_ATTRIB_NORMAL, HAPI_ATTROWNER_POINT, &N_attrib_info) == HAPI_RESULT_SUCCESS)
         && N_attrib_info.exists)
        N_owner = HAPI_ATTROWNER_POINT;
    else if ((HAPI_GetAttributeInfo(session, asset_id, object_id, geo_id, part_id, HAPI_ATTRIB_NORMAL, HAPI_ATTROWNER_VERTEX, &N_attrib_info) == HAPI_RESULT_SUCCESS)
             && N_attrib_info.exists)
        N_owner = HAPI_ATTROWNER_VERTEX;

    if ((N_owner != HAPI_ATTROWNER_MAX) && (N_attrib_info.count > 0)
            && (N_attrib_info.tupleSize <= 3) && (N_attrib_info.storage == HAPI_STORAGETYPE_FLOAT)) // normals do exist and they are in the right format
    {
        CoreArray<float> normals(N_attrib_info.count * N_attrib_info.count);
        if (HAPI_GetAttributeFloatData(session, asset_id, object_id, geo_id, part_id, HAPI_ATTRIB_NORMAL, &N_attrib_info, &normals[0], 0, N_attrib_info.count) == HAPI_RESULT_SUCCESS)
        {
            normal_maps.resize(1);
            GeometryNormalMap& normal_map = normal_maps[0];
            normal_map.normals.resize(N_attrib_info.count);
            for (int i = 0; i < N_attrib_info.count; ++i)
            {
                normal_map.normals[i] = GMathVec3f(0.0f, 0.0f, 0.0f);
                for (int j = 0; j < N_attrib_info.tupleSize; ++j)
                    normal_map.normals[i][j] = -normals[i * N_attrib_info.tupleSize + j]; // why do we have to negate here?
                // because of a different winding order? see maya plugin
                if (!normal_map.normals[i].normalize()) // this case generate something more useful
                    normal_map.normals[i] = GMathVec3f(0.0f, 1.0f, 0.0f);
            }
            const unsigned int nlist_count = polygon_vertex_indices.get_count();
            normal_map.polygon_indices.resize(nlist_count);
            if (N_owner == HAPI_ATTROWNER_VERTEX)
            {
                for (unsigned int i = 0; i < nlist_count; ++i)
                    normal_map.polygon_indices[i] = i;
            }
            else
            {
                for (unsigned int i = 0; i < nlist_count; ++i)
                    normal_map.polygon_indices[i] = polygon_vertex_indices[i];
            }
        }
    }
}

inline void export_uvs(const HAPI_Session* session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_PartInfo& part_info,
        CoreArray<unsigned int>& polygon_vertex_indices, CoreArray<GeometryUvMap>& uv_maps)
{
    HAPI_AttributeInfo UV_attrib_info;
    HAPI_AttributeOwner UV_owner = HAPI_ATTROWNER_MAX;
    UV_attrib_info.exists = false;
    if ((HAPI_GetAttributeInfo(session, asset_id, object_id, geo_id, part_id, HAPI_ATTRIB_UV, HAPI_ATTROWNER_POINT, &UV_attrib_info) == HAPI_RESULT_SUCCESS)
        && UV_attrib_info.exists)
        UV_owner = HAPI_ATTROWNER_POINT;
    else if ((HAPI_GetAttributeInfo(session, asset_id, object_id, geo_id, part_id, HAPI_ATTRIB_UV, HAPI_ATTROWNER_VERTEX, &UV_attrib_info) == HAPI_RESULT_SUCCESS)
        && UV_attrib_info.exists)
        UV_owner = HAPI_ATTROWNER_VERTEX;

    if ((UV_owner != HAPI_ATTROWNER_MAX) && (UV_attrib_info.count > 0)
            && (UV_attrib_info.tupleSize <= 3) && (UV_attrib_info.storage == HAPI_STORAGETYPE_FLOAT)) // normals do exist and they are in the right format
    {
        CoreArray<float> uvs(UV_attrib_info.count * UV_attrib_info.tupleSize);
        if (HAPI_GetAttributeFloatData(session, asset_id, object_id, geo_id, part_id, HAPI_ATTRIB_UV, &UV_attrib_info, &uvs[0], 0, UV_attrib_info.count) == HAPI_RESULT_SUCCESS)
        {
            uv_maps.resize(1);
            GeometryUvMap& uv_map = uv_maps[0];
            uv_map.vertices.resize(UV_attrib_info.count);
            for (int i = 0; i < UV_attrib_info.count; ++i)
            {
                uv_map.vertices[i] = GMathVec3f(0.0f, 0.0f, 0.0f);
                for (int j = 0; j < UV_attrib_info.tupleSize; ++j)
                    uv_map.vertices[i][j] = uvs[i * UV_attrib_info.tupleSize + j];
            }

            const unsigned int uvlist_count = polygon_vertex_indices.get_count();
            uv_map.polygon_indices.resize(uvlist_count);
            if (UV_owner == HAPI_ATTROWNER_VERTEX)
            {
                for (unsigned int i = 0; i < uvlist_count; ++i)
                    uv_map.polygon_indices[i] = i;
            }
            else
            {
                for (unsigned int i = 0; i < uvlist_count; ++i)
                    uv_map.polygon_indices[i] = polygon_vertex_indices[i];
            }
        }
    }
}

inline void export_materials(const HAPI_Session* session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_PartInfo& part_info,
        pthread_mutex_t* rec_mutex, CoreArray<CoreString>& shading_groups, CoreArray<unsigned int>& shading_group_indices)
{
    shading_group_indices.resize(part_info.faceCount);

    CoreArray<HAPI_MaterialId> material_ids(part_info.faceCount);
    memset(&material_ids[0], 0, sizeof(int) * part_info.faceCount);

    HAPI_Bool all_materials_the_same = 1;
    if (rec_mutex != 0)
        pthread_mutex_lock(rec_mutex);
    if (HAPI_GetMaterialIdsOnFaces(session, asset_id, object_id, geo_id, part_id,
            &all_materials_the_same, &material_ids[0], 0, part_info.faceCount) == HAPI_RESULT_SUCCESS)
    {
        // the material ids are not numbered from 0 to max material ids
        // they are numbered globally
        // so we have to convert them
        if (all_materials_the_same)
        {
            for (int i = 0; i < part_info.faceCount; ++i)
                shading_group_indices[i] = 0;
            shading_groups.resize(1);
            shading_groups[0] = get_material_name(session, asset_id, material_ids[0]);
        }
        else
        {
            std::map<int, unsigned int> material_map;
            for (int i = 0; i < part_info.faceCount; ++i)
            {
                HAPI_MaterialId id = material_ids[i];
                std::map<int, unsigned int>::const_iterator it = material_map.find(id);
                if (it == material_map.end())
                {
                    const unsigned int id_cl = static_cast<unsigned int>(material_map.size());
                    material_map.insert(std::make_pair(id, id_cl));
                    shading_group_indices[i] = id_cl;
                }
                else
                    shading_group_indices[i] = it->second;
            }
            const unsigned int material_count = static_cast<unsigned int>(material_map.size());
            shading_groups.resize(material_count);
            for (unsigned int i = 0; i <= material_count; ++i)
            {
                const HAPI_MaterialId id = material_ids[i];
                // linear search for now, building a new map could be more expensive
                for (std::map<int, unsigned int>::const_iterator it = material_map.begin(); it != material_map.end(); ++it)
                {
                    if (id == it->first)
                    {
                        shading_groups[it->second] = get_material_name(session, asset_id, id); // move code above to a function
                        break;
                    }
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < part_info.faceCount; ++i)
            shading_group_indices[i] = 0;

        shading_groups.resize(1);
        shading_groups[0] = "default";
    }
    if (rec_mutex != 0)
        pthread_mutex_unlock(rec_mutex);
}*/
