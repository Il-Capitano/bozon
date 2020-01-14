#include "bytecode.h"
#include "context.h"

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
	default: assert(false); break;
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
	default: assert(false); break;
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
	default: assert(false); break;
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
	default: assert(false); break;
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
	default: assert(false); break;
	}
#undef CASE
}

void call::execute(executor &exec) const
{
	assert(false);
}

void cast::execute(executor &exec) const
{
	auto store = [&exec, this](auto val)
	{
#define CASE(type)                                                     \
case type_kind::type:                                                  \
    if constexpr (bz::meta::is_convertible_v<decltype(val), type##_t>) \
    {                                                                  \
        this->dest.store_value(static_cast<type##_t>(val), exec);      \
        break;                                                         \
    }                                                                  \
    else                                                               \
    {                                                                  \
        assert(false);                                                 \
    }                                                                  \
    break;

	switch (dest_t)
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
	default: assert(false); break;
	}
#undef CASE
	};

#define CASE(type)                                      \
case type_kind::type:                                   \
{                                                       \
    type##_t val = this->src.get_value<type##_t>(exec); \
    store(val);                                         \
    break;                                              \
}
	switch (this->src_t)
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
	default: assert(false); break;
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
		mul{ 2, 0, 1, type_kind::uint64 },
		cast{ 2, 2, type_kind::uint64, type_kind::float64 },
	};

	assert(exec.registers[r0]._uint64 == 0);
	exec.execute(instructions);
	assert(exec.registers[r0]._uint64 == 123);
	assert(exec.registers[r2]._float64 == (123 * 123));
}
