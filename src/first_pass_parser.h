#ifndef FIRST_PASS_PARSER_H
#define FIRST_PASS_PARSER_H

#include "core.h"

#include "lexer.h"


struct fp_if_statement;
using fp_if_statement_ptr = std::unique_ptr<fp_if_statement>;
struct fp_while_statement;
using fp_while_statement_ptr = std::unique_ptr<fp_while_statement>;
struct fp_for_statement;
using fp_for_statement_ptr = std::unique_ptr<fp_for_statement>;
struct fp_return_statement;
using fp_return_statement_ptr = std::unique_ptr<fp_return_statement>;
struct fp_no_op_statement;
using fp_no_op_statement_ptr = std::unique_ptr<fp_no_op_statement>;
struct fp_compound_statement;
using fp_compound_statement_ptr = std::unique_ptr<fp_compound_statement>;
struct fp_expression_statement;
using fp_expression_statement_ptr = std::unique_ptr<fp_expression_statement>;
struct fp_declaration_statement;
using fp_declaration_statement_ptr = std::unique_ptr<fp_declaration_statement>;


struct fp_statement :
bz::variant<
	fp_if_statement_ptr,
	fp_while_statement_ptr,
	fp_for_statement_ptr,
	fp_return_statement_ptr,
	fp_no_op_statement_ptr,
	fp_compound_statement_ptr,
	fp_expression_statement_ptr,
	fp_declaration_statement_ptr
>
{
	using base_t = bz::variant<
		fp_if_statement_ptr,
		fp_while_statement_ptr,
		fp_for_statement_ptr,
		fp_return_statement_ptr,
		fp_no_op_statement_ptr,
		fp_compound_statement_ptr,
		fp_expression_statement_ptr,
		fp_declaration_statement_ptr
	>;

	enum : uint32_t
	{
		if_statement          = index_of<fp_if_statement_ptr>,
		while_statement       = index_of<fp_while_statement_ptr>,
		for_statement         = index_of<fp_for_statement_ptr>,
		return_statement      = index_of<fp_return_statement_ptr>,
		no_op_statement       = index_of<fp_no_op_statement_ptr>,
		compound_statement    = index_of<fp_compound_statement_ptr>,
		expression_statement  = index_of<fp_expression_statement_ptr>,
		declaration_statement = index_of<fp_declaration_statement_ptr>,
	};

	uint32_t kind(void) const
	{
		return base_t::index();
	}

	fp_statement(fp_if_statement_ptr          if_stm      );
	fp_statement(fp_while_statement_ptr       while_stm   );
	fp_statement(fp_for_statement_ptr         for_stm     );
	fp_statement(fp_return_statement_ptr      return_stm  );
	fp_statement(fp_no_op_statement_ptr       no_op_stm   );
	fp_statement(fp_compound_statement_ptr    compound_stm);
	fp_statement(fp_expression_statement_ptr  expr_stm    );
	fp_statement(fp_declaration_statement_ptr decl_stmt   );
};

using fp_statement_ptr = std::unique_ptr<fp_statement>;

struct fp_if_statement
{
	token_range      condition;
	fp_statement_ptr then_block;
	fp_statement_ptr else_block;

	fp_if_statement(
		token_range      _cond,
		fp_statement_ptr _then_block,
		fp_statement_ptr _else_block
	)
		: condition (_cond),
		  then_block(std::move(_then_block)),
		  else_block(std::move(_else_block))
	{}
};

struct fp_while_statement
{
	token_range      condition;
	fp_statement_ptr while_block;

	fp_while_statement(
		token_range      _cond,
		fp_statement_ptr _while_block
	)
		: condition  (_cond),
		  while_block(std::move(_while_block))
	{}
};

struct fp_for_statement
{
	// TODO: implement
//	fp_for_statement()
//		:
//	{}
};

struct fp_return_statement
{
	token_range expr;

	fp_return_statement(token_range _expr)
		: expr(_expr)
	{}
};

struct fp_no_op_statement
{
	// nothing
};

struct fp_compound_statement
{
	bz::vector<fp_statement_ptr> statements;

	fp_compound_statement(bz::vector<fp_statement_ptr> _stms)
		: statements(std::move(_stms))
	{}
};

struct fp_expression_statement
{
	token_range expr;

	fp_expression_statement(token_range _expr)
		: expr(_expr)
	{}
};


struct fp_variable_decl
{
	intern_string identifier;
	token_range   type_and_init;

	fp_variable_decl(
		intern_string _id,
		token_range   _type_and_init
	)
		: identifier   (_id),
		  type_and_init(_type_and_init)
	{}
};
using fp_variable_decl_ptr = std::unique_ptr<fp_variable_decl>;

struct fp_function_decl
{
	intern_string             identifier;
	token_range               params;
	token_range               return_type;
	fp_compound_statement_ptr body;

	fp_function_decl(
		intern_string             _id,
		token_range               _params,
		token_range               _ret_type,
		fp_compound_statement_ptr _body
	)
		: identifier (_id),
		  params     (_params),
		  return_type(_ret_type),
		  body       (std::move(_body))
	{}
};
using fp_function_decl_ptr = std::unique_ptr<fp_function_decl>;

struct fp_operator_decl
{
	uint32_t                  op;
	token_range               params;
	token_range               return_type;
	fp_compound_statement_ptr body;

	fp_operator_decl(
		uint32_t                  _op,
		token_range               _params,
		token_range               _ret_type,
		fp_compound_statement_ptr _body
	)
		: op         (_op),
		  params     (_params),
		  return_type(_ret_type),
		  body       (std::move(_body))
	{}
};
using fp_operator_decl_ptr = std::unique_ptr<fp_operator_decl>;

struct fp_struct_decl
{
	// TODO
};
using fp_struct_decl_ptr = std::unique_ptr<fp_struct_decl>;


struct fp_declaration_statement :
bz::variant<
	fp_variable_decl_ptr,
	fp_function_decl_ptr,
	fp_operator_decl_ptr,
	fp_struct_decl_ptr
>
{
	using base_t = bz::variant<
		fp_variable_decl_ptr,
		fp_function_decl_ptr,
		fp_operator_decl_ptr,
		fp_struct_decl_ptr
	>;

	enum : uint32_t
	{
		variable_decl = index_of<fp_variable_decl_ptr>,
		function_decl = index_of<fp_function_decl_ptr>,
		operator_decl = index_of<fp_operator_decl_ptr>,
		struct_decl   = index_of<fp_struct_decl_ptr>,
	};

	uint32_t kind(void) const
	{
		return base_t::index();
	}

	fp_declaration_statement(fp_variable_decl_ptr _var_decl)
		: base_t(std::move(_var_decl))
	{}

	fp_declaration_statement(fp_function_decl_ptr _func_decl)
		: base_t(std::move(_func_decl))
	{}

	fp_declaration_statement(fp_operator_decl_ptr _op_decl)
		: base_t(std::move(_op_decl))
	{}

	fp_declaration_statement(fp_struct_decl_ptr _struct_decl)
		: base_t(std::move(_struct_decl))
	{}
};


template<typename ...Args>
inline fp_statement_ptr make_fp_statement(Args &&...args)
{
	return std::make_unique<fp_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
inline fp_if_statement_ptr make_fp_if_statement(Args &&...args)
{
	return std::make_unique<fp_if_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
inline fp_while_statement_ptr make_fp_while_statement(Args &&...args)
{
	return std::make_unique<fp_while_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
inline fp_for_statement_ptr make_fp_for_statement(Args &&...args)
{
	return std::make_unique<fp_for_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
inline fp_return_statement_ptr make_fp_return_statement(Args &&...args)
{
	return std::make_unique<fp_return_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
inline fp_no_op_statement_ptr make_fp_no_op_statement(Args &&...args)
{
	return std::make_unique<fp_no_op_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
inline fp_compound_statement_ptr make_fp_compound_statement(Args &&...args)
{
	return std::make_unique<fp_compound_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
inline fp_expression_statement_ptr make_fp_expression_statement(Args &&...args)
{
	return std::make_unique<fp_expression_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
inline fp_declaration_statement_ptr make_fp_declaration_statement(Args &&...args)
{
	return std::make_unique<fp_declaration_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
inline fp_variable_decl_ptr make_fp_variable_decl(Args &&...args)
{
	return std::make_unique<fp_variable_decl>(std::forward<Args>(args)...);
}

template<typename ...Args>
inline fp_function_decl_ptr make_fp_function_decl(Args &&...args)
{
	return std::make_unique<fp_function_decl>(std::forward<Args>(args)...);
}

template<typename ...Args>
inline fp_operator_decl_ptr make_fp_operator_decl(Args &&...args)
{
	return std::make_unique<fp_operator_decl>(std::forward<Args>(args)...);
}

template<typename ...Args>
inline fp_struct_decl_ptr make_fp_struct_decl(Args &&...args)
{
	return std::make_unique<fp_struct_decl>(std::forward<Args>(args)...);
}


fp_statement_ptr get_fp_statement(src_tokens::pos &stream, src_tokens::pos end);
bz::vector<fp_statement_ptr> get_fp_statements(src_tokens::pos &stream, src_tokens::pos end);

#endif // FIRST_PASS_PARSER_H
