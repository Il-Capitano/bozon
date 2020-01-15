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
    else if constexpr (sizeof(decltype(val)) == sizeof(type##_t))      \
    {                                                                  \
        this->dest.store_value(bit_cast<type##_t>(val), exec);         \
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

void deref::execute(executor &exec) const
{
#define CASE(type)                                                                 \
case type_kind::type:                                                              \
{                                                                                  \
    auto ptr = reinterpret_cast<type##_t *>(this->src_ptr.get_value<ptr_t>(exec)); \
    this->dest.store_value(*ptr, exec);                                            \
    break;                                                                         \
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
		sub{ rsp, rsp, register_value{._uint64 = 4}, type_kind::uint64 },

		mov{ ptr_value(-4ll), register_value{._int32 = -123}, type_kind::int32 },
		mov{ r0, ptr_value(-4ll), type_kind::int32 },

		sub{ r1, rbp, register_value{._uint64 = 4}, type_kind::uint64 },
		cast{ r1, r1, type_kind::uint64, type_kind::ptr },
		deref{ r1, r2, type_kind::int32 },

		add{ rsp, rsp, register_value{._uint64 = 4}, type_kind::uint64 },
	};

	exec.execute(instructions);
	assert(exec.registers[rsp]._ptr == exec.registers[rbp]._ptr);
	assert(*reinterpret_cast<int32_t *>(
		reinterpret_cast<uint8_t *>(exec.registers[rbp]._ptr) - 4
	) == -123);
	assert(exec.registers[r0]._int32 == -123);
	assert(exec.registers[r2]._int32 == -123);
}
