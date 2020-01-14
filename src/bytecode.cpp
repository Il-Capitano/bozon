#include "bytecode.h"

namespace bytecode
{

void executor::execute(bz::vector<instruction> const &instructions)
{
	for (auto &inst : instructions)
	{
		inst.execute(*this);
	}
}

void mov::execute(executor &exec) const
{
#define CASE(type)                                  \
case type_kind::type:                               \
{                                                   \
    auto val = this->src.get_value<type##_t>(exec); \
    this->dest.store_value(val, exec);              \
    break;                                          \
}
	switch (this->type)
	{
	CASE(int8)
	CASE(int16)
	CASE(int32)
	CASE(int64)
	CASE(uint8)
	CASE(uint16)
	CASE(uint32)
	CASE(uint64)
	CASE(float32)
	CASE(float64)
	CASE(ptr)
	}
#undef CASE
}

void add::execute(executor &exec) const
{
#define CASE(type)                                                                            \
case type_kind::type:                                                                         \
{                                                                                             \
    type##_t val = this->lhs.get_value<type##_t>(exec) + this->rhs.get_value<type##_t>(exec); \
    this->res.store_value(val, exec);                                                         \
    break;                                                                                    \
}
	switch (this->type)
	{
	CASE(int8)
	CASE(int16)
	CASE(int32)
	CASE(int64)
	CASE(uint8)
	CASE(uint16)
	CASE(uint32)
	CASE(uint64)
	CASE(float32)
	CASE(float64)
	case type_kind::ptr: assert(false); break;
	}
#undef CASE
}

void sub::execute(executor &exec) const
{
#define CASE(type)                                                                            \
case type_kind::type:                                                                         \
{                                                                                             \
    type##_t val = this->lhs.get_value<type##_t>(exec) - this->rhs.get_value<type##_t>(exec); \
    this->res.store_value(val, exec);                                                         \
    break;                                                                                    \
}
	switch (this->type)
	{
	CASE(int8)
	CASE(int16)
	CASE(int32)
	CASE(int64)
	CASE(uint8)
	CASE(uint16)
	CASE(uint32)
	CASE(uint64)
	CASE(float32)
	CASE(float64)
	case type_kind::ptr: assert(false); break;
	}
#undef CASE
}

void mul::execute(executor &exec) const
{
#define CASE(type)                                                                            \
case type_kind::type:                                                                         \
{                                                                                             \
    type##_t val = this->lhs.get_value<type##_t>(exec) * this->rhs.get_value<type##_t>(exec); \
    this->res.store_value(val, exec);                                                         \
    break;                                                                                    \
}
	switch (this->type)
	{
	CASE(int8)
	CASE(int16)
	CASE(int32)
	CASE(int64)
	CASE(uint8)
	CASE(uint16)
	CASE(uint32)
	CASE(uint64)
	CASE(float32)
	CASE(float64)
	case type_kind::ptr: assert(false); break;
	}
#undef CASE
}

void div::execute(executor &exec) const
{
#define CASE(type)                                                                            \
case type_kind::type:                                                                         \
{                                                                                             \
    type##_t val = this->lhs.get_value<type##_t>(exec) / this->rhs.get_value<type##_t>(exec); \
    this->res.store_value(val, exec);                                                         \
    break;                                                                                    \
}
	switch (this->type)
	{
	CASE(int8)
	CASE(int16)
	CASE(int32)
	CASE(int64)
	CASE(uint8)
	CASE(uint16)
	CASE(uint32)
	CASE(uint64)
	CASE(float32)
	CASE(float64)
	case type_kind::ptr: assert(false); break;
	}
#undef CASE
}


void instruction::execute(executor &exec) const
{
	this->visit([&exec](auto const &inst) { inst.execute(exec); });
}


} // namespace bytecode


void bytecode_test(void)
{
	using namespace bytecode;

	executor exec;
	bz::vector<instruction> instructions = {
		mov{ 1, register_value{ ._uint64 = 123 }, type_kind::uint64 },
		mov{ 0, 1, type_kind::uint64 },
		sub{ 2, 0, 1, type_kind::uint64 },
	};

	assert(exec.registers[0]._uint64 == 0);
	exec.execute(instructions);
	assert(exec.registers[0]._uint64 == 123);
	assert(exec.registers[2]._uint64 == 0);
}
