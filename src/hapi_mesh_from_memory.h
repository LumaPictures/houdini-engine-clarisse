#pragma once

#include <core_array.h>
#include <gmath_vec3.h>
#include <geometry_object.h>
#include <core_mutex.h>
#include <of_app.h>
#include <of_class.h>
#include <core_vector.h>

struct MeshFromMemoryData{
    CoreMutexHandle* lock;
    // empty data in the first version
    CoreArray<GMathVec3f> velocities;
    CoreArray<GeometryUvMap> uv_maps;
    CoreArray<GeometryNormalMap> normal_maps;
    CoreArray<GeometryColorMap> color_maps;

    // data that's actually filled out
    CoreArray<GMathVec3f> vertices;
    CoreArray<unsigned int> polygon_vertex_count;
    CoreArray<unsigned int> polygon_vertex_indices;
    CoreArray<unsigned int> shading_group_indices;
    CoreArray<CoreString> shading_groups;

    MeshFromMemoryData()
    {
        lock = CoreMutex::create();
    }

    ~MeshFromMemoryData()
    {
        CoreMutex::destroy(lock);
    }

    void clear()
    {
        velocities.remove_all();
        uv_maps.remove_all();
        normal_maps.remove_all();
        color_maps.remove_all();

        vertices.remove_all();
        polygon_vertex_count.remove_all();
        polygon_vertex_indices.remove_all();
        shading_group_indices.remove_all();
        shading_groups.remove_all();
    }
};

void hapi_mesh_from_memory_on_register_module(OfApp& app, CoreVector<OfClass *>& new_classes);
