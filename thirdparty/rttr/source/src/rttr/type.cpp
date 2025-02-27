/************************************************************************************
*                                                                                   *
*   Copyright (c) 2014, 2015 - 2017 Axel Menzel <info@rttr.org>                     *
*                                                                                   *
*   This file is part of RTTR (Run Time Type Reflection)                            *
*   License: MIT License                                                            *
*                                                                                   *
*   Permission is hereby granted, free of charge, to any person obtaining           *
*   a copy of this software and associated documentation files (the "Software"),    *
*   to deal in the Software without restriction, including without limitation       *
*   the rights to use, copy, modify, merge, publish, distribute, sublicense,        *
*   and/or sell copies of the Software, and to permit persons to whom the           *
*   Software is furnished to do so, subject to the following conditions:            *
*                                                                                   *
*   The above copyright notice and this permission notice shall be included in      *
*   all copies or substantial portions of the Software.                             *
*                                                                                   *
*   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR      *
*   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,        *
*   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE     *
*   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER          *
*   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,   *
*   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE   *
*   SOFTWARE.                                                                       *
*                                                                                   *
*************************************************************************************/

#include "rttr/type.h"

#include "rttr/constructor.h"
#include "rttr/property.h"
#include "rttr/destructor.h"
#include "rttr/enumeration.h"
#include "rttr/method.h"

#include "rttr/detail/constructor/constructor_wrapper_base.h"
#include "rttr/detail/destructor/destructor_wrapper_base.h"
#include "rttr/detail/enumeration/enumeration_wrapper_base.h"
#include "rttr/detail/method/method_wrapper_base.h"
#include "rttr/detail/property/property_wrapper.h"
#include "rttr/rttr_enable.h"

#include "rttr/detail/parameter_info/parameter_infos_compare.h"
#include "rttr/detail/filter/filter_item_funcs.h"
#include "rttr/detail/type/type_register_p.h"

#include <algorithm>
#include <unordered_map>
#include <vector>
#include <memory>
#include <set>
#include <thread>
#include <mutex>
#include <cstring>
#include <cctype>
#include <utility>

using namespace std;

namespace rttr
{
namespace
{

/////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
static array_range<T> get_items_for_type(const type& t,
                                         const std::vector<T>& vec)
{
    return array_range<T>(vec.data(), vec.size(),
                          detail::default_predicate<T>([t](const T& item) { return (item.get_declaring_type() == t); }) );
}

} // end anonymous namespace

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////

std::vector<type> type::get_raw_types() RTTR_NOEXCEPT
{
	auto& type_data_storage = detail::type_register_private::get_type_data_storage();

	std::vector<type> types;
	types.reserve(type_data_storage.size());

	for (auto&& type_data : type_data_storage)
	{
		if (std::find(types.begin(), types.end(), type(type_data->raw_type_data)) != types.end())
			continue;

		types.emplace_back(type(type_data->raw_type_data));
	}

	return types;
}

/////////////////////////////////////////////////////////////////////////////////////////

bool type::is_derived_from(const type& other) const RTTR_NOEXCEPT
{
    auto& src_raw_type = m_type_data->raw_type_data;
    auto& tgt_raw_type = other.m_type_data->raw_type_data;

    if (src_raw_type == tgt_raw_type)
        return true;

    for (auto& t : src_raw_type->get_class_data().m_base_types)
    {
        if (t.m_type_data == tgt_raw_type)
        {
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////

void* type::apply_offset(void* ptr, const type& source_type, const type& target_type) RTTR_NOEXCEPT
{
    auto& src_raw_type = source_type.m_type_data->raw_type_data;
    auto& tgt_raw_type = target_type.m_type_data->raw_type_data;

    if (src_raw_type == tgt_raw_type || ptr == nullptr)
        return ptr;

    const detail::derived_info info = src_raw_type->get_class_data().m_derived_info_func(ptr);
    if (info.m_type.m_type_data->raw_type_data == tgt_raw_type)
        return info.m_ptr;

    auto& class_list = info.m_type.m_type_data->raw_type_data->get_class_data();
    int i = 0;
    for (auto& t : class_list.m_base_types)
    {
        if (t.m_type_data == tgt_raw_type)
        {
            return class_list.m_conversion_list[i](info.m_ptr);
        }
        ++i;
    }

    return nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////

type type::get_derived_type(void* ptr, const type& source_type) RTTR_NOEXCEPT
{
    if (ptr == nullptr)
        return type();

    auto& src_raw_type = source_type.m_type_data->raw_type_data;
    const detail::derived_info info = src_raw_type->get_class_data().m_derived_info_func(ptr);
    return info.m_type;
}

/////////////////////////////////////////////////////////////////////////////////////////

array_range<type> type::get_base_classes() const RTTR_NOEXCEPT
{
    return array_range<type>(m_type_data->get_class_data().m_base_types.data(),
                             m_type_data->get_class_data().m_base_types.size());
}

/////////////////////////////////////////////////////////////////////////////////////////

array_range<type> type::get_derived_classes() const RTTR_NOEXCEPT
{
    return array_range<type>(m_type_data->get_class_data().m_derived_types.data(),
                             m_type_data->get_class_data().m_derived_types.size());
}

/////////////////////////////////////////////////////////////////////////////////////////

vector<type> type::get_raw_derived_classes() const RTTR_NOEXCEPT
{
	vector<type> result;

	for (auto& derived_type : m_type_data->get_class_data().m_derived_types)
	{
		type raw_type = derived_type.get_raw_type();
		if (std::find(result.begin(), result.end(), raw_type) != result.end())
			continue;

		result.emplace_back(raw_type);
	}

	return result;
}

/////////////////////////////////////////////////////////////////////////////////////////

array_range<type> type::get_types() RTTR_NOEXCEPT
{
    auto& type_list = detail::type_register_private::get_type_storage();
    return array_range<type>(&type_list[1], type_list.size() - 1);
}

/////////////////////////////////////////////////////////////////////////////////////////

variant type::get_metadata(const variant& key) const
{
    return detail::type_register_private::get_metadata(*this, key);
}

/////////////////////////////////////////////////////////////////////////////////////////

variant type::create(vector<argument> args) const
{
    auto& ctors = m_type_data->get_class_data().m_ctors;
    for (const auto& ctor : ctors)
    {
        if (detail::compare_with_arg_list::compare(ctor.get_parameter_infos(), args))
            return ctor.invoke_variadic(std::move(args));
    }

    return variant();
}

/////////////////////////////////////////////////////////////////////////////////////////

variant type::create_from_type(const char* name, std::vector<argument> args)
{
	return get_by_name(name).create(args);
}

/////////////////////////////////////////////////////////////////////////////////////////

bool type::destroy(variant& obj) const RTTR_NOEXCEPT
{
    return get_destructor().invoke(obj);
}

/////////////////////////////////////////////////////////////////////////////////////////

property type::get_property(string_view name) const RTTR_NOEXCEPT
{
    const auto& vec = get_raw_type().m_type_data->get_class_data().m_properties;
    auto ret = std::find_if(vec.cbegin(), vec.cend(),
                            [name](const property& item)
                            {
                                return (item.get_name() == name);
                            });
    if (ret != vec.cend())
        return *ret;

    return detail::create_invalid_item<property>();
}

/////////////////////////////////////////////////////////////////////////////////////////

variant type::get_property_value(string_view name, instance obj) const
{
    return get_property(name).get_value(obj);
}

/////////////////////////////////////////////////////////////////////////////////////////

variant type::get_property_value(string_view name)
{
    const auto prop = get_global_property(name);
    return prop.get_value(instance());
}

/////////////////////////////////////////////////////////////////////////////////////////

bool type::set_property_value(string_view name, instance obj, argument arg) const
{
    const auto prop = get_property(name);
    return prop.set_value(obj, arg);
}

/////////////////////////////////////////////////////////////////////////////////////////

bool type::set_property_value(string_view name, argument arg)
{
    const auto prop = get_global_property(name);
    return prop.set_value(instance(), arg);
}

/////////////////////////////////////////////////////////////////////////////////////////

array_range<property> type::get_properties() const RTTR_NOEXCEPT
{
    auto& vec = get_raw_type().m_type_data->get_class_data().m_properties;
    if (!vec.empty())
    {
        return array_range<property>(vec.data(), vec.size(),
                                     detail::default_predicate<property>([](const property& prop)
                                     {
                                         return (prop.get_access_level() == access_levels::public_access);
                                     }) );
    }

    return array_range<property>();
}

/////////////////////////////////////////////////////////////////////////////////////////

array_range<property> type::get_properties(filter_items filter) const RTTR_NOEXCEPT
{
    const auto raw_t = get_raw_type();
    auto& vec = raw_t.m_type_data->get_class_data().m_properties;
    if (!vec.empty())
        return array_range<property>(vec.data(), vec.size(), detail::get_filter_predicate<property>(raw_t, filter));

    return array_range<property>();
}

/////////////////////////////////////////////////////////////////////////////////////////

method type::get_method(string_view name) const RTTR_NOEXCEPT
{
    const auto raw_t = get_raw_type();
    const auto& vec = raw_t.m_type_data->get_class_data().m_methods;
    auto ret = std::find_if(vec.cbegin(), vec.cend(),
                            [name](const method& item)
                            {
                                return (item.get_name() == name);
                            });
    if (ret != vec.cend())
        return *ret;

    return detail::create_invalid_item<method>();
}

/////////////////////////////////////////////////////////////////////////////////////////

method type::get_method(string_view name, const std::vector<type>& type_list) const RTTR_NOEXCEPT
{
    const auto raw_t = get_raw_type();
    for (const auto& meth : raw_t.m_type_data->get_class_data().m_methods)
    {
        if ( meth.get_name() == name &&
             detail::compare_with_type_list::compare(meth.get_parameter_infos(), type_list))
        {
            return meth;
        }
    }

    return detail::create_invalid_item<method>();
}

/////////////////////////////////////////////////////////////////////////////////////////

array_range<method> type::get_methods() const RTTR_NOEXCEPT
{
    const auto raw_t = get_raw_type();
    auto& vec = raw_t.m_type_data->get_class_data().m_methods;
    if (!vec.empty())
    {
        return array_range<method>(vec.data(), vec.size(),
                                   detail::default_predicate<method>([](const method& meth)
                                   {
                                        return (meth.get_access_level() == access_levels::public_access);
                                   }) );
    }

    return array_range<method>();
}

/////////////////////////////////////////////////////////////////////////////////////////

array_range<method> type::get_methods(filter_items filter) const RTTR_NOEXCEPT
{
    const auto raw_t = get_raw_type();
    auto& vec = raw_t.m_type_data->get_class_data().m_methods;
    if (!vec.empty())
        return array_range<method>(vec.data(), vec.size(), detail::get_filter_predicate<method>(raw_t, filter));

    return array_range<method>();
}

/////////////////////////////////////////////////////////////////////////////////////////

property type::get_global_property(string_view name) RTTR_NOEXCEPT
{
    auto& prop_list = detail::type_register_private::get_global_property_storage();
    const auto ret = prop_list.find(name);
    if (ret != prop_list.end())
        return *ret;

    return detail::create_invalid_item<property>();
}

/////////////////////////////////////////////////////////////////////////////////////////

method type::get_global_method(string_view name) RTTR_NOEXCEPT
{
    auto& meth_list = detail::type_register_private::get_global_method_storage();
    const auto ret = meth_list.find(name);
    if (ret != meth_list.end())
        return *ret;

    return detail::create_invalid_item<method>();
}

/////////////////////////////////////////////////////////////////////////////////////////

method type::get_global_method(string_view name, const std::vector<type>& type_list) RTTR_NOEXCEPT
{
    auto& meth_list = detail::type_register_private::get_global_method_storage();
    auto itr = meth_list.find(name);
    while (itr != meth_list.end())
    {
        const auto& meth = *itr;
        if (meth.get_name() != name)
            break;

        if (detail::compare_with_type_list::compare(meth.get_parameter_infos(), type_list))
            return meth;

        ++itr;
    }

    return detail::create_invalid_item<method>();
}

/////////////////////////////////////////////////////////////////////////////////////////

array_range<method> type::get_global_methods() RTTR_NOEXCEPT
{
    auto& vec = detail::type_register_private::get_global_method_storage().value_data();
    return array_range<method>(vec.data(), vec.size());
}

/////////////////////////////////////////////////////////////////////////////////////////

array_range<property> type::get_global_properties() RTTR_NOEXCEPT
{
    auto& vec = detail::type_register_private::get_global_property_storage().value_data();
    return array_range<property>(vec.data(), vec.size());
}

/////////////////////////////////////////////////////////////////////////////////////////

enumeration type::get_enumeration() const RTTR_NOEXCEPT
{
    return detail::type_register_private::get_enumeration(*this);
}

/////////////////////////////////////////////////////////////////////////////////////////

variant type::invoke(string_view name, instance obj, std::vector<argument> args) const
{
    const auto raw_t = get_raw_type();
    for (const auto& meth : raw_t.m_type_data->get_class_data().m_methods)
    {
        if ( meth.get_name() == name &&
             detail::compare_with_arg_list::compare(meth.get_parameter_infos(), args))
        {
            return meth.invoke_variadic(obj, args);
        }
    }

    return variant();
}

/////////////////////////////////////////////////////////////////////////////////////////

variant type::invoke(string_view name, std::vector<argument> args)
{
    auto& meth_list = detail::type_register_private::get_global_method_storage();
    auto itr = meth_list.find(name);
    while (itr != meth_list.end())
    {
        const auto& meth = *itr;
        if (meth.get_name() != name)
            break;

        if (detail::compare_with_arg_list::compare(meth.get_parameter_infos(), args))
        {
            return meth.invoke_variadic(instance(), args);
        }

        ++itr;
    }

    return variant();
}

/////////////////////////////////////////////////////////////////////////////////////////

type type::get_by_name(string_view name) RTTR_NOEXCEPT
{
    auto& custom_name_to_id = detail::type_register_private::get_custom_name_to_id();
    auto ret = custom_name_to_id.find(name);
    if (ret != custom_name_to_id.end())
        return (*ret);

    return detail::get_invalid_type();
}

/////////////////////////////////////////////////////////////////////////////////////////

const detail::type_converter_base* type::get_type_converter(const type& target_type) const RTTR_NOEXCEPT
{
    return detail::type_register_private::get_converter(*this, target_type);
}

/////////////////////////////////////////////////////////////////////////////////////////

bool type::can_create_instance() const RTTR_NOEXCEPT
{
	return !get_constructors().empty();
}

/////////////////////////////////////////////////////////////////////////////////////////

constructor type::get_constructor(const std::vector<type>& args) const RTTR_NOEXCEPT
{
    auto& ctors = m_type_data->get_class_data().m_ctors;
    for (const auto& ctor : ctors)
    {
        if (detail::compare_with_type_list::compare(ctor.get_parameter_infos(), args))
            return ctor;
    }

    return detail::create_invalid_item<constructor>();
}

/////////////////////////////////////////////////////////////////////////////////////////

array_range<constructor> type::get_constructors() const RTTR_NOEXCEPT
{
    auto& ctors = m_type_data->get_class_data().m_ctors;
    if (!ctors.empty())
    {
        return array_range<constructor>(ctors.data(), ctors.size(),
                                        detail::default_predicate<constructor>([](const constructor& ctor)
                                        {
                                            return (ctor.get_access_level() == access_levels::public_access);
                                        }) );
    }

    return array_range<constructor>();
}

/////////////////////////////////////////////////////////////////////////////////////////

array_range<constructor> type::get_constructors(filter_items filter) const RTTR_NOEXCEPT
{
    auto& ctors = m_type_data->get_class_data().m_ctors;
    if (!ctors.empty())
        return array_range<constructor>(ctors.data(), ctors.size(), detail::get_filter_predicate<constructor>(*this, filter));

    return array_range<constructor>();
}

/////////////////////////////////////////////////////////////////////////////////////////

destructor type::get_destructor() const RTTR_NOEXCEPT
{
    return get_raw_type().m_type_data->get_class_data().m_dtor;
}

/////////////////////////////////////////////////////////////////////////////////////////

} // end namespace rttr
