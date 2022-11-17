#include "global_codegen_context.h"

namespace comptime
{

global_codegen_context::global_codegen_context(size_t pointer_size)
	: type_set(pointer_size)
{
	type const * const pointer_pair[] = { this->type_set.get_pointer_type(), this->type_set.get_pointer_type() };
	this->pointer_pair_t = this->type_set.get_aggregate_type(pointer_pair);
	this->null_t = this->type_set.get_aggregate_type({});
}

type const *global_codegen_context::get_builtin_type(builtin_type_kind kind)
{
	return this->type_set.get_builtin_type(kind);
}

type const *global_codegen_context::get_pointer_type(void)
{
	return this->type_set.get_pointer_type();
}

type const *global_codegen_context::get_aggregate_type(bz::array_view<type const * const> elem_types)
{
	return this->type_set.get_aggregate_type(elem_types);
}

type const *global_codegen_context::get_array_type(type const *elem_type, size_t size)
{
	return this->type_set.get_array_type(elem_type, size);
}

type const *global_codegen_context::get_str_t(void)
{
	return this->pointer_pair_t;
}

type const *global_codegen_context::get_null_t(void)
{
	return this->null_t;
}

type const *global_codegen_context::get_slice_t(void)
{
	return this->pointer_pair_t;
}

type const *global_codegen_context::get_optional_type(type const *value_type)
{
	type const * const types[] = { value_type, this->get_builtin_type(builtin_type_kind::i1) };
	return this->get_aggregate_type(types);
}

} // namespace comptime
