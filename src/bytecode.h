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

enum register_index : size_t
{
	r0 = 0,
	r1, r2, r3, r4, r5, r6, r7,
	rsp, rbp, _last
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

inline bool are_types_compatible(type_kind t1, type_kind t2)
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

struct instruction;

struct executor
{
	static constexpr auto register_count = static_cast<size_t>(register_index::_last);
	std::array<register_value, register_count> registers;
	std::array<type_kind,      register_count> register_types;
	bz::vector<uint8_t> stack;

	executor(void)
		: registers{},
		  register_types{},
		  stack(1024 * 1024, 0)
	{
		this->registers[rbp]._ptr = &*this->stack.end();
		this->register_types[rbp] = type_kind::ptr;
		this->registers[rsp]._ptr = &*this->stack.end();
		this->register_types[rsp] = type_kind::ptr;
	}

	void execute(bz::vector<instruction> const &instructions);
};


struct ptr_value : bz::variant<int64_t, size_t>
{
	using base_t = bz::variant<int64_t, size_t>;

	template<typename T>
	explicit ptr_value(T &&val)
		: base_t(std::forward<T>(val))
	{}
};

struct value_pos_t : bz::variant<ptr_value, size_t, register_value>
{
	using base_t = bz::variant<ptr_value, size_t, register_value>;

	using base_t::variant;

	template<typename T>
	T get_value(executor const &exec) const
	{
#define CASE(type)                                                               \
if constexpr (type_kind_of<T> == type_kind::type)                                \
{                                                                                \
    switch (this->index())                                                       \
    {                                                                            \
    case index_of<ptr_value>:                                                    \
    {                                                                            \
        auto &ptr = this->get<ptr_value>();                                      \
        switch (ptr.index())                                                     \
        {                                                                        \
        case ptr_value::index_of<int64_t>:                                       \
        {                                                                        \
            auto const offset = ptr.get<int64_t>();                              \
            auto const ptr_val =                                                 \
                reinterpret_cast<uint8_t *>(exec.registers[rbp]._ptr) + offset;  \
            assert((uint64_t)ptr_val % alignof(type##_t) == 0);                  \
            assert(ptr_val >= &*exec.stack.begin());                             \
            assert(ptr_val < &*exec.stack.end());                                \
            return *reinterpret_cast<type##_t *>(ptr_val);                       \
        }                                                                        \
        case ptr_value::index_of<size_t>:                                        \
        {                                                                        \
            auto const idx = ptr.get<size_t>();                                  \
            assert(exec.register_types[idx] == type_kind::ptr);                  \
            auto const ptr_val = exec.registers[idx]._ptr;                       \
            assert((uint64_t)ptr_val % alignof(type##_t) == 0);                  \
            assert(ptr_val >= &*exec.stack.begin());                             \
            assert(ptr_val < &*exec.stack.end());                                \
            return *reinterpret_cast<type##_t *>(ptr_val);                       \
        }                                                                        \
        default: assert(false);                                                  \
        }                                                                        \
        break;                                                                   \
    }                                                                            \
    case index_of<size_t>:                                                       \
    {                                                                            \
        auto idx = this->get<size_t>();                                          \
        assert(are_types_compatible(exec.register_types[idx], type_kind::type)); \
        return exec.registers[idx]._##type;                                      \
    }                                                                            \
    case index_of<register_value>:                                               \
        return this->get<register_value>().get_value<type##_t>();                \
    default: assert(false);                                                      \
    }                                                                            \
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
#define CASE(type)                                                              \
if constexpr (type_kind_of<T> == type_kind::type)                               \
{                                                                               \
    switch (this->index())                                                      \
    {                                                                           \
    case index_of<ptr_value>:                                                   \
    {                                                                           \
        auto &ptr = this->get<ptr_value>();                                     \
        switch (ptr.index())                                                    \
        {                                                                       \
        case ptr_value::index_of<int64_t>:                                      \
        {                                                                       \
            auto const offset = ptr.get<int64_t>();                             \
            auto const ptr_val =                                                \
                reinterpret_cast<uint8_t *>(exec.registers[rbp]._ptr) + offset; \
            assert((uint64_t)ptr_val % alignof(type##_t) == 0);                 \
            assert(ptr_val >= &*exec.stack.begin());                            \
            assert(ptr_val < &*exec.stack.end());                               \
            *reinterpret_cast<type##_t *>(ptr_val) = val;                       \
            break;                                                              \
        }                                                                       \
        case ptr_value::index_of<size_t>:                                       \
        {                                                                       \
            auto const idx = ptr.get<size_t>();                                 \
            assert(exec.register_types[idx] == type_kind::ptr);                 \
            auto const ptr_val = exec.registers[idx]._ptr;                      \
            assert((uint64_t)ptr_val % alignof(type##_t) == 0);                 \
            assert(ptr_val >= &*exec.stack.begin());                            \
            assert(ptr_val < &*exec.stack.end());                               \
            *reinterpret_cast<type##_t *>(ptr_val) = val;                       \
            break;                                                              \
        }                                                                       \
        default: assert(false); break;                                          \
        }                                                                       \
        break;                                                                  \
    }                                                                           \
    case index_of<size_t>:                                                      \
    {                                                                           \
        auto idx = this->get<size_t>();                                         \
        exec.registers[idx]._##type = val;                                      \
        exec.register_types[idx] = type_kind::type;                             \
        break;                                                                  \
    }                                                                           \
    default: assert(false); break;                                              \
    }                                                                           \
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

struct call
{
	bz::string id;

	void execute(executor &exec) const;
};

struct cast
{
	value_pos_t src;
	value_pos_t dest;
	type_kind src_t;
	type_kind dest_t;

	void execute(executor &exec) const;
};

struct deref
{
	value_pos_t src_ptr;
	value_pos_t dest;
	type_kind type;

	void execute(executor &exec) const;
};


struct instruction : bz::variant<
	mov,
	add, sub, mul, div,
	call,
	cast,
	deref
>
{
	using base_t = bz::variant<
		mov,
		add, sub, mul, div,
		call,
		cast,
		deref
	>;

	using base_t::variant;

	void execute(executor &exec) const;
};

} // namespace bytecode

#endif // BYTECODE_H
