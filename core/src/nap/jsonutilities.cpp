/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include "jsonutilities.h"

#include <rttr/property.h>
#include <rttr/instance.h>
#include <rtti/typeinfo.h>
#include <rtti/jsonreader.h>
#include <nap/numeric.h>

#include "logger.h"

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
			if (prop_type.is_class() && prop_type != RTTI_OF(std::string) && !prop_type.is_array())
			{
				auto nested_prop = prop.get_value(instance);
				if (!deserializeRecursive(json_field, nested_prop, lenient, errorState))
					return false;

				if (!errorState.check(prop.set_value(instance, nested_prop), "Error setting value for `%s`", prop_name.c_str()))
					return false;

				continue;
			}

			// Handle array properties
			if (prop_type.is_array() && json_field.IsArray())
			{
				// Get value type and create array view
				rtti::Variant value;
				value = prop.get_value(instance);
				auto array_view = value.create_array_view();
				if (!errorState.check(array_view.is_valid(), "Failed to create array view for property `%s`", prop_name.c_str()))
					return false;

				// Create array view and set correct size
				size_t size = json_field.Size();
				array_view.set_size(size);
				for (std::size_t index = 0; index < size; ++index)
				{
					// Extract wrapped value from array view
					auto wrapped_value = array_view.get_value_as_ref(index).extract_wrapped_value();

					// Deserialize the value
					if (!deserializeRecursive(json_field[index], wrapped_value, lenient, errorState))
						return false;

					// Set the value in the array view
					if (!errorState.check(array_view.set_value(index, wrapped_value), "Failed to set array property `%s`", prop_name.c_str()))
						return false;
				}

				// Set the value back to the property
				if (!errorState.check(prop.set_value(instance, value), "Failed to set array property `%s`", prop_name.c_str()))
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
