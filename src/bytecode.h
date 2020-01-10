#ifndef BYTECODE_H
#define BYTECODE_H

#include "core.h"

namespace bytecode
{

struct stack
{
	using stack_t = bz::vector<uint8_t>;
	using iterator = stack_t::iterator;

	stack_t  _stack;
	iterator _base;
	iterator _end;

	stack(void)
		: _stack(1024 * 1024, 0),
		  _base (this->_stack.end()),
		  _end  (this->_stack.end())
	{}
};

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
		void     *_ptr;
	};
};

enum class type_kind
{
	int8, int16, int32, int64,
	uint8, uint16, uint32, uint64,
	float32, float64,
};


struct executor
{
	std::array<register_value, 8> registers;
	stack stack;
};

} // namespace bytecode

#endif // BYTECODE_H
