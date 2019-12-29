#include "bytecode.h"

void stack::change_size(int64_t amount)
{
	if (amount < 0)
	{
		assert(static_cast<size_t>(-amount) <= this->stack.size());
		this->stack.resize(this->stack.size() + amount);
	}
	else
	{
		this->stack.resize(this->stack.size() + amount);
	}
}

namespace bytecode
{

void move_stack_pointer::execute(stack &stack)
{
	stack.change_size(this->amount);
}

#define type_switch(type, o)                \
switch (type)                               \
{                                           \
case type_kind::int8: o(int8_t); break;     \
case type_kind::int16: o(int16_t); break;   \
case type_kind::int32: o(int32_t); break;   \
case type_kind::int64: o(int64_t); break;   \
case type_kind::uint8: o(uint8_t); break;   \
case type_kind::uint16: o(uint16_t); break; \
case type_kind::uint32: o(uint32_t); break; \
case type_kind::uint64: o(uint64_t); break; \
case type_kind::float32: o(float); break;   \
case type_kind::float64: o(double); break;  \
default: assert(false); break;              \
}

#define integer_type_switch(type, o)        \
switch (type)                               \
{                                           \
case type_kind::int8: o(int8_t); break;     \
case type_kind::int16: o(int16_t); break;   \
case type_kind::int32: o(int32_t); break;   \
case type_kind::int64: o(int64_t); break;   \
case type_kind::uint8: o(uint8_t); break;   \
case type_kind::uint16: o(uint16_t); break; \
case type_kind::uint32: o(uint32_t); break; \
case type_kind::uint64: o(uint64_t); break; \
default: assert(false); break;              \
}

void add::execute(stack &stack)
{
#define o(x) {                                                          \
    auto const lhs = stack.value_at_offset<x>(this->lhs_offset);        \
    auto const rhs = stack.value_at_offset<x>(this->rhs_offset);        \
    stack.store_at_offset(this->res_offset, static_cast<x>(lhs + rhs)); \
}
	type_switch(this->type, o)
#undef o
}

void sub::execute(stack &stack)
{
#define o(x) {                                                          \
    auto const lhs = stack.value_at_offset<x>(this->lhs_offset);        \
    auto const rhs = stack.value_at_offset<x>(this->rhs_offset);        \
    stack.store_at_offset(this->res_offset, static_cast<x>(lhs - rhs)); \
}
	type_switch(this->type, o)
#undef o
}

void mul::execute(stack &stack)
{
#define o(x) {                                                          \
    auto const lhs = stack.value_at_offset<x>(this->lhs_offset);        \
    auto const rhs = stack.value_at_offset<x>(this->rhs_offset);        \
    stack.store_at_offset(this->res_offset, static_cast<x>(lhs * rhs)); \
}
	type_switch(this->type, o)
#undef o
}

void div::execute(stack &stack)
{
#define o(x) {                                                          \
    auto const lhs = stack.value_at_offset<x>(this->lhs_offset);        \
    auto const rhs = stack.value_at_offset<x>(this->rhs_offset);        \
    stack.store_at_offset(this->res_offset, static_cast<x>(lhs / rhs)); \
}
	type_switch(this->type, o)
#undef o
}

void mod::execute(stack &stack)
{
#define o(x) {                                                          \
    auto const lhs = stack.value_at_offset<x>(this->lhs_offset);        \
    auto const rhs = stack.value_at_offset<x>(this->rhs_offset);        \
    stack.store_at_offset(this->res_offset, static_cast<x>(lhs % rhs)); \
}
	integer_type_switch(this->type, o)
#undef o
}

void cast::execute(stack &stack)
{
#define o(from_t)                                                                                                                               \
switch (this->dest_type)                                                                                                                        \
{                                                                                                                                               \
case type_kind::int8: stack.store_at_offset(this->res_offset, static_cast<int8_t>(stack.value_at_offset<from_t>(this->arg_offset))); break;     \
case type_kind::int16: stack.store_at_offset(this->res_offset, static_cast<int16_t>(stack.value_at_offset<from_t>(this->arg_offset))); break;   \
case type_kind::int32: stack.store_at_offset(this->res_offset, static_cast<int32_t>(stack.value_at_offset<from_t>(this->arg_offset))); break;   \
case type_kind::int64: stack.store_at_offset(this->res_offset, static_cast<int64_t>(stack.value_at_offset<from_t>(this->arg_offset))); break;   \
case type_kind::uint8: stack.store_at_offset(this->res_offset, static_cast<uint8_t>(stack.value_at_offset<from_t>(this->arg_offset))); break;   \
case type_kind::uint16: stack.store_at_offset(this->res_offset, static_cast<uint16_t>(stack.value_at_offset<from_t>(this->arg_offset))); break; \
case type_kind::uint32: stack.store_at_offset(this->res_offset, static_cast<uint32_t>(stack.value_at_offset<from_t>(this->arg_offset))); break; \
case type_kind::uint64: stack.store_at_offset(this->res_offset, static_cast<uint64_t>(stack.value_at_offset<from_t>(this->arg_offset))); break; \
case type_kind::float32: stack.store_at_offset(this->res_offset, static_cast<float>(stack.value_at_offset<from_t>(this->arg_offset))); break;   \
case type_kind::float64: stack.store_at_offset(this->res_offset, static_cast<double>(stack.value_at_offset<from_t>(this->arg_offset))); break;  \
default: assert(false); break;                                                                                                                  \
}
	type_switch(this->src_type, o)
#undef o
}

void mov::execute(stack &stack)
{
	auto down_cast = [](int64_t value, auto x) -> decltype(x)
	{
		using T = decltype(x);
		if constexpr (sizeof(T) == 1)
		{
			return bit_cast<T>(static_cast<uint8_t> (value & 0xff));
		}
		else if constexpr (sizeof(T) == 2)
		{
			return bit_cast<T>(static_cast<uint16_t>(value & 0xffff));
		}
		else if constexpr (sizeof(T) == 4)
		{
			return bit_cast<T>(static_cast<uint32_t>(value & 0xffff'ffff));
		}
		else
		{
			return bit_cast<T>(value);
		}
	};
#define o(x) stack.store_at_offset(this->offset, down_cast(this->value, x{}))
	type_switch(this->type, o)
#undef o
}

} // namespace bytecode


void execute(bz::vector<instruction> const &instructions, stack &stack);

void execute_new(bz::vector<instruction> const &instructions)
{
	stack stack;
	execute(instructions, stack);
}

template<typename ...Ts>
struct overload : Ts...
{
	using Ts::operator ()...;
};
template<typename ...Ts>
overload(Ts...) -> overload<Ts...>;

void execute(bz::vector<instruction> const &instructions, stack &stack)
{
	for (auto inst : instructions)
	{
		inst.visit([&](auto i) { return i.execute(stack); });
	}

	int i = 0;
	for (auto v : stack.stack)
	{
		if (i == 4)
		{
			bz::print(" ");
			i = 0;
		}
		++i;

		bz::printf("{:02x}", v);
	}
	bz::print("\n");
}

void bytecode_test(void)
{
	bz::vector<instruction> instructions = {
		bytecode::move_stack_pointer{16},

		bytecode::mov{-16, bytecode::type_kind::int32, 0x5},
		bytecode::mov{-12,  bytecode::type_kind::int32, 0x3},
		bytecode::mul{-8, -12, -16, bytecode::type_kind::int32},
		bytecode::cast{-4, -8, bytecode::type_kind::int32, bytecode::type_kind::float32},

//		bytecode::move_stack_pointer{-16},
	};

	execute_new(instructions);
}
