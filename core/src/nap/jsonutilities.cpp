/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include "jsonutilities.h"

#include <rttr/property.h>
#include <rttr/instance.h>
#include <rtti/typeinfo.h>
#include <rtti/jsonreader.h>
#include <nap/numeric.h>

namespace nap::utility
{
	 bool deserializeRecursive(const rapidjson::Value& node, rtti::Variant& outVariant, bool lenient, utility::ErrorState& errorState)
	 {
		if (!errorState.check(node.IsObject(), "Node is not an object"))
			return false;

		if (!errorState.check(outVariant.is_valid(), "Invalid variant"))
			return false;

		rtti::Instance instance = outVariant;
		if (!errorState.check(instance.is_valid(), "Failed to create rttr instance from variant of type `%s`", outVariant.get_type().get_name().to_string().c_str()))
			return false;

		auto obj_type = instance.get_type();
		if (!errorState.check(obj_type.is_valid(), "Invalid type"))
			return false;

		for (auto prop : obj_type.get_properties())
		{
			if (!prop.is_valid())
				continue;

			// Check if the property name exists in the current json object
			auto prop_name = prop.get_name().to_string();
			if (!node.HasMember(prop_name.c_str()))
			{
				// Skip if missing properties are allowed
				if (lenient)
					continue;

				// Otherwise bail
				errorState.fail("Property `%s` missing", prop_name.c_str());
				return false;
			}

			const auto& json_field = node[prop_name.c_str()];
			auto prop_type = prop.get_type();

			// Handle (nested) objects
			if (prop_type.is_class() && prop_type != RTTI_OF(std::string))
			{
				auto nested_prop = prop.get_value(instance);
				if (!deserializeRecursive(json_field, nested_prop, lenient, errorState))
					return false;

				if (!errorState.check(prop.set_value(instance, nested_prop), "Error setting value for `%s`", prop_name.c_str()))
					return false;

				continue;
			}

			// Handle primitive types and string
			// Verify the property type and json field are equal data types so the property can be assigned
			bool success = false;

			if (prop_type == RTTI_OF(bool) && json_field.IsBool())
				success = prop.set_value(instance, json_field.GetBool());

			else if (prop_type == RTTI_OF(std::string) && json_field.IsString())
				success = prop.set_value(instance, std::string(json_field.GetString()));

			else if (prop_type == RTTI_OF(nap::uint64) && json_field.IsUint64())
				success = prop.set_value(instance, json_field.GetUint64());

			else if (prop_type == RTTI_OF(uint32) && json_field.IsUint())
				success = prop.set_value(instance, json_field.GetUint());

			else if (prop_type == RTTI_OF(int64) && json_field.IsInt64())
				success = prop.set_value(instance, json_field.GetInt64());

			else if (prop_type == RTTI_OF(int32) && json_field.IsInt())
				success = prop.set_value(instance, json_field.GetInt());

			else if (prop_type == RTTI_OF(float) || prop_type == RTTI_OF(double))
			{
				if (json_field.IsDouble())
					success = prop.set_value(instance, json_field.GetDouble());

				else if (json_field.IsNumber())
					success = prop.set_value(instance, json_field.GetFloat());
			}

			if (!errorState.check(success, "Type mismatch or unhandled type for JSON key `%s`", prop_name.c_str()))
				return false;
		}
		return true;
	}
}
