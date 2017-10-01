#pragma once

#include <core_array.h>
#include <gmath_vec3.h>
#include <geometry_object.h>
#include <core_mutex.h>
#include <of_app.h>
#include <of_class.h>
#include <core_vector.h>

struct CurveFromMemoryData {
    CoreMutexHandle* lock;
    // vertex_count specifies the number of CVs per curve
    // vertices is just a list of vertices for each curve
    CoreArray<GMathVec3f> vertices;
    CoreArray<float> radii;
    CoreArray<unsigned int> vertex_count;

    CurveFromMemoryData()
    {
        lock = CoreMutex::create();
    }

    ~CurveFromMemoryData()
    {
        CoreMutex::destroy(lock);
    }

    void clear()
    {
    	vertices.remove_all();
    	radii.remove_all();
    	vertex_count.remove_all();
    }
};

void hapi_curve_from_memory_on_register_module(OfApp& app, CoreVector<OfClass *>& new_classes);
