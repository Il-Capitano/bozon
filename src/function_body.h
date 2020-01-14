#ifndef FUNCTION_BODY_H
#define FUNCTION_BODY_H

#include "core.h"
#include "bytecode.h"

struct function_body
{
	bz::string identifier;
	bz::vector<bytecode::instruction> instructions;

	function_body(
		bz::string _id,
		bz::vector<bytecode::instruction> _instructions
	)
		: identifier  (std::move(_id)),
		  instructions(std::move(_instructions))
	{}
};

#endif // FUNCTION_BODY_H
