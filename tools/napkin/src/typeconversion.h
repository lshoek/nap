/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// Local Includes
#include "document.h"

// External Includes
#include <QVariant>
#include <rtti/typeinfo.h>
#include <rttr/type.h>
#include <nap/logger.h>
#include <instanceproperty.h>

namespace napkin
{

	/**
	 * Attempt to convert an enum string value into its integer counterpart.
	 * @param enumer The enumeration to use while converting
	 * @param name The name of the enum value
	 * @param ok Will be set to true if all was dandy, false if it failed.
	 * @return The enum value as an integer
	 */
	uint64_t enumStringToIndex(rttr::enumeration enumer, const std::string& name, bool* ok);

	/**
	 * Convert an integer enum value into a QString.
	 * @param enumer The enumation to use while converting.
	 * @param index The enum value as integer.
	 * @return The enum value as a QString.
	 */
	QString enumIndexToQString(rttr::enumeration enumer, int index);

	/**
	 * Convert an integer enum value into an std::string.
	 * @param enumer The enumation to use while converting.
	 * @param index The enum value as integer.
	 * @return The enum value as a std::string.
	 */
	std::string enumIndexToStdString(rttr::enumeration enumer, int index);

	/**
	 * Convert an rttr::variant into a QVariant.
	 * @param type The type of the value.
	 * @param value The value to convert.
	 * @param outValue The resulting QVariant
	 * @return True if the conversion was successful, false otherwise.
	 */
	bool toQVariant(const nap::rtti::TypeInfo& type, const rttr::variant& value, QVariant& outValue);

	/**
	 * Convert a QVariant value into an rttr::variant.
	 * @param type The type of the value.
	 * @param variant The value to convert.
	 * @param ok Will be set to true if conversion was successful, false otherwise.
	 * @return The resulting value as an rttr::variant
	 */
	rttr::variant fromQVariant(const nap::rtti::TypeInfo& type, const QVariant& variant, bool* ok);

	/**
	 * Creates an instance property override for the given type
	 * @param type the type of the instance property override
	 * @param document the document the property belongs to
	 */
	nap::InstancePropertyValue* createInstanceProperty(const rttr::type& type, napkin::Document& document);

	/**
	 * Deletes an instance property override
	 * @param instanceProperty the instance property override to remove
	 * @param document the document the property belongs to
	 */
	void removeInstanceProperty(rttr::variant& instanceProperty, napkin::Document& document);

	/**
	 * @return the value of the instance property override
	 * @param instanceProperty instance property that holds the value
	 */
	rttr::variant getInstancePropertyValue(const nap::InstancePropertyValue& instanceProperty);

	/**
	 * Assigns a new value to the instance property
	 * @param instanceProperty variant that holds the instance property
	 * @param value new instance property value
	 * @return if the value has been assigned to the instance property
	 */
	bool setInstancePropertyValue(rttr::variant& instanceProperty, const rttr::variant& value);
}

