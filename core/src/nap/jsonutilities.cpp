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
	static bool deserializePrimitive(const rapidjson::Value& value, rtti::Property& property, rtti::Variant& outVariant, utility::ErrorState& errorState)
	{
		// Handle primitive types and string
		// Verify the property type and json field are equal data types so the property can be assigned
		bool success = false;
		auto type = property.get_type();

		if (type == RTTI_OF(bool) && value.IsBool())
			success = property.set_value(outVariant, value.GetBool());

		else if (type == RTTI_OF(std::string) && value.IsString())
			success = property.set_value(outVariant, std::string(value.GetString(), value.GetStringLength()));

		else if (type == RTTI_OF(uint64) && value.IsUint64())
			success = property.set_value(outVariant, value.GetUint64());

		else if (type == RTTI_OF(uint32) && value.IsUint())
			success = property.set_value(outVariant, value.GetUint());

		else if (type == RTTI_OF(int64) && value.IsInt64())
			success = property.set_value(outVariant, value.GetInt64());

		else if (type == RTTI_OF(int32) && value.IsInt())
			success = property.set_value(outVariant, value.GetInt());

		else if (type == RTTI_OF(float) && value.IsFloat())
			success = property.set_value(outVariant, value.GetFloat());

		else if (type == RTTI_OF(double) && value.IsDouble())
			success = property.set_value(outVariant, value.GetDouble());

		// Attempt an implicit conversion as fallback
		else if (type.is_arithmetic() && value.IsNumber())
			success = property.set_value(outVariant, value.GetDouble());

		if (!errorState.check(success, "Type mismatch or unhandled type for JSON key `%s`", property.get_name().to_string().c_str()))
			return false;

		return success;
	}


	static bool serializePrimitive(const rtti::TypeInfo& type, const rtti::Variant& value, rapidjson::Writer<rapidjson::StringBuffer>& writer, utility::ErrorState& errorState)
	{
		// Handle primitive types and string
		// Verify the property type and json field are equal data types so the property can be assigned
		if (type.is_arithmetic())
		{
			if (type == rtti::TypeInfo::get<bool>())
				return writer.Bool(value.to_bool());

			if (type == rtti::TypeInfo::get<char>())
				return writer.String(value.to_string().c_str());

			if (type == rtti::TypeInfo::get<int8_t>())
				return writer.Int(value.to_int8());

			if (type == rtti::TypeInfo::get<int16_t>())
				return writer.Int(value.to_int16());

			if (type == rtti::TypeInfo::get<int32_t>())
				return writer.Int(value.to_int32());

			if (type == rtti::TypeInfo::get<int64_t>())
				return writer.Int64(value.to_int64());

			if (type == rtti::TypeInfo::get<uint8_t>())
				return writer.Uint(value.to_uint8());

			if (type == rtti::TypeInfo::get<uint16_t>())
				return writer.Uint(value.to_uint16());

			if (type == rtti::TypeInfo::get<uint32_t>())
				return writer.Uint(value.to_uint32());

			if (type == rtti::TypeInfo::get<uint64_t>())
				return writer.Uint64(value.to_uint64());

			if (type == rtti::TypeInfo::get<float>())
				return writer.Double(value.to_double());

			if (type == rtti::TypeInfo::get<double>())
				return writer.Double(value.to_double());

			return false;
		}

		if (type.is_enumeration())
		{
			// Try to convert the enum to string first
			bool conversion_succeeded = false;
			auto result = value.to_string(&conversion_succeeded);
			if (conversion_succeeded)
				return writer.String(value.to_string().c_str());

			// Failed to convert enum to string; try to write as int
			conversion_succeeded = false;
			auto value_int = value.to_uint64(&conversion_succeeded);
			if (conversion_succeeded)
				return writer.Uint64(value_int);

			return false;
		}

		if (type == rtti::TypeInfo::get<std::string>())
			return writer.String(value.to_string().c_str());

		return false;
	}


	static bool deserializeArray(const rapidjson::GenericArray<true, rapidjson::Value>& array, rtti::Property& prop, rtti::Variant& outVariant, bool lenient, utility::ErrorState& errorState)
	{
		// Create instance
		rtti::Instance instance = outVariant;

		// Get name
		const auto prop_name = prop.get_name().to_string();

		// Get value type and create array view
		rtti::Variant value = prop.get_value(instance);
		auto array_view = value.create_array_view();
		if (!errorState.check(array_view.is_valid(), "Failed to create array view for property `%s`", prop_name.c_str()))
			return false;

		// Create array view and set correct size
		const size_t size = array.Size();
		array_view.set_size(size);
		for (size_t index = 0; index < size; ++index)
		{
			// Extract wrapped value from array view
			auto wrapped_value = array_view.get_value_as_ref(index).extract_wrapped_value();
			auto wrapped_value_type = wrapped_value.get_type();

			if (wrapped_value_type.is_class() && wrapped_value_type != RTTI_OF(std::string))
			{
				// Deserialize the value
				if (!deserializeRecursive(array[index], wrapped_value, lenient, errorState))
					return false;

				// Set the value in the array view
				if (!errorState.check(array_view.set_value(index, wrapped_value), "Failed to set array property `%s`", prop_name.c_str()))
					return false;
			}
			else
			{
				// Deserialize the primitive value in arrays
				auto variant = array_view.get_value_as_ref(index);
				if (!deserializePrimitive(array[index], prop, variant, errorState))
					return false;
			}
		}

		// Set the value back to the property
		if (!errorState.check(prop.set_value(instance, value), "Failed to set array property `%s`", prop_name.c_str()))
			return false;

		return true;
	}


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

			// Handle enum properties
            if (prop_type.is_enumeration())
            {
                if (!errorState.check(json_field.IsString(), "Enum property `%s` must be a string", prop_name.c_str()))
                    return false;

                // Get the enum value from the string
                auto enum_value = prop_type.get_enumeration().name_to_value(json_field.GetString());
                if (!errorState.check(enum_value.is_valid(), "Enum value `%s` for property `%s` is not valid", json_field.GetString(), prop_name.c_str()))
                    return false;

                // Set the enum value to the property
                if (!errorState.check(prop.set_value(instance, enum_value), "Error setting enum value for `%s`", prop_name.c_str()))
                    return false;

                continue;
            }

			// Handle array properties
			if (prop_type.is_array() && json_field.IsArray())
			{
				if (!errorState.check(deserializeArray(json_field.GetArray(), prop, outVariant, lenient, errorState), "Error deserializing array property `%s`", prop_name.c_str()))
					return false;

				continue;
			}

			if (!deserializePrimitive(json_field, prop, outVariant, errorState))
				return false;
		}
		return true;
	}


	static bool serializeArray(const rtti::Instance object, const rtti::VariantArray& array, rapidjson::Writer<rapidjson::StringBuffer>& writer, rapidjson::StringBuffer& buffer, utility::ErrorState& errorState)
	{
		// Write the start of the array
		if (!errorState.check(writer.StartArray(), "Failed write start of array"))
			return false;

		// Write the elements
		for (int i = 0; i < array.get_size(); ++i)
		{
			// Start compound
			if (!errorState.check(writer.StartObject(), "Failed to start nested compound"))
				return false;

			// Write each value
			if (!serializeRecursive(array.get_value(i), writer, buffer, errorState))
				return false;

			// Finish compound
			if (!errorState.check(writer.EndObject(), "Failed to end nested compound"))
				return false;
		}

		// Finish the array
		if (!errorState.check(writer.EndArray(), "Failed to finish array"))
			return false;

		return true;
	}


	bool serializeRecursive(const rtti::Instance object, rapidjson::Writer<rapidjson::StringBuffer>& writer, rapidjson::StringBuffer& buffer, utility::ErrorState& errorState)
	{
		// Ensure the object is not of a wrapped type
		if (!errorState.check(!object.get_type().get_raw_type().is_wrapper(), "Wrapped types are not supported"))
			return false;

		// Write all properties
		for (const auto& property : object.get_derived_type().get_properties())
		{
			// Get the value of the property
			auto value = property.get_value(object);
			assert(value.is_valid());

			// Write property name
			if (!errorState.check(writer.String(property.get_name().data()), "Failed to write property name"))
				return false;

			// If this is an array, recurse
			auto value_type = value.get_type();
			if (value_type.is_array())
			{
				if (!serializeArray(value, value.create_array_view(), writer, buffer, errorState))
					return false;

				continue;
			}

			if (rtti::isPrimitive(value_type))
			{
				// Write primitive type (float, string, etc)
				if (!serializePrimitive(value_type, value, writer, errorState))
					return false;

				continue;
			}

			// Write nested compound
			if (!value_type.get_properties().empty())
			{
				// Start compound
				if (!errorState.check(writer.StartObject(), "Failed to start nested compound"))
					return false;

				// Recurse into compound
				if (!serializeRecursive(value, writer, buffer, errorState))
					return false;

				// Finish compound
				if (!errorState.check(writer.EndObject(), "Failed to end nested compound"))
					return false;

				continue;
			}

			// Associative containers
			if (!errorState.check(!value_type.is_associative_container(), "Associative containers are not supported"))
				return false;

			// Pointers
			if (!errorState.check(!value_type.is_pointer(), "Pointers are not supported"))
				return false;

			// Unknown types
			errorState.fail("Encountered unknown property type");
			return false;
		}
		return true;
	}
}
