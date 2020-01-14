#ifndef BYTECODE_H
#define BYTECODE_H

#include "core.h"

namespace bytecode
{

using ptr_t = void *;

struct register_value
{
	union
	{
		int8_t    _int8;
		int16_t   _int16;
		int32_t   _int32;
		int64_t   _int64;
		uint8_t   _uint8;
		uint16_t  _uint16;
		uint32_t  _uint32;
		uint64_t  _uint64;
		float32_t _float32;
		float64_t _float64;
		ptr_t     _ptr;
	};

	template<typename T>
	T get_value(void) const;
};

enum class type_kind
{
	int8, int16, int32, int64,
	uint8, uint16, uint32, uint64,
	float32, float64, ptr
};

inline size_t type_size(type_kind t)
{
	switch (t)
	{
	case type_kind::int8: return 1;
	case type_kind::int16: return 2;
	case type_kind::int32: return 4;
	case type_kind::int64: return 8;
	case type_kind::uint8: return 1;
	case type_kind::uint16: return 2;
	case type_kind::uint32: return 4;
	case type_kind::uint64: return 8;
	case type_kind::float32: return 4;
	case type_kind::float64: return 8;
	case type_kind::ptr: return 8;
	}
	assert(false);
	return 0;
}

inline bool are_compatible_types(type_kind t1, type_kind t2)
{
	return type_size(t1) == type_size(t2);
}

template<typename T>
constexpr type_kind type_kind_of;

#define def_type_kind_of(type)                               \
template<>                                                   \
constexpr type_kind type_kind_of<type##_t> = type_kind::type

def_type_kind_of(int8);
def_type_kind_of(int16);
def_type_kind_of(int32);
def_type_kind_of(int64);
def_type_kind_of(uint8);
def_type_kind_of(uint16);
def_type_kind_of(uint32);
def_type_kind_of(uint64);
def_type_kind_of(float32);
def_type_kind_of(float64);
def_type_kind_of(ptr);

template<typename T>
T register_value::get_value(void) const
{
#define CASE(type)                                \
if constexpr (type_kind_of<T> == type_kind::type) \
{                                                 \
    return this->_##type;                         \
} else
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
		{
			assert(false);
		}
#undef CASE
}

struct stack
{
	using stack_t = bz::vector<uint8_t>;
	using iterator = stack_t::iterator;

	stack_t  _stack;
	iterator _base_ptr;
	iterator _stack_ptr;

	void *get_base_ptr(void)
	{
		return &*this->_base_ptr;
	}

	void *get_stack_ptr(void)
	{
		return &*this->_stack_ptr;
	}

	void *get_end_ptr(void)
	{
		return &*this->_stack.begin();
	}

	stack(void)
		: _stack    (1024 * 1024, 0),
		  _base_ptr (this->_stack.end()),
		  _stack_ptr(this->_stack.end())
	{}
};

struct instruction;

struct executor
{
	std::array<register_value, 8> registers;
	std::array<type_kind, 8>      register_types;
	stack stack;

	executor(void)
		: registers{},
		  register_types{},
		  stack()
	{}

	void execute(bz::vector<instruction> const &instructions);
};


struct value_pos_t : bz::variant<void *, size_t, register_value>
{
	using base_t = bz::variant<void *, size_t, register_value>;

	using base_t::variant;

	template<typename T>
	T get_value(executor const &exec) const
	{
		auto &st = exec.stack;
#define CASE(type)                                                                          \
if constexpr (type_kind_of<T> == type_kind::type)                                           \
{                                                                                           \
    switch (this->index())                                                                  \
    {                                                                                       \
    case index_of<void *>:                                                                  \
    {                                                                                       \
        auto ptr = this->get<void *>();                                                     \
        assert((intptr_t)ptr % alignof(type##_t) == 0);                                     \
        assert(ptr >= &*st._stack.begin());                                                 \
        assert(ptr < &*st._stack.end());                                                    \
        return *reinterpret_cast<type##_t *>(ptr);                                          \
    }                                                                                       \
    case index_of<size_t>:                                                                  \
    {                                                                                       \
        auto register_index = this->get<size_t>();                                          \
        assert(are_compatible_types(exec.register_types[register_index], type_kind::type)); \
        return exec.registers[register_index]._##type;                                      \
    }                                                                                       \
    case index_of<register_value>:                                                          \
        return this->get<register_value>().get_value<type##_t>();                           \
    default: assert(false);                                                                 \
    }                                                                                       \
} else
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
		{
			assert(false);
		}
#undef CASE
	}

	template<typename T>
	void store_value(T val, executor &exec) const
	{
		auto &st = exec.stack;
#define CASE(type)                                             \
if constexpr (type_kind_of<T> == type_kind::type)              \
{                                                              \
    switch (this->index())                                     \
    {                                                          \
    case index_of<void *>:                                     \
    {                                                          \
        auto ptr = this->get<void *>();                        \
        assert((intptr_t)ptr % alignof(type##_t) == 0);        \
        assert(ptr >= &*st._stack.begin());                    \
        assert(ptr < &*st._stack.end());                       \
        *reinterpret_cast<type##_t *>(ptr) = val;              \
        break;                                                 \
    }                                                          \
    case index_of<size_t>:                                     \
    {                                                          \
        auto register_index = this->get<size_t>();             \
        exec.registers[register_index]._##type = val;          \
        exec.register_types[register_index] = type_kind::type; \
        break;                                                 \
    }                                                          \
    default: assert(false); break;                             \
    }                                                          \
} else
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
		{
			assert(false);
		}
#undef CASE
	}
};

struct mov
{
	value_pos_t dest;
	value_pos_t src;
	type_kind type;

	void execute(executor &exec) const;
};

struct add
{
	value_pos_t res;
	value_pos_t lhs;
	value_pos_t rhs;
	type_kind type;

	void execute(executor &exec) const;
};

struct sub
{
	value_pos_t res;
	value_pos_t lhs;
	value_pos_t rhs;
	type_kind type;

	void execute(executor &exec) const;
};

struct mul
{
	value_pos_t res;
	value_pos_t lhs;
	value_pos_t rhs;
	type_kind type;

	void execute(executor &exec) const;
};

struct div
{
	value_pos_t res;
	value_pos_t lhs;
	value_pos_t rhs;
	type_kind type;

	void execute(executor &exec) const;
};


struct instruction : bz::variant<
	mov,
	add, sub, mul, div
>
{
	using base_t = bz::variant<
		mov,
		add, sub, mul, div
	>;

	using base_t::variant;

	void execute(executor &exec) const;
};

} // namespace bytecode

#endif // BYTECODE_H
