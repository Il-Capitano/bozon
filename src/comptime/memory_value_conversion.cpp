#include "memory_value_conversion.h"
#include "executor_context.h"
#include "codegen_context.h"
#include "resolve/consteval.h"

namespace comptime::memory
{

template<typename T>
static void store(auto value, uint8_t *mem, endianness_kind endianness)
{
	if constexpr (bz::meta::is_same<T, bool>)
	{
		*mem = value ? 1 : 0;
	}
	else
	{
		if (is_native(endianness))
		{
			auto const actual_value = static_cast<T>(value);
			std::memcpy(mem, &actual_value, sizeof (T));
		}
		else
		{
			auto const int_value = byteswap(bit_cast<uint_t<sizeof (T)>>(static_cast<T>(value)));
			std::memcpy(mem, &int_value, sizeof (T));
		}
	}
}

template<typename T, typename U>
static void store_array(bz::array_view<U const> values, uint8_t *mem, endianness_kind endianness)
{
	if (is_native(endianness))
	{
		if constexpr (sizeof (T) == sizeof (U))
		{
			std::memcpy(mem, values.data(), values.size() * sizeof (T));
		}
		else
		{
			for (auto const i : bz::iota(0, values.size()))
			{
				auto const elem = static_cast<T>(values[i]);
				std::memcpy(mem + i * sizeof (T), &elem, sizeof (T));
			}
		}
	}
	else
	{
		for (auto const i : bz::iota(0, values.size()))
		{
			auto const int_value = byteswap(bit_cast<uint_t<sizeof (T)>>(static_cast<T>(values[i])));
			std::memcpy(mem + i * sizeof (T), &int_value, sizeof (T));
		}
	}
}

static void object_from_constant_value(
	lex::src_tokens const &src_tokens,
	ast::constant_value const &value,
	type const *object_type,
	uint8_t *mem,
	size_t current_offset,
	codegen_context &context
)
{
	auto const endianness = context.get_endianness();
	switch (value.kind())
	{
	case ast::constant_value::sint:
		bz_assert(object_type->is_integer_type());
		switch (object_type->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			store<int8_t>(value.get_sint(), mem, endianness);
			break;
		case builtin_type_kind::i16:
			store<int16_t>(value.get_sint(), mem, endianness);
			break;
		case builtin_type_kind::i32:
			store<int32_t>(value.get_sint(), mem, endianness);
			break;
		case builtin_type_kind::i64:
			store<int64_t>(value.get_sint(), mem, endianness);
			break;
		default:
			bz_unreachable;
		}
		break;
	case ast::constant_value::uint:
		bz_assert(object_type->is_integer_type());
		switch (object_type->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			store<uint8_t>(value.get_uint(), mem, endianness);
			break;
		case builtin_type_kind::i16:
			store<uint16_t>(value.get_uint(), mem, endianness);
			break;
		case builtin_type_kind::i32:
			store<uint32_t>(value.get_uint(), mem, endianness);
			break;
		case builtin_type_kind::i64:
			store<uint64_t>(value.get_uint(), mem, endianness);
			break;
		default:
			bz_unreachable;
		}
		break;
	case ast::constant_value::float32:
		bz_assert(object_type->is_floating_point_type() && object_type->get_builtin_kind() == builtin_type_kind::f32);
		store<float32_t>(value.get_float32(), mem, endianness);
		break;
	case ast::constant_value::float64:
		bz_assert(object_type->is_floating_point_type() && object_type->get_builtin_kind() == builtin_type_kind::f64);
		store<float64_t>(value.get_float64(), mem, endianness);
		break;
	case ast::constant_value::u8char:
		bz_assert(object_type->is_integer_type() && object_type->get_builtin_kind() == builtin_type_kind::i32);
		static_assert(sizeof (bz::u8char) == 4);
		store<bz::u8char>(value.get_u8char(), mem, endianness);
		break;
	case ast::constant_value::string:
	{
		auto const str = value.get_string();
		if (str.size() == 0)
		{
			std::memset(mem, 0, object_type->size);
			break;
		}

		auto &global_memory = context.global_memory;

		auto const char_array_type = context.get_array_type(context.get_builtin_type(builtin_type_kind::i8), str.size());
		auto const char_array_index = global_memory.add_object(
			src_tokens,
			char_array_type,
			bz::fixed_vector<uint8_t>(bz::array_view(str.data(), str.size()))
		);
		auto const begin_ptr = global_memory.objects[char_array_index].address;
		auto const end_ptr = global_memory.make_global_one_past_the_end_address(global_memory.objects[char_array_index].address + str.size());
		bz_assert(object_type->is_aggregate() && object_type->get_aggregate_types().size() == 2);
		auto const pointer_size = object_type->get_aggregate_types()[0]->size;
		bz_assert(object_type->size == 2 * pointer_size);
		if (pointer_size == 8)
		{
			store<uint64_t>(begin_ptr, mem, endianness);
			store<uint64_t>(end_ptr, mem + pointer_size, endianness);
		}
		else
		{
			bz_assert(pointer_size == 4);
			store<uint32_t>(begin_ptr, mem, endianness);
			store<uint32_t>(end_ptr, mem + pointer_size, endianness);
		}
		break;
	}
	case ast::constant_value::boolean:
		bz_assert(object_type->is_integer_type() && object_type->get_builtin_kind() == builtin_type_kind::i1);
		store<bool>(value.get_boolean(), mem, endianness);
		break;
	case ast::constant_value::null:
		// pointers are set to null, optionals are set to not having a value
		std::memset(mem, 0, object_type->size);
		break;
	case ast::constant_value::void_:
		bz_unreachable;
	case ast::constant_value::enum_:
		bz_assert(object_type->is_integer_type());
		switch (object_type->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			store<uint8_t>(value.get_enum().value, mem, endianness);
			break;
		case builtin_type_kind::i16:
			store<uint16_t>(value.get_enum().value, mem, endianness);
			break;
		case builtin_type_kind::i32:
			store<uint32_t>(value.get_enum().value, mem, endianness);
			break;
		case builtin_type_kind::i64:
			store<uint64_t>(value.get_enum().value, mem, endianness);
			break;
		default:
			bz_unreachable;
		}
		break;
	case ast::constant_value::array:
	{
		bz_assert(object_type->is_array());
		auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
		auto const array = value.get_array();
		bz_assert(array.size() == object_type->size / elem_type->size);
		auto const elem_size = elem_type->size;
		for (auto const &elem : array)
		{
			object_from_constant_value(src_tokens, elem, elem_type, mem, current_offset, context);
			mem += elem_size;
			current_offset += elem_size;
		}
		break;
	}
	case ast::constant_value::sint_array:
	{
		bz_assert(object_type->is_array());
		auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
		auto const array = value.get_sint_array();
		bz_assert(array.size() == object_type->size / elem_type->size);
		bz_assert(elem_type->is_integer_type());
		switch (elem_type->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			store_array<int8_t>(array, mem, endianness);
			break;
		case builtin_type_kind::i16:
			store_array<int16_t>(array, mem, endianness);
			break;
		case builtin_type_kind::i32:
			store_array<int32_t>(array, mem, endianness);
			break;
		case builtin_type_kind::i64:
			store_array<int64_t>(array, mem, endianness);
			break;
		default:
			bz_unreachable;
		}
		break;
	}
	case ast::constant_value::uint_array:
	{
		bz_assert(object_type->is_array());
		auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
		auto const array = value.get_uint_array();
		bz_assert(array.size() == object_type->size / elem_type->size);
		bz_assert(elem_type->is_integer_type());
		switch (elem_type->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			store_array<uint8_t>(array, mem, endianness);
			break;
		case builtin_type_kind::i16:
			store_array<uint16_t>(array, mem, endianness);
			break;
		case builtin_type_kind::i32:
			store_array<uint32_t>(array, mem, endianness);
			break;
		case builtin_type_kind::i64:
			store_array<uint64_t>(array, mem, endianness);
			break;
		default:
			bz_unreachable;
		}
		break;
	}
	case ast::constant_value::float32_array:
	{
		bz_assert(object_type->is_array());
		auto const array = value.get_float32_array();
		bz_assert([&]() {
			auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
			bz_assert(elem_type->is_floating_point_type() && elem_type->get_builtin_kind() == builtin_type_kind::f32);
			return array.size() == object_type->size / elem_type->size;
		}());
		store_array<float32_t>(array, mem, endianness);
		break;
	}
	case ast::constant_value::float64_array:
	{
		bz_assert(object_type->is_array());
		auto const array = value.get_float64_array();
		bz_assert([&]() {
			auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
			bz_assert(elem_type->is_floating_point_type() && elem_type->get_builtin_kind() == builtin_type_kind::f64);
			return array.size() == object_type->size / elem_type->size;
		}());
		store_array<float64_t>(array, mem, endianness);
		break;
	}
	case ast::constant_value::tuple:
	{
		auto const values = value.get_tuple();
		bz_assert(object_type->is_aggregate());
		auto const types = object_type->get_aggregate_types();
		auto const offsets = object_type->get_aggregate_offsets();
		bz_assert(types.size() == values.size());

		for (auto const i : bz::iota(0, values.size()))
		{
			object_from_constant_value(
				src_tokens,
				values[i],
				types[i],
				mem + offsets[i],
				current_offset + offsets[i],
				context
			);
		}
		break;
	}
	case ast::constant_value::function:
	{
		auto const func_body = value.get_function();
		auto const func = context.get_function(func_body);
		auto const ptr_value = context.get_function_pointer_value(func);
		bz_assert(object_type->is_pointer());
		if (object_type->size == 8)
		{
			store<uint64_t>(ptr_value, mem, endianness);
		}
		else
		{
			bz_assert(object_type->size == 4);
			store<uint32_t>(ptr_value, mem, endianness);
		}
		break;
	}
	case ast::constant_value::type:
		bz_unreachable;
	case ast::constant_value::aggregate:
	{
		auto const values = value.get_aggregate();
		bz_assert(object_type->is_aggregate());
		auto const types = object_type->get_aggregate_types();
		auto const offsets = object_type->get_aggregate_offsets();
		bz_assert(types.size() == values.size());

		for (auto const i : bz::iota(0, values.size()))
		{
			object_from_constant_value(
				src_tokens,
				values[i],
				types[i],
				mem + offsets[i],
				current_offset + offsets[i],
				context
			);
		}
		break;
	}
	default:
		bz_unreachable;
	}
}

bz::fixed_vector<uint8_t> object_from_constant_value(
	lex::src_tokens const &src_tokens,
	ast::constant_value const &value,
	type const *object_type,
	codegen_context &context
)
{
	auto result = bz::fixed_vector<uint8_t>(object_type->size);
	object_from_constant_value(
		src_tokens,
		value,
		object_type,
		result.data(),
		0,
		context
	);
	return result;
}

template<typename T>
static T load(uint8_t const *mem, endianness_kind endianness)
{
	if constexpr (bz::meta::is_same<T, bool>)
	{
		bz_assert(*mem <= 1);
		return *mem != 0;
	}
	else
	{
		if (is_native(endianness))
		{
			T result{};
			std::memcpy(&result, mem, sizeof (T));
			return result;
		}
		else
		{
			uint_t<sizeof (T)> int_result = 0;
			std::memcpy(&int_result, mem, sizeof (T));
			return bit_cast<T>(byteswap(int_result));
		}
	}
}

template<typename T, typename U>
static void load_array(uint8_t const *mem, U *dest, size_t size, endianness_kind endianness)
{
	if (is_native(endianness))
	{
		if constexpr (sizeof (T) == sizeof (U))
		{
			std::memcpy(dest, mem, size * sizeof (T));
		}
		else
		{
			for (auto const i : bz::iota(0, size))
			{
				T elem{};
				std::memcpy(&elem, mem + i * sizeof (T), sizeof (T));
				*(dest + i) = elem;
			}
		}
	}
	else
	{
		for (auto const i : bz::iota(0, size))
		{
			uint_t<sizeof (T)> int_result = 0;
			std::memcpy(&int_result, mem + i * sizeof (T), sizeof (T));
			*(dest + i) = bit_cast<T>(byteswap(int_result));
		}
	}
}

constant_value_from_object_result_t constant_value_from_object(
	type const *object_type,
	uint8_t const *mem,
	ast::typespec_view ts,
	endianness_kind endianness,
	executor_context const &context
)
{
	ts = ts.remove_any_mut();
	if (object_type->is_builtin())
	{
		if (ts.is<ast::ts_base_type>())
		{
			auto const kind = ts.get<ast::ts_base_type>().info->kind;
			switch (kind)
			{
			case ast::type_info::int8_:
				return { ast::constant_value(static_cast<int64_t>(load<int8_t>(mem, endianness))), {} };
			case ast::type_info::int16_:
				return { ast::constant_value(static_cast<int64_t>(load<int16_t>(mem, endianness))), {} };
			case ast::type_info::int32_:
				return { ast::constant_value(static_cast<int64_t>(load<int32_t>(mem, endianness))), {} };
			case ast::type_info::int64_:
				return { ast::constant_value(static_cast<int64_t>(load<int64_t>(mem, endianness))), {} };
			case ast::type_info::uint8_:
				return { ast::constant_value(static_cast<uint64_t>(load<uint8_t>(mem, endianness))), {} };
			case ast::type_info::uint16_:
				return { ast::constant_value(static_cast<uint64_t>(load<uint16_t>(mem, endianness))), {} };
			case ast::type_info::uint32_:
				return { ast::constant_value(static_cast<uint64_t>(load<uint32_t>(mem, endianness))), {} };
			case ast::type_info::uint64_:
				return { ast::constant_value(static_cast<uint64_t>(load<uint64_t>(mem, endianness))), {} };
			case ast::type_info::float32_:
				return { ast::constant_value(load<float32_t>(mem, endianness)), {} };
			case ast::type_info::float64_:
				return { ast::constant_value(load<float64_t>(mem, endianness)), {} };
			case ast::type_info::char_:
				return { ast::constant_value(load<bz::u8char>(mem, endianness)), {} };
			case ast::type_info::bool_:
				return { ast::constant_value(load<bool>(mem, endianness)), {} };
			default:
				bz_unreachable;
			}
		}
		else
		{
			bz_assert(ts.is<ast::ts_enum>());
			auto const decl = ts.get<ast::ts_enum>().decl;

			bz_assert(object_type->is_integer_type());
			switch (object_type->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return { ast::constant_value::get_enum(decl, static_cast<uint64_t>(load<uint8_t>(mem, endianness))), {} };
			case builtin_type_kind::i16:
				return { ast::constant_value::get_enum(decl, static_cast<uint64_t>(load<uint16_t>(mem, endianness))), {} };
			case builtin_type_kind::i32:
				return { ast::constant_value::get_enum(decl, static_cast<uint64_t>(load<uint32_t>(mem, endianness))), {} };
			case builtin_type_kind::i64:
				return { ast::constant_value::get_enum(decl, static_cast<uint64_t>(load<uint64_t>(mem, endianness))), {} };
			default:
				bz_unreachable;
			}
		}
	}
	else if (object_type->is_pointer())
	{
		auto const address = object_type->size == 8
			? load<uint64_t>(mem, endianness)
			: load<uint32_t>(mem, endianness);
		if (address == 0)
		{
			bz_assert(ts.is<ast::ts_optional>());
			return { ast::constant_value::get_null(), {} };
		}
		else if (ts.is<ast::ts_function>() || ts.is_optional_function())
		{
			auto const func = context.memory.global_memory->get_function_pointer(address).func;
			return { ast::constant_value(func->func_body), {} };
		}
		else
		{
			auto result = constant_value_from_object_result_t();
			result.reasons.push_back({
				{}, bz::format("a pointer of type '{}' is not a constant expression", ts)
			});
			return result;
		}
	}
	else if (object_type->is_aggregate())
	{
		if (ts.is<ast::ts_tuple>())
		{
			auto const aggregate_types = object_type->get_aggregate_types();
			auto const aggregate_offsets = object_type->get_aggregate_offsets();
			auto const tuple_types = ts.get<ast::ts_tuple>().types.as_array_view();
			bz_assert(aggregate_types.size() == tuple_types.size());

			auto result = constant_value_from_object_result_t();
			auto &result_vec = result.value.emplace<ast::constant_value::tuple>();
			result_vec.reserve(tuple_types.size());
			for (auto const i : bz::iota(0, tuple_types.size()))
			{
				auto [elem_value, elem_error_reasons] = constant_value_from_object(
					aggregate_types[i],
					mem + aggregate_offsets[i],
					tuple_types[i],
					endianness,
					context
				);
				if (elem_value.is_null())
				{
					result.value.clear();
					result.reasons.reserve(1 + elem_error_reasons.size());
					result.reasons.push_back({
						{}, bz::format("invalid value of type '{}' for element {} in tuple of type '{}'", tuple_types[i], i, ts)
					});
					result.reasons.append_move(std::move(elem_error_reasons));
					break;
				}

				result_vec.push_back(std::move(elem_value));
			}

			return result;
		}
		else if (ts.is<ast::ts_optional>())
		{
			auto const aggregate_types = object_type->get_aggregate_types();
			auto const aggregate_offsets = object_type->get_aggregate_offsets();
			bz_assert(aggregate_types.size() == 2);

			auto const has_value = load<bool>(mem + aggregate_offsets[1], endianness);
			if (has_value)
			{
				return constant_value_from_object(aggregate_types[0], mem, ts.get<ast::ts_optional>(), endianness, context);
			}
			else
			{
				return { ast::constant_value::get_null(), {} };
			}
		}
		else if (ts.is<ast::ts_base_type>())
		{
			auto const info = ts.get<ast::ts_base_type>().info;
			if (info->kind != ast::type_info::aggregate)
			{
				// str or __null_t
				if (info->kind == ast::type_info::null_t_)
				{
					return { ast::constant_value::get_null(), {} };
				}
				else
				{
					bz_assert(info->kind == ast::type_info::str_);
					bz_assert(object_type->is_aggregate());
					auto const pointer_size = object_type->size / 2;
					bz_assert(pointer_size == object_type->get_aggregate_types()[0]->size);
					auto const begin_ptr = pointer_size == 8
						? load<uint64_t>(mem, endianness)
						: load<uint32_t>(mem, endianness);
					auto const end_ptr = pointer_size == 8
						? load<uint64_t>(mem + pointer_size, endianness)
						: load<uint32_t>(mem + pointer_size, endianness);

					if (begin_ptr == 0 && end_ptr == 0)
					{
						return { ast::constant_value(bz::u8string()), {} };
					}
					else if (context.memory.is_global(begin_ptr))
					{
						return {
							ast::constant_value(bz::u8string_view(
								context.memory.get_memory(begin_ptr),
								context.memory.get_memory(end_ptr)
							)),
							{}
						};
					}
					else
					{
						auto const elem_type = context.codegen_ctx->get_builtin_type(builtin_type_kind::i8);
						return {
							ast::constant_value(),
							context.memory.get_slice_construction_error_reason(begin_ptr, end_ptr, elem_type)
						};
					}
				}
			}

			auto const aggregate_types = object_type->get_aggregate_types();
			auto const aggregate_offsets = object_type->get_aggregate_offsets();
			auto const members = info->member_variables.as_array_view();
			bz_assert(aggregate_types.size() == members.size());

			auto result = constant_value_from_object_result_t();
			auto &result_vec = result.value.emplace<ast::constant_value::aggregate>();
			result_vec.reserve(members.size());
			for (auto const i : bz::iota(0, members.size()))
			{
				auto [elem_value, elem_error_reasons] = constant_value_from_object(
					aggregate_types[i],
					mem + aggregate_offsets[i],
					members[i]->get_type(),
					endianness,
					context
				);
				if (elem_value.is_null())
				{
					result.value.clear();
					result.reasons.reserve(1 + elem_error_reasons.size());
					result.reasons.push_back({
						{}, bz::format(
							"invalid value of type '{}' for member '{}' in type '{}'",
							members[i]->get_type(), members[i]->get_id().format_as_unqualified(), ts
						)
					});
					result.reasons.append_move(std::move(elem_error_reasons));
					break;
				}

				result_vec.push_back(std::move(elem_value));
			}

			return result;
		}
		else
		{
			// array slice
			auto result = constant_value_from_object_result_t();
			result.reasons.push_back({
				{}, bz::format("an array slice of type '{}' is not a constant expression", ts)
			});
			return result;
		}
	}
	else if (object_type->is_array())
	{
		bz_assert(ts.is<ast::ts_array>());
		auto const info = resolve::get_flattened_array_type_and_size(ts);
		if (resolve::is_special_array_type(ts))
		{
			bz_assert(info.elem_type.is<ast::ts_base_type>());
			bz_assert(get_multi_dimensional_array_elem_type(object_type)->size * info.size == object_type->size);
			auto const kind = info.elem_type.get<ast::ts_base_type>().info->kind;

			auto result = constant_value_from_object_result_t();
			switch (kind)
			{
			case ast::type_info::int8_:
			{
				auto &result_array = result.value.emplace<ast::constant_value::sint_array>(info.size);
				load_array<int8_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::int16_:
			{
				auto &result_array = result.value.emplace<ast::constant_value::sint_array>(info.size);
				load_array<int16_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::int32_:
			{
				auto &result_array = result.value.emplace<ast::constant_value::sint_array>(info.size);
				load_array<int32_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::int64_:
			{
				auto &result_array = result.value.emplace<ast::constant_value::sint_array>(info.size);
				load_array<int64_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::uint8_:
			{
				auto &result_array = result.value.emplace<ast::constant_value::uint_array>(info.size);
				load_array<uint8_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::uint16_:
			{
				auto &result_array = result.value.emplace<ast::constant_value::uint_array>(info.size);
				load_array<uint16_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::uint32_:
			{
				auto &result_array = result.value.emplace<ast::constant_value::uint_array>(info.size);
				load_array<uint32_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::uint64_:
			{
				auto &result_array = result.value.emplace<ast::constant_value::uint_array>(info.size);
				load_array<uint64_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::float32_:
			{
				auto &result_array = result.value.emplace<ast::constant_value::float32_array>(info.size);
				load_array<float32_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::float64_:
			{
				auto &result_array = result.value.emplace<ast::constant_value::float64_array>(info.size);
				load_array<float64_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			default:
				bz_unreachable;
			}

			return result;
		}
		else
		{
			auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
			bz_assert(elem_type->size * info.size == object_type->size);

			auto result = constant_value_from_object_result_t();
			auto &result_array = result.value.emplace<ast::constant_value::array>();
			result_array.reserve(info.size);

			auto mem_it = mem;
			auto const mem_end = mem + object_type->size;
			for (; mem_it != mem_end; mem_it += elem_type->size)
			{
				auto [elem_value, elem_error_reasons] = constant_value_from_object(
					elem_type,
					mem_it,
					info.elem_type,
					endianness,
					context
				);
				if (elem_value.is_null())
				{
					result.value.clear();
					auto index = static_cast<size_t>(mem_it - mem) / elem_type->size;
					auto total_size = info.size;
					auto array_ts = ts;
					do
					{
						auto const &[size, elem_type] = array_ts.get<ast::ts_array>();
						total_size /= size;
						auto const elem_index = index / total_size;
						index %= total_size;
						result.reasons.push_back({
							{}, bz::format(
								"invalid value of type '{}' for element {} in array of type '{}'",
								elem_type, elem_index, array_ts
							)
						});
						array_ts = elem_type;
					} while (array_ts.is<ast::ts_array>());
					result.reasons.append_move(std::move(elem_error_reasons));
					break;
				}

				result_array.push_back(std::move(elem_value));
			}

			return result;
		}
	}
	else
	{
		bz_unreachable;
	}
}

} // namespace comptime::memory
