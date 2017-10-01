#pragma once

#include <of_app.h>
#include <core_vector.h>
#include <of_class.h>

void hapi_on_register_module(OfApp& app, CoreVector<OfClass *>& new_classes);
