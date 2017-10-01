#include <of_object.h>
#include <of_app.h>
#include <dso_export.h>
#include <module_material.h>
#include <ctx_shader.h>
#include <shader_helpers.h>
#include <of_object_factory.h>
#include <module_light.h>

#include "hapi_mesh.h"
#include "hapi_mesh_from_memory.h"
#include "hapi_curve.h"
#include "hapi_curve_from_memory.h"
#include "hapi.h"

IX_BEGIN_EXTERN_C
DSO_EXPORT void
on_register_module(OfApp& app, CoreVector<OfClass *>& new_classes)
{
	hapi_on_register_module(app, new_classes);
    hapi_mesh_on_register_module(app, new_classes);
    hapi_mesh_from_memory_on_register_module(app, new_classes);
    hapi_curve_on_register_module(app, new_classes);
    hapi_curve_from_memory_on_register_module(app, new_classes);
}
IX_END_EXTERN_C
