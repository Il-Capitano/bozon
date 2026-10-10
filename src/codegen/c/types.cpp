#include "types.h"

namespace codegen::c
{

struct_type_t const &type_set_t::get_struct_type(type::struct_reference struct_ref) const
{
	bz_assert(struct_ref.index < this->struct_types.size());
	return this->struct_types[struct_ref.index];
}

typedef_type_t const &type_set_t::get_typedef_type(type::typedef_reference typedef_ref) const
{
	bz_assert(typedef_ref.index < this->typedef_types.size());
	return this->typedef_types[typedef_ref.index];
}

array_type_t const &type_set_t::get_array_type(type::array_reference array_ref) const
{
	bz_assert(array_ref.index < this->array_types.size());
	return this->array_types[array_ref.index];
}

slice_type_t const &type_set_t::get_slice_type(type::slice_reference slice_ref) const
{
	bz_assert(slice_ref.index < this->slice_types.size());
	return this->slice_types[slice_ref.index];
}

function_type_t const &type_set_t::get_function_type(type::function_reference function_ref) const
{
	bz_assert(function_ref.index < this->function_types.size());
	return this->function_types[function_ref.index];
}

bz::u8string_view type_set_t::get_struct_type_name(type::struct_reference struct_ref) const
{
	bz_assert(struct_ref.index < this->struct_type_names.size());
	return this->struct_type_names[struct_ref.index];
}

bz::u8string_view type_set_t::get_typedef_type_name(type::typedef_reference typedef_ref) const
{
	bz_assert(typedef_ref.index < this->typedef_type_names.size());
	return this->typedef_type_names[typedef_ref.index];
}

bz::u8string_view type_set_t::get_array_type_name(type::array_reference array_ref) const
{
	bz_assert(array_ref.index < this->array_type_names.size());
	return this->array_type_names[array_ref.index];
}

bz::u8string_view type_set_t::get_function_type_name(type::function_reference function_ref) const
{
	bz_assert(function_ref.index < this->function_type_names.size());
	return this->function_type_names[function_ref.index];
}

std::pair<type::struct_reference, bool> type_set_t::add_struct_type(struct_type_t struct_type)
{
	auto const view = struct_type_view_t{ struct_type.members };
	if (auto const it = this->struct_types_map.find(view); it != this->struct_types_map.end())
	{
		return { it->second, false };
	}

	auto const result = type::struct_reference{ .index = static_cast<uint32_t>(this->struct_types.size()) };
	this->struct_types.push_back(std::move(struct_type));
	auto const &new_type = this->struct_types.back();
	this->struct_types_map.insert({ struct_type_view_t{ new_type.members }, result });
	return { result, true };
}

std::pair<type::typedef_reference, bool> type_set_t::add_typedef_type(typedef_type_t typedef_type)
{
	auto const view = typedef_type_view_t{ typedef_type.aliased_type };
	if (auto const it = this->typedef_types_map.find(view); it != this->typedef_types_map.end())
	{
		return { it->second, false };
	}

	auto const result = type::typedef_reference{ .index = static_cast<uint32_t>(this->typedef_types.size()) };
	this->typedef_types.push_back(std::move(typedef_type));
	auto const &new_type = this->typedef_types.back();
	this->typedef_types_map.insert({ typedef_type_view_t{ new_type.aliased_type }, result });
	return { result, true };
}

std::pair<type::array_reference, bool> type_set_t::add_array_type(array_type_t array_type)
{
	auto const view = array_type_view_t{ array_type.elem_type, array_type.size };
	if (auto const it = this->array_types_map.find(view); it != this->array_types_map.end())
	{
		return { it->second, false };
	}

	auto const result = type::array_reference{ .index = static_cast<uint32_t>(this->array_types.size()) };
	this->array_types.push_back(std::move(array_type));
	auto const &new_type = this->array_types.back();
	this->array_types_map.insert({ array_type_view_t{ new_type.elem_type, new_type.size }, result });
	return { result, true };
}

std::pair<type::slice_reference, bool> type_set_t::add_slice_type(slice_type_t slice_type)
{
	auto const view = slice_type_view_t{ slice_type.elem_type, slice_type.is_const };
	if (auto const it = this->slice_types_map.find(view); it != this->slice_types_map.end())
	{
		return { it->second, false };
	}

	auto const result = type::slice_reference{ .index = static_cast<uint32_t>(this->slice_types.size()) };
	this->slice_types.push_back(std::move(slice_type));
	auto const &new_type = this->slice_types.back();
	this->slice_types_map.insert({ slice_type_view_t{ new_type.elem_type, new_type.is_const }, result });
	return { result, true };
}

std::pair<type::function_reference, bool> type_set_t::add_function_type(function_type_t function_type)
{
	auto const view = function_type_view_t{ function_type.return_type, function_type.param_types };
	if (auto const it = this->function_types_map.find(view); it != this->function_types_map.end())
	{
		return { it->second, false };
	}

	auto const result = type::function_reference{ .index = static_cast<uint32_t>(this->function_types.size()) };
	this->function_types.push_back(std::move(function_type));
	auto const &new_type = this->function_types.back();
	this->function_types_map.insert({ function_type_view_t{ new_type.return_type, new_type.param_types }, result });
	return { result, true };
}

void type_set_t::add_struct_type_name(type::struct_reference struct_ref, bz::u8string struct_type_name)
{
	bz_assert(struct_ref.index == this->struct_type_names.size());
	this->struct_type_names.push_back(std::move(struct_type_name));
}

void type_set_t::add_typedef_type_name(type::typedef_reference typedef_ref, bz::u8string typedef_type_name)
{
	bz_assert(typedef_ref.index == this->typedef_type_names.size());
	this->typedef_type_names.push_back(std::move(typedef_type_name));
}

void type_set_t::add_array_type_name(type::array_reference array_ref, bz::u8string array_type_name)
{
	bz_assert(array_ref.index == this->array_type_names.size());
	this->array_type_names.push_back(std::move(array_type_name));
}

void type_set_t::add_function_type_name(type::function_reference function_ref, bz::u8string function_type_name)
{
	bz_assert(function_ref.index == this->function_type_names.size());
	this->function_type_names.push_back(std::move(function_type_name));
}

type::struct_reference type_set_t::add_unique_struct(struct_type_t struct_type)
{
	auto const result = type::struct_reference{ .index = static_cast<uint32_t>(this->struct_types.size()) };
	this->struct_types.push_back(std::move(struct_type));
	return result;
}

type::typedef_reference type_set_t::add_unique_typedef(typedef_type_t typedef_type)
{
	auto const result = type::typedef_reference{ .index = static_cast<uint32_t>(this->typedef_types.size()) };
	this->typedef_types.push_back(std::move(typedef_type));
	return result;
}

void type_set_t::modify_struct(type::struct_reference struct_ref, struct_type_t struct_type)
{
	bz_assert(struct_ref.index < this->struct_types.size());
	auto &old_struct_type = this->struct_types[struct_ref.index];
	bz_assert(old_struct_type.members.empty());
	old_struct_type = std::move(struct_type);
}

} // namespace codegen::c
