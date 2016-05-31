#pragma once

#include <HAPI.h>
#include <limits.h>

struct HapiObjectData{
    HAPI_PartInfo part_info;
    HAPI_AssetId asset_id;
    HAPI_ObjectId object_id;
    HAPI_GeoId geo_id;
    HAPI_Session* session;

    union{
        HAPI_CurveInfo curve_info;
        HAPI_PartId part_id;
    };
    
    HapiObjectData()
    {
        asset_id = INT_MIN;
        object_id = INT_MIN;
        geo_id = INT_MIN;
        part_id = INT_MIN;
        session = 0;
    }
};
