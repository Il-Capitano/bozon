#ifndef FIRST_PASS_PARSER_H
#define FIRST_PASS_PARSER_H

#include "core.h"

#include "token_stream.h"

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
variant<
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
	using base_t = variant<
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
		if_statement          = index_of<fp_if_statement_ptr>(),
		while_statement       = index_of<fp_while_statement_ptr>(),
		for_statement         = index_of<fp_for_statement_ptr>(),
		return_statement      = index_of<fp_return_statement_ptr>(),
		no_op_statement       = index_of<fp_no_op_statement_ptr>(),
		compound_statement    = index_of<fp_compound_statement_ptr>(),
		expression_statement  = index_of<fp_expression_statement_ptr>(),
		declaration_statement = index_of<fp_declaration_statement_ptr>(),
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
	std::vector<token> condition;
	fp_statement_ptr   then_block;
	fp_statement_ptr   else_block;

	fp_if_statement(
		std::vector<token> _cond,
		fp_statement_ptr   _then_block,
		fp_statement_ptr   _else_block
	)
		: condition (std::move(_cond)),
		  then_block(std::move(_then_block)),
		  else_block(std::move(_else_block))
	{}
};

struct fp_while_statement
{
	std::vector<token> condition;
	fp_statement_ptr   while_block;

	fp_while_statement(
		std::vector<token> _cond,
		fp_statement_ptr   _while_block
	)
		: condition  (std::move(_cond)),
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
	std::vector<token> expr;

	fp_return_statement(std::vector<token> _expr)
		: expr(std::move(_expr))
	{}
};

struct fp_no_op_statement
{
	// nothing
};

struct fp_compound_statement
{
	std::vector<fp_statement_ptr> statements;

	fp_compound_statement(std::vector<fp_statement_ptr> _stms)
		: statements(std::move(_stms))
	{}
};

struct fp_expression_statement
{
	std::vector<token> expr;

	fp_expression_statement(std::vector<token> _expr)
		: expr(std::move(_expr))
	{}
};


struct fp_variable_decl
{
	intern_string      identifier;
	std::vector<token> type_and_init;

	fp_variable_decl(
		intern_string      _id,
		std::vector<token> _type_and_init
	)
		: identifier   (_id),
		  type_and_init(std::move(_type_and_init))
	{}
};
using fp_variable_decl_ptr = std::unique_ptr<fp_variable_decl>;

struct fp_function_decl
{
	intern_string             identifier;
	std::vector<token>        params;
	std::vector<token>        return_type;
	fp_compound_statement_ptr body;

	fp_function_decl(
		intern_string             _id,
		std::vector<token>        _params,
		std::vector<token>        _ret_type,
		fp_compound_statement_ptr _body
	)
		: identifier (_id),
		  params     (std::move(_params)),
		  return_type(std::move(_ret_type)),
		  body       (std::move(_body))
	{}
};
using fp_function_decl_ptr = std::unique_ptr<fp_function_decl>;

struct fp_operator_decl
{
	uint32_t                  op;
	std::vector<token>        params;
	std::vector<token>        return_type;
	fp_compound_statement_ptr body;

	fp_operator_decl(
		uint32_t                  _op,
		std::vector<token>        _params,
		std::vector<token>        _ret_type,
		fp_compound_statement_ptr _body
	)
		: op         (_op),
		  params     (std::move(_params)),
		  return_type(std::move(_ret_type)),
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
variant<
	fp_variable_decl_ptr,
	fp_function_decl_ptr,
	fp_operator_decl_ptr,
	fp_struct_decl_ptr
>
{
	using base_t = variant<
		fp_variable_decl_ptr,
		fp_function_decl_ptr,
		fp_operator_decl_ptr,
		fp_struct_decl_ptr
	>;

	enum : uint32_t
	{
		variable_decl = index_of<fp_variable_decl_ptr>(),
		function_decl = index_of<fp_function_decl_ptr>(),
		operator_decl = index_of<fp_operator_decl_ptr>(),
		struct_decl   = index_of<fp_struct_decl_ptr>(),
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


std::vector<fp_statement_ptr> get_fp_statements(token_stream &stream);

#endif // FIRST_PASS_PARSER_H
