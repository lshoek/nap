/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#pragma once

#include <rtti/jsonreader.h>
#include <rttr/variant.h>
#include <rapidjson/document.h>
#include <utility/dllexport.h>
#include <utility/errorstate.h>

namespace nap::utility
{
	/**
	 * Deserialize JSON value
	 * The target object must be rttr registered
	 * @tparam T target object type
	 * @param node json node to deserialize
	 * @param outVariant target object object to write wrapped in rttr variant
	 * @param lenient whether missing properties are allowed in the json
	 * @param errorState the error state if deserialization fails
	 * @return whether deserialization was successful
	 */
	bool NAPAPI deserializeRecursive(const rapidjson::Value& node, rtti::Variant& outVariant, bool lenient, utility::ErrorState& errorState);

	/**
	 * Generic JSON deserialization. The target object must be rttr registered.
	 * Below is a simple example of a nested data type.
	 *
	 * ~~~~~{.h}
	 * 	struct MyPayload
	 *	{
	 *		RTTR_ENABLE()
	 *	public:
	 *		uint flag = 0;
	 *		std::string text;
	 *	};
	 *
	 *	struct MyStruct
	 *	{
	 *		RTTR_ENABLE()
	 *	public:
	 *		std::string name;
	 *		MyPayload data = 0;
	 *	};
	 * ~~~~~
	 *
	 * ~~~~~{.cpp}
	 *	RTTR_REGISTRATION
	 *	{
	 *	rttr::registration::class_<nap::MyPayload>("MyPayload")
	 *		.constructor<>()
	 *		.property("flag", 		&nap::TrackerResponseData::flag)
	 *		.property("text", 		&nap::TrackerResponseData::text)
	 *
	 *	rttr::registration::class_<nap::MyStruct>("MyStruct")
	 *		.constructor<>()
	 *		.property("name", 		&nap::TrackerResponseRoot::name)
	 *		.property("data", 		&nap::TrackerResponseRoot::data)
	 *	}
	 * ~~~~~
	 *
	 * @tparam T target object type
	 * @param json json to deserialize
	 * @param outObject target object object to write
	 * @param lenient whether missing properties are allowed in the json
	 * @param errorState the error state if deserialization fails
	 * @return whether deserialization was successful
	 */
	template<typename T>
	bool NAPAPI deserialize(const std::string& json, T& outObject, bool lenient, utility::ErrorState& errorState)
	{
		rapidjson::Document doc;
		if (!rtti::JSONDocumentFromString(json, doc, errorState))
			return false;

		// Ensure document root is object
		if (!errorState.check(doc.IsObject(), "Missing root json object"))
			return false;

		// Wrap a copy of the object in an rttr variant
		rtti::Variant variant = outObject;
		if (!deserializeRecursive(doc, variant, lenient, errorState))
			return false;

		outObject = variant.get_value<T>();
		return true;
	}
}
