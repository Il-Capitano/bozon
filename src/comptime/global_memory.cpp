#include "global_memory.h"

namespace comptime::memory
{

global_object::global_object(lex::src_tokens const &_object_src_tokens, ptr_t _address, type const *_object_type, bz::fixed_vector<uint8_t> data)
	: address(_address),
	  object_type(_object_type),
	  memory(std::move(data)),
	  object_src_tokens(_object_src_tokens)
{}

size_t global_object::object_size(void) const
{
	return this->memory.size();
}

uint8_t *global_object::get_memory(ptr_t address)
{
	bz_assert(address >= this->address && address <= this->address + this->object_size());
	bz_assert(this->memory.not_empty());
	return this->memory.data() + (address - this->address);
}

uint8_t const *global_object::get_memory(ptr_t address) const
{
	bz_assert(address >= this->address && address <= this->address + this->object_size());
	bz_assert(this->memory.not_empty());
	return this->memory.data() + (address - this->address);
}

bool global_object::check_dereference(ptr_t address, type const *subobject_type) const
{
	if (address < this->address || address >= this->address + this->object_size() || this->memory.empty())
	{
		return false;
	}

	auto const offset = address - this->address;
	bz_assert(contained_in_object(this->object_type, offset, subobject_type));
	return true;
}

bz::u8string global_object::get_dereference_error_reason(ptr_t, type const *) const
{
	// this shouldn't ever happen, because the only kind of invalid memory access
	// into global objects is a one-past-the-end pointer dereference, which is handled
	// much earlier as a meta pointer
	bz_unreachable;
	return {};
}

bool global_object::check_slice_construction(ptr_t begin, ptr_t end, bool end_is_one_past_the_end, type const *elem_type) const
{
	if (begin == end)
	{
		return true;
	}

	auto const total_size = end - begin;
	bz_assert(total_size % elem_type->size == 0);
	auto const offset = begin - this->address;

	return slice_contained_in_object(this->object_type, offset, elem_type, total_size, end_is_one_past_the_end);
}

bz::vector<bz::u8string> global_object::get_slice_construction_error_reason(
	ptr_t begin,
	ptr_t end,
	bool end_is_one_past_the_end,
	type const *
) const
{
	auto const begin_offset = begin - this->address;
	auto const end_offset = end - this->address;

	bz::vector<bz::u8string> result;
	result.reserve(3);
	result.push_back("begin and end addresses point to different subobjects in this global object");
	bz_assert(begin != end);
	result.push_back(bz::format("begin address points to a subobject at offset {}", begin_offset));
	if (end_is_one_past_the_end)
	{
		result.push_back(bz::format("end address is a one-past-the-end pointer with offset {}", end_offset));
	}
	else
	{
		result.push_back(bz::format("end address points to a subobject at offset {}", end_offset));
	}
	return result;
}

pointer_arithmetic_result_t global_object::do_pointer_arithmetic(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t amount,
	type const *pointer_type
) const
{
	auto const result_address = address + static_cast<uint64_t>(amount * static_cast<int64_t>(pointer_type->size));
	if (result_address < this->address || result_address > this->address + this->object_size())
	{
		return {
			.address = 0,
			.is_one_past_the_end = false,
		};
	}
	else if (address == result_address)
	{
		return {
			.address = address,
			.is_one_past_the_end = is_one_past_the_end,
		};
	}

	auto const check_result = check_pointer_arithmetic(
		this->object_type,
		address - this->address,
		result_address - this->address,
		is_one_past_the_end,
		pointer_type
	);
	switch (check_result)
	{
	case pointer_arithmetic_check_result::fail:
		return {
			.address = 0,
			.is_one_past_the_end = false,
		};
	case pointer_arithmetic_check_result::good:
		return {
			.address = result_address,
			.is_one_past_the_end = false,
		};
	case pointer_arithmetic_check_result::one_past_the_end:
		return {
			.address = result_address,
			.is_one_past_the_end = true,
		};
	}
}

pointer_arithmetic_result_t global_object::do_pointer_arithmetic_unchecked(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t amount,
	type const *pointer_type
) const
{
	auto const result_address = address + static_cast<uint64_t>(amount * static_cast<int64_t>(pointer_type->size));
	bz_assert(result_address >= this->address && result_address <= this->address + this->object_size());
	if (address == result_address)
	{
		return {
			.address = address,
			.is_one_past_the_end = is_one_past_the_end,
		};
	}

	auto const check_result = check_pointer_arithmetic(
		this->object_type,
		address - this->address,
		result_address - this->address,
		is_one_past_the_end,
		pointer_type
	);
	bz_assert(check_result != pointer_arithmetic_check_result::fail);
	return {
		.address = result_address,
		.is_one_past_the_end = check_result == pointer_arithmetic_check_result::one_past_the_end,
	};
}

bz::optional<int64_t> global_object::do_pointer_difference(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *object_type
) const
{
	if (lhs == rhs)
	{
		return 0;
	}
	else if (lhs < rhs)
	{
		auto const slice_check = this->check_slice_construction(lhs, rhs, rhs_is_one_past_the_end, object_type);
		if (slice_check)
		{
			return static_cast<int64_t>((lhs - rhs) / object_type->size);
		}
		else
		{
			return {};
		}
	}
	else
	{
		auto const slice_check = this->check_slice_construction(rhs, lhs, lhs_is_one_past_the_end, object_type);
		if (slice_check)
		{
			return -static_cast<int64_t>((rhs - lhs) / object_type->size);
		}
		else
		{
			return {};
		}
	}
}

copy_values_memory_t global_object::get_copy_source_memory(ptr_t address, size_t count, type const *elem_type)
{
	auto const begin_offset = address - this->address;
	auto const end_offset = begin_offset + count * elem_type->size;
	auto const check_result = check_pointer_arithmetic(
		this->object_type,
		begin_offset,
		end_offset,
		false,
		elem_type
	);

	if (check_result == pointer_arithmetic_check_result::fail)
	{
		return {};
	}
	else
	{
		return { this->memory.slice(begin_offset, end_offset) };
	}
}

bz::vector<bz::u8string> global_object::get_get_copy_source_memory_error_reasons(ptr_t address, size_t count, type const *elem_type)
{
	bz::vector<bz::u8string> result;

	auto const begin_offset = address - this->address;
	auto const end_offset = begin_offset + count * elem_type->size;
	auto const check_result = check_pointer_arithmetic(
		this->object_type,
		begin_offset,
		end_offset,
		false,
		elem_type
	);

	if (check_result == pointer_arithmetic_check_result::fail)
	{
		result.push_back(bz::format(
			"source address points to an invalid memory range in this global object with offset {} and element count {}",
			begin_offset, count
		));
	}

	return result;
}

global_memory_manager::global_memory_manager(ptr_t global_memory_begin)
	: segment_info{},
	  head(0),
	  one_past_the_end_pointers(),
	  objects()
{
	constexpr size_t segment_count = global_meta_segment_info_t::segment_count;
	constexpr size_t segment_size = 1u << 16;
	for (size_t i = 0; i < segment_count; ++i)
	{
		this->segment_info.segment_begins[i] = global_memory_begin + i * segment_size;
	}
	this->head = this->segment_info.get_segment_begin<global_meta_memory_segment::objects>();
}

ptr_t global_memory_manager::make_global_one_past_the_end_address(ptr_t address)
{
	auto const result_index = this->one_past_the_end_pointers.size();
	this->one_past_the_end_pointers.push_back({ address });
	return this->segment_info.get_segment_begin<global_meta_memory_segment::one_past_the_end>() + result_index;
}

global_memory_manager::one_past_the_end_pointer const &global_memory_manager::get_one_past_the_end_pointer(ptr_t address) const
{
	auto const index = address - this->segment_info.get_segment_begin<global_meta_memory_segment::one_past_the_end>();
	bz_assert(index < this->one_past_the_end_pointers.size());
	return this->one_past_the_end_pointers[index];
}

ptr_t global_memory_manager::make_unique_function_pointer(function *func)
{
	auto const result_index = this->function_pointers.size();
	bz_assert(this->function_pointers.is_all([func](auto const fp) { return fp.func != func; }));
	this->function_pointers.push_back({ func });
	return this->segment_info.get_segment_begin<global_meta_memory_segment::functions>() + result_index;
}

global_memory_manager::function_pointer const &global_memory_manager::get_function_pointer(ptr_t address) const
{
	auto const index = address - this->segment_info.get_segment_begin<global_meta_memory_segment::functions>();
	bz_assert(index < this->function_pointers.size());
	return this->function_pointers[index];
}

uint32_t global_memory_manager::add_object(lex::src_tokens const &object_src_tokens, type const *object_type, bz::fixed_vector<uint8_t> data)
{
	auto const result = static_cast<uint32_t>(this->objects.size());
	this->objects.emplace_back(object_src_tokens, this->head, object_type, std::move(data));
	this->head += object_type->size;
	this->head += (max_object_align - this->head % max_object_align);
	return result;
}

global_object *global_memory_manager::get_global_object(ptr_t address)
{
	if (
		this->objects.empty()
		|| address < this->objects[0].address
		|| address > this->objects.back().address + this->objects.back().object_size()
	)
	{
		return nullptr;
	}

	// find the last element that has an address that is less than or equal to address.
	// we do this by finding the first element, whose address is greater than address
	// and taking the element before that
	auto const it = std::upper_bound(
		this->objects.begin(), this->objects.end(),
		address,
		[](ptr_t address, auto const &object) {
			return address < object.address;
		}
	);
	return &*(it - 1);
}

global_object const *global_memory_manager::get_global_object(ptr_t address) const
{
	if (
		this->objects.empty()
		|| address < this->objects[0].address
		|| address > this->objects.back().address + this->objects.back().object_size()
	)
	{
		return nullptr;
	}

	// find the last element that has an address that is less than or equal to address.
	// we do this by finding the first element, whose address is greater than address
	// and taking the element before that
	auto const it = std::upper_bound(
		this->objects.begin(), this->objects.end(),
		address,
		[](ptr_t address, auto const &object) {
			return address < object.address;
		}
	);
	return &*(it - 1);
}

bool global_memory_manager::check_dereference(ptr_t address, type const *object_type) const
{
	auto const object = this->get_global_object(address);
	return object != nullptr && object->check_dereference(address, object_type);
}

bz::vector<error_reason_t> global_memory_manager::get_dereference_error_reason(ptr_t address, type const *object_type) const
{
	auto const object = this->get_global_object(address);
	bz_assert(object != nullptr);
	bz::vector<error_reason_t> result;
	result.push_back({ object->object_src_tokens, object->get_dereference_error_reason(address, object_type) });
	return result;
}

bool global_memory_manager::check_slice_construction(ptr_t begin, ptr_t end, bool end_is_one_past_the_end, type const *elem_type) const
{
	bz_assert(begin <= end);
	auto const object = this->get_global_object(begin);
	if (object == nullptr)
	{
		return false;
	}
	else if (end > object->address + object->object_size())
	{
		return false;
	}
	else
	{
		return object->check_slice_construction(begin, end, end_is_one_past_the_end, elem_type);
	}
}

bz::vector<error_reason_t> global_memory_manager::get_slice_construction_error_reason(
	ptr_t begin,
	ptr_t end,
	bool end_is_one_past_the_end,
	type const *elem_type
) const
{
	bz_assert(begin <= end);
	auto const begin_global_object = this->get_global_object(begin);
	auto const end_global_object   = this->get_global_object(end);
	bz_assert(begin_global_object != nullptr);
	bz_assert(end_global_object != nullptr);

	bz::vector<error_reason_t> result;
	if (begin_global_object == end_global_object)
	{
		auto messages = begin_global_object->get_slice_construction_error_reason(begin, end, end_is_one_past_the_end, elem_type);
		result.reserve(messages.size());
		result.append(messages.transform([begin_global_object](auto const &message) {
			return error_reason_t{ begin_global_object->object_src_tokens, message };
		}));
	}
	else
	{
		result.reserve(3);
		result.push_back({ {}, "begin and end addresses point to different global objects" });
		result.push_back({ begin_global_object->object_src_tokens, "begin address points to this global object" });
		result.push_back({ end_global_object->object_src_tokens, "end address points to this global object" });
	}

	return result;
}

bz::optional<int> global_memory_manager::compare_pointers(ptr_t lhs, ptr_t rhs) const
{
	auto const lhs_segment = this->segment_info.get_segment(lhs);
	auto const rhs_segment = this->segment_info.get_segment(rhs);

	if (lhs_segment != rhs_segment)
	{
		return {};
	}

	switch (lhs_segment)
	{
	case global_meta_memory_segment::one_past_the_end:
		bz_unreachable;
	case global_meta_memory_segment::functions:
		if (lhs == rhs)
		{
			return 0;
		}
		else
		{
			return {};
		}
	case global_meta_memory_segment::objects:
	{
		auto const lhs_global_object = this->get_global_object(lhs);
		if (lhs_global_object == nullptr)
		{
			return {};
		}
		else if (rhs < lhs_global_object->address || rhs > lhs_global_object->address + lhs_global_object->object_size())
		{
			return {};
		}
		else
		{
			return lhs == rhs ? 0 : (lhs < rhs ? -1 : 1);
		}
	}
	}
}

pointer_arithmetic_result_t global_memory_manager::do_pointer_arithmetic(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t offset,
	type const *object_type
) const
{
	auto const object = this->get_global_object(address);
	if (object == nullptr)
	{
		return { 0, false };
	}
	else
	{
		return object->do_pointer_arithmetic(address, is_one_past_the_end, offset, object_type);
	}
}

pointer_arithmetic_result_t global_memory_manager::do_pointer_arithmetic_unchecked(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t offset,
	type const *object_type
) const
{
	auto const object = this->get_global_object(address);
	bz_assert(object != nullptr);
	return object->do_pointer_arithmetic_unchecked(address, is_one_past_the_end, offset, object_type);
}

bz::optional<int64_t> global_memory_manager::do_pointer_difference(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *object_type
) const
{
	auto const object = this->get_global_object(lhs);
	if (object == nullptr)
	{
		return {};
	}
	else if (rhs < object->address || rhs > object->address + object->object_size())
	{
		return {};
	}
	else
	{
		return object->do_pointer_difference(lhs, rhs, lhs_is_one_past_the_end, rhs_is_one_past_the_end, object_type);
	}
}

uint8_t *global_memory_manager::get_memory(ptr_t address)
{
	auto const object = this->get_global_object(address);
	bz_assert(object != nullptr);
	return object->get_memory(address);
}

uint8_t const *global_memory_manager::get_memory(ptr_t address) const
{
	auto const object = this->get_global_object(address);
	bz_assert(object != nullptr);
	return object->get_memory(address);
}

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
			auto const int_value = byteswap(bit_cast<uint_t<sizeof  (T)>>(static_cast<T>(value)));
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
	endianness_kind endianness,
	size_t current_offset,
	global_memory_manager &manager,
	type_set_t &type_set
)
{
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

		auto const char_array_type = type_set.get_array_type(type_set.get_builtin_type(builtin_type_kind::i8), str.size());
		auto const char_array_index = manager.add_object(
			src_tokens,
			char_array_type,
			bz::fixed_vector<uint8_t>(bz::array_view(str.data(), str.size()))
		);
		auto const begin_ptr = manager.objects[char_array_index].address;
		auto const end_ptr = manager.make_global_one_past_the_end_address(manager.objects[char_array_index].address + str.size());
		bz_assert(object_type->is_aggregate() && object_type->get_aggregate_types().size() == 2);
		auto const pointer_size = object_type->get_aggregate_types()[0]->size;
		bz_assert(object_type->size == 2 * pointer_size);
		if (pointer_size == 8)
		{
			auto const begin_ptr64 = is_native(endianness)
				? static_cast<uint64_t>(begin_ptr)
				: memory::byteswap(static_cast<uint64_t>(begin_ptr));
			auto const end_ptr64 = is_native(endianness)
				? static_cast<uint64_t>(end_ptr)
				: memory::byteswap(static_cast<uint64_t>(end_ptr));
			std::memcpy(mem, &begin_ptr64, sizeof begin_ptr64);
			std::memcpy(mem + 8, &end_ptr64, sizeof end_ptr64);
		}
		else
		{
			auto const begin_ptr32 = is_native(endianness)
				? static_cast<uint32_t>(begin_ptr)
				: memory::byteswap(static_cast<uint32_t>(begin_ptr));
			auto const end_ptr32 = is_native(endianness)
				? static_cast<uint32_t>(end_ptr)
				: memory::byteswap(static_cast<uint32_t>(end_ptr));
			std::memcpy(mem, &begin_ptr32, sizeof begin_ptr32);
			std::memcpy(mem + 4, &end_ptr32, sizeof end_ptr32);
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
			object_from_constant_value(src_tokens, elem, elem_type, mem, endianness, current_offset, manager, type_set);
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
		auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
		auto const array = value.get_float32_array();
		bz_assert(array.size() == object_type->size / elem_type->size);
		bz_assert(elem_type->is_floating_point_type() && elem_type->get_builtin_kind() == builtin_type_kind::f32);
		store_array<float32_t>(array, mem, endianness);
		break;
	}
	case ast::constant_value::float64_array:
	{
		bz_assert(object_type->is_array());
		auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
		auto const array = value.get_float64_array();
		bz_assert(array.size() == object_type->size / elem_type->size);
		bz_assert(elem_type->is_floating_point_type() && elem_type->get_builtin_kind() == builtin_type_kind::f64);
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
				endianness,
				current_offset + offsets[i],
				manager,
				type_set
			);
		}
		break;
	}
	case ast::constant_value::function:
		bz_unreachable; // TODO
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
				endianness,
				current_offset + offsets[i],
				manager,
				type_set
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
	endianness_kind endianness,
	global_memory_manager &manager,
	type_set_t &type_set
)
{
	auto result = bz::fixed_vector<uint8_t>(object_type->size);
	object_from_constant_value(
		src_tokens,
		value,
		object_type,
		result.data(),
		endianness,
		0,
		manager,
		type_set
	);
	return result;
}

} // namespace comptime::memory
