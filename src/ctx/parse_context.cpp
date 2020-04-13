#include "parse_context.h"
#include "global_context.h"
#include "built_in_operators.h"
#include "parser.h"

namespace ctx
{

parse_context::parse_context(global_context &_global_ctx)
	: global_ctx(_global_ctx), scope_decls{}
{}

void parse_context::report_error(lex::token_pos it) const
{
	this->global_ctx.report_error(ctx::make_error(it));
}

void parse_context::report_error(
	lex::token_pos it,
	bz::string message, bz::vector<ctx::note> notes
) const
{
	this->global_ctx.report_error(ctx::make_error(
		it, std::move(message), std::move(notes)
	));
}

void parse_context::report_error(
	lex::token_pos begin, lex::token_pos pivot, lex::token_pos end,
	bz::string message, bz::vector<ctx::note> notes
) const
{
	this->global_ctx.report_error(ctx::make_error(
		begin, pivot, end, std::move(message), std::move(notes)
	));
}

bool parse_context::has_errors(void) const
{
	return this->global_ctx.has_errors();
}

lex::token_pos parse_context::assert_token(lex::token_pos &stream, uint32_t kind) const
{
	if (stream->kind != kind)
	{
		this->global_ctx.report_error(ctx::make_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format("expected {} before end-of-file", lex::get_token_name_for_message(kind))
			: bz::format("expected {}", lex::get_token_name_for_message(kind))
		));
		return stream;
	}
	else
	{
		auto t = stream;
		++stream;
		return t;
	}
}

lex::token_pos parse_context::assert_token(lex::token_pos &stream, uint32_t kind1, uint32_t kind2) const
{
	if (stream->kind != kind1 && stream->kind != kind2)
	{
		this->global_ctx.report_error(ctx::make_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format(
				"expected {} or {} before end-of-file",
				lex::get_token_name_for_message(kind1), lex::get_token_name_for_message(kind2)
			)
			: bz::format(
				"expected {} or {}",
				lex::get_token_name_for_message(kind1), lex::get_token_name_for_message(kind2)
			)
		));
		return stream;
	}
	else
	{
		auto t = stream;
		++stream;
		return t;
	}
}

void parse_context::report_ambiguous_id_error(lex::token_pos id) const
{
	bz_assert(false);
}


void parse_context::add_scope(void)
{
	this->scope_decls.push_back({});
}

void parse_context::remove_scope(void)
{
	bz_assert(!this->scope_decls.empty());
	this->scope_decls.pop_back();
}


void parse_context::add_local_variable(ast::decl_variable &var_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	this->scope_decls.back().var_decls.push_back(&var_decl);
}

void parse_context::add_local_function(ast::decl_function &func_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	auto &sets = this->scope_decls.back().func_sets;
	auto const set = std::find_if(
		sets.begin(), sets.end(),
		[id = func_decl.identifier->value](auto const &set) {
			return id == set.id;
		}
	);
	if (set == sets.end())
	{
		sets.push_back({ func_decl.identifier->value, { &func_decl } });
	}
	else
	{
		// TODO: check for conflicts
		set->func_decls.push_back(&func_decl);
	}
	this->global_ctx.add_compile_function(func_decl);
}

void parse_context::add_local_operator(ast::decl_operator &op_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	auto &sets = this->scope_decls.back().op_sets;
	auto const set = std::find_if(
		sets.begin(), sets.end(),
		[op = op_decl.op->kind](auto const &set) {
			return op == set.op;
		}
	);
	if (set == sets.end())
	{
		sets.push_back({ op_decl.op->kind, { &op_decl } });
	}
	else
	{
		// TODO: check for conflicts
		set->op_decls.push_back(&op_decl);
	}
	this->global_ctx.add_compile_operator(op_decl);
}

void parse_context::add_local_struct(ast::decl_struct &struct_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	this->scope_decls.back().type_infos.push_back({ struct_decl.identifier->value, &struct_decl.info });
}


auto parse_context::get_identifier_decl(lex::token_pos id) const
	-> bz::variant<ast::decl_variable const *, ast::decl_function const *>
{
	// ==== local decls ====
	// we go in reverse through the scopes and the variables
	// in case there's shadowing
	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const var = std::find_if(
			scope->var_decls.rbegin(), scope->var_decls.rend(),
			[id = id->value](auto const &var) {
				return var->identifier->value == id;
			}
		);
		if (var != scope->var_decls.rend())
		{
			return *var;
		}

		auto const fn_set = std::find_if(
			scope->func_sets.begin(), scope->func_sets.end(),
			[id = id->value](auto const &fn_set) {
				return fn_set.id == id;
			}
		);
		if (fn_set != scope->func_sets.end())
		{
			if (fn_set->func_decls.size() == 1)
			{
				return fn_set->func_decls[0];
			}
			else
			{
				bz_assert(!fn_set->func_decls.empty());
				return static_cast<ast::decl_function const *>(nullptr);
			}
		}
	}

	// ==== export (global) decls ====
	auto &export_decls = this->global_ctx._export_decls;
	auto const var = std::find_if(
		export_decls.var_decls.begin(), export_decls.var_decls.end(),
		[id = id->value](auto const &var) {
			return id == var->identifier->value;
		}
	);
	if (var != export_decls.var_decls.end())
	{
		return *var;
	}

	auto const fn_set = std::find_if(
		export_decls.func_sets.begin(), export_decls.func_sets.end(),
		[id = id->value](auto const &fn_set) {
			return id == fn_set.id;
		}
	);
	if (fn_set != export_decls.func_sets.end())
	{
		if (fn_set->func_decls.size() == 1)
		{
			return fn_set->func_decls[0];
		}
		else
		{
			bz_assert(!fn_set->func_decls.empty());
			return static_cast<ast::decl_function const *>(nullptr);
		}
	}
	return {};
}


ast::expression::expr_type_t parse_context::get_identifier_type(lex::token_pos id) const
{
	auto decl = this->get_identifier_decl(id);
	switch (decl.index())
	{
	case decl.index_of<ast::decl_variable const *>:
	{
		auto const var = decl.get<ast::decl_variable const *>();
		return {
			var->var_type.is<ast::ts_reference>()
			? ast::expression::lvalue_reference
			: ast::expression::lvalue,
			ast::remove_lvalue_reference(var->var_type)
		};
	}
	case decl.index_of<ast::decl_function const *>:
	{
		auto const fn = decl.get<ast::decl_function const *>();
		auto fn_t = [&]() -> ast::typespec {
			if (fn == nullptr)
			{
				return ast::typespec();
			}
			bz::vector<ast::typespec> arg_types = {};
			for (auto &p : fn->body.params)
			{
				arg_types.emplace_back(p.var_type);
			}
			return ast::make_ts_function({ nullptr, nullptr }, nullptr, fn->body.return_type, arg_types);
		}();
		return { ast::expression::function_name, fn_t };
	}
	case decl.null:
		this->report_error(id, "undeclared identifier");
		return { ast::expression::rvalue, ast::typespec() };
	default:
		bz_assert(false);
		return {};
	}
}

static bool are_directly_matchable_types(
	ast::expression::expr_type_t const &from,
	ast::typespec const &to
)
{
	if (
		to.is<ast::ts_reference>()
		&& from.type_kind != ast::expression::expr_type_kind::lvalue
		&& from.type_kind != ast::expression::expr_type_kind::lvalue_reference
	)
	{
		return false;
	}

	ast::typespec const *to_it = [&]() -> ast::typespec const * {
		if (to.is<ast::ts_reference>())
		{
			return &to.get<ast::ts_reference_ptr>()->base;
		}
		else if (to.is<ast::ts_constant>())
		{
			return &to.get<ast::ts_constant_ptr>()->base;
		}
		else
		{
			return &to;
		}
	}();

	auto from_it = &from.expr_type;

	auto const advance = [](ast::typespec const *&it)
	{
		switch (it->kind())
		{
		case ast::typespec::index<ast::ts_pointer>:
			it = &it->get<ast::ts_pointer_ptr>()->base;
			break;
		case ast::typespec::index<ast::ts_constant>:
			it = &it->get<ast::ts_constant_ptr>()->base;
			break;
		default:
			bz_assert(false);
			break;
		}
	};

	while (true)
	{
		if (
			to_it->is<ast::ts_base_type>()
			&& from_it->is<ast::ts_base_type>()
		)
		{
			return to_it->get<ast::ts_base_type_ptr>()->info
				== from_it->get<ast::ts_base_type_ptr>()->info;
		}
		else if (to_it->kind() == from_it->kind())
		{
			advance(to_it);
			advance(from_it);
		}
		else if (to_it->is<ast::ts_constant>())
		{
			advance(to_it);
		}
		else
		{
			return false;
		}
	}

	return false;
}

static bool is_built_in_type(ast::typespec const &ts)
{
	switch (ts.kind())
	{
	case ast::typespec::index<ast::ts_constant>:
		return is_built_in_type(ts.get<ast::ts_constant_ptr>()->base);
	case ast::typespec::index<ast::ts_base_type>:
	{
		auto &base = *ts.get<ast::ts_base_type_ptr>();
		return (base.info->flags & ast::type_info::built_in) != 0;
	}
	case ast::typespec::index<ast::ts_pointer>:
	case ast::typespec::index<ast::ts_function>:
	case ast::typespec::index<ast::ts_tuple>:
		return true;
	default:
		return false;
	}
}

static int get_type_match_level(
	ast::typespec const &dest,
	ast::expression::expr_type_t const &src
)
{
	int result = 0;

	auto const *dest_it = &ast::remove_const(dest);
	auto const *src_it = &src.expr_type;

	if (dest_it->is<ast::ts_base_type>())
	{
		src_it = &ast::remove_const(*src_it);
		// if the argument is just a base type, return 1 if there's a conversion, and -1 otherwise
		// TODO: use is_convertible here...
		if (
			src_it->is<ast::ts_base_type>()
			&& src_it->get<ast::ts_base_type_ptr>()->info == dest_it->get<ast::ts_base_type_ptr>()->info
		)
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}
	else if (dest_it->is<ast::ts_void>())
	{
		bz_assert(false);
	}
	else if (dest_it->is<ast::ts_tuple>())
	{
		bz_assert(false);
	}
	else if (dest_it->is<ast::ts_function>())
	{
		bz_assert(false);
	}
	else if (dest_it->is<ast::ts_reference>())
	{
		// if the source is not an lvalue, return -1
		if (
			src.type_kind != ast::expression::lvalue
			&& src.type_kind != ast::expression::lvalue_reference
		)
		{
			return -1;
		}

		dest_it = &ast::remove_lvalue_reference(*dest_it);
		// if the dest is not a &const return 0 if the src and dest types match, -1 otherwise
		if (!dest_it->is<ast::ts_constant>())
		{
			return *dest_it == *src_it ? 0 : -1;
		}
		else
		{
			dest_it = &dest_it->get<ast::ts_constant_ptr>()->base;
			// if the source is not const increment the result
			if (src_it->is<ast::ts_constant>())
			{
				src_it = &src_it->get<ast::ts_constant_ptr>()->base;
			}
			else
			{
				++result;
			}
		}
	}
	else // if (dest_it->is<ast::ts_pointer>())
	{
		result = 1; // not a reference, so result starts at 1
		// advance src_it if it's const
		src_it = &ast::remove_const(*src_it);
		// return -1 if the source is not a pointer
		if (!src_it->is<ast::ts_pointer>())
		{
			return -1;
		}

		// advance src_it and dest_it
		src_it = &src_it->get<ast::ts_pointer_ptr>()->base;
		dest_it = &dest_it->get<ast::ts_pointer_ptr>()->base;

		// if the dest is not a *const return 1 if the src and dest types match, -1 otherwise
		if (!dest_it->is<ast::ts_constant>())
		{
			return *dest_it == *src_it ? 0 : -1;
		}
		else
		{
			dest_it = &dest_it->get<ast::ts_constant_ptr>()->base;
			// if the source is not const increment the result
			if (src_it->is<ast::ts_constant>())
			{
				src_it = &src_it->get<ast::ts_constant_ptr>()->base;
			}
			else
			{
				++result;
			}
		}
	}

	// we can only get here if the dest type is &const or *const
	auto const advance = [](ast::typespec const *&ts)
	{
		switch (ts->kind())
		{
		case ast::typespec::index<ast::ts_constant>:
			ts = &ts->get<ast::ts_constant_ptr>()->base;
			break;
		case ast::typespec::index<ast::ts_pointer>:
			ts = &ts->get<ast::ts_pointer_ptr>()->base;
			break;
		default:
			bz_assert(false);
			break;
		}
	};

	while (true)
	{
		if (dest_it->is<ast::ts_base_type>())
		{
			if (
				src_it->is<ast::ts_base_type>()
				&& src_it->get<ast::ts_base_type_ptr>()->info == dest_it->get<ast::ts_base_type_ptr>()->info
			)
			{
				return result;
			}
			else
			{
				return -1;
			}
		}
		else if (dest_it->is<ast::ts_void>())
		{
			bz_assert(false);
		}
		else if (dest_it->is<ast::ts_function>())
		{
			bz_assert(false);
		}
		else if (dest_it->is<ast::ts_tuple>())
		{
			bz_assert(false);
		}
		else if (dest_it->kind() == src_it->kind())
		{
			advance(dest_it);
			advance(src_it);
		}
		else if (dest_it->is<ast::ts_constant>())
		{
			++result;
			advance(dest_it);
		}
		else
		{
			return -1;
		}
	}
}

struct match_level
{
	int min;
	int sum;
};

static match_level get_function_call_match_level(
	ast::function_body &func_body,
	ast::expr_function_call const &func_call,
	parse_context &context
)
{
	if (func_body.params.size() != func_call.params.size())
	{
		return { -1, -1 };
	}

	resolve_symbol(func_body, context);

	match_level result = { std::numeric_limits<int>::max(), 0 };

	auto const add_to_result = [&](int match)
	{
		if (result.min == -1)
		{
			return;
		}
		else if (match == -1)
		{
			result = { -1, -1 };
		}
		else
		{
			if (match < result.min)
			{
				result.min = match;
			}
			result.sum += match;
		}
	};

	auto params_it = func_body.params.begin();
	auto call_it  = func_call.params.begin();
	auto const types_end = func_body.params.end();
	for (; params_it != types_end; ++call_it, ++params_it)
	{
		add_to_result(get_type_match_level(params_it->var_type, call_it->expr_type));
	}
	return result;
}

static match_level get_function_call_match_level(
	ast::function_body &func_body,
	ast::expr_unary_op const &unary_op,
	parse_context &context
)
{
	if (func_body.params.size() != 1)
	{
		return { -1, -1 };
	}

	resolve_symbol(func_body, context);

	auto const match_level = get_type_match_level(func_body.params[0].var_type, unary_op.expr.expr_type);
	return { match_level, match_level };
}

static match_level get_function_call_match_level(
	ast::function_body &func_body,
	ast::expr_binary_op const &binary_op,
	parse_context &context
)
{
	if (func_body.params.size() != 2)
	{
		return { -1, -1 };
	}

	resolve_symbol(func_body, context);

	auto const lhs_level = get_type_match_level(func_body.params[0].var_type, binary_op.lhs.expr_type);
	auto const rhs_level = get_type_match_level(func_body.params[0].var_type, binary_op.rhs.expr_type);
	if (lhs_level == -1 || rhs_level == -1)
	{
		return { -1, -1 };
	}
	return { std::min(lhs_level, rhs_level), lhs_level + rhs_level };
}

static error get_bad_call_error(
	ast::decl_function *func,
	ast::expr_function_call const &func_call
)
{
	if (func->body.params.size() != func_call.params.size())
	{
		return make_error(
			func_call,
			bz::format(
				"expected {} {} for call to '{}', but was given {}",
				func->body.params.size(), func->body.params.size() == 1 ? "argument" : "arguments",
				func->identifier->value, func_call.params.size()
			),
			{
				make_note(
					func->identifier,
					bz::format("see declaration of '{}':", func->identifier->value)
				)
			}
		);
	}

	auto params_it = func->body.params.begin();
	auto call_it  = func_call.params.begin();
	auto const types_end = func->body.params.end();
//	auto const call_end  = func_call.params.end();
	for (; params_it != types_end; ++call_it, ++params_it)
	{
		if (get_type_match_level(params_it->var_type, call_it->expr_type) == -1)
		{
			return make_error(
				*call_it,
				bz::format(
					"unable to convert argument {} from '{}' to '{}'",
					(call_it - func_call.params.begin()) + 1,
					call_it->expr_type.expr_type, params_it->var_type
				)
			);
		}
	}

	bz_assert(false);
	return make_error(func->identifier, "");
}


static auto find_best_match(
	bz::vector<std::pair<match_level, ast::function_body *>> const &possible_funcs,
	size_t scope_decl_count
) -> std::pair<match_level, ast::function_body *>
{
	std::pair<match_level, ast::function_body *> min_scope_match = { { -1, -1 }, nullptr };
	{
		auto it = possible_funcs.begin();
		auto const end = possible_funcs.begin() + scope_decl_count;
		for (; it != end; ++it)
		{
			if (
				min_scope_match.first.min == -1
				|| it->first.min < min_scope_match.first.min
				|| (it->first.min == min_scope_match.first.min && it->first.sum < min_scope_match.first.sum)
			)
			{
				min_scope_match = *it;
			}
		}
	}

	std::pair<match_level, ast::function_body *> min_global_match = { { -1, -1 }, nullptr };
	{
		auto it = possible_funcs.begin() + scope_decl_count;
		auto const end = possible_funcs.end();
		for (; it != end; ++it)
		{
			if (
				min_global_match.first.min == -1
				|| it->first.min < min_global_match.first.min
				|| (it->first.min == min_global_match.first.min && it->first.sum < min_global_match.first.sum)
			)
			{
				min_global_match = *it;
			}
		}
	}

	if (min_scope_match.first.min == -1 && min_global_match.first.min == -1)
	{
		return { { -1, -1 }, nullptr };
	}
	// there's no global match
	else if (min_global_match.first.min == -1)
	{
		return min_scope_match;
	}
	// there's a better scope match
	else if (
		min_scope_match.first.min != -1
		&& (
			min_scope_match.first.min < min_global_match.first.min
			|| (min_scope_match.first.min == min_global_match.first.min && min_scope_match.first.sum <= min_global_match.first.sum)
		)
	)
	{
		return min_scope_match;
	}
	// the global match is the best
	else
	{
		// we need to check for ambiguity here
		bz::vector<std::pair<match_level, ast::function_body *>> possible_min_matches = {};
		for (
			auto it = possible_funcs.begin() + scope_decl_count;
			it != possible_funcs.end();
			++it
		)
		{
			if (it->first.min == min_global_match.first.min && it->first.sum == min_global_match.first.sum)
			{
				possible_min_matches.push_back(*it);
			}
		}

		bz_assert(possible_min_matches.size() != 0);
		if (possible_min_matches.size() == 1)
		{
			return min_global_match;
		}
		else
		{
			// TODO: report ambiguous call error somehow
			bz_assert(false);
			return { { -1, -1 }, nullptr };
		}
	}
}


auto parse_context::get_operation_body_and_type(ast::expr_unary_op const &unary_op)
	-> std::pair<ast::function_body *, ast::expression::expr_type_t>
{
	std::pair<ast::function_body *, ast::expression::expr_type_t> error_result = {
		nullptr, { ast::expression::rvalue, ast::typespec() }
	};

	if (!lex::is_overloadable_unary_operator(unary_op.op->kind))
	{
		auto type = get_non_overloadable_operation_type(
			unary_op.expr.expr_type, unary_op.op->kind,
			*this
		);
		if (type.has_error())
		{
			this->report_error(unary_op, std::move(type.get_error()));
			return error_result;
		}
		else
		{
			return { nullptr, std::move(type.get_result()) };
		}
	}

	if (is_built_in_type(ast::remove_const(unary_op.expr.expr_type.expr_type)))
	{
		auto type = get_built_in_operation_type(
			unary_op.expr.expr_type, unary_op.op->kind,
			*this
		);
		if (type.expr_type.kind() != ast::typespec::null)
		{
			return { nullptr, type };
		}
	}

	auto const report_undeclared_error = [&, this]()
	{
		this->report_error(
			unary_op,
			bz::format(
				"undeclared unary operator {} with type '{}'",
				unary_op.op->value,
				unary_op.expr.expr_type.expr_type
			)
		);
	};

	bz::vector<std::pair<match_level, ast::function_body *>> possible_funcs = {};

	// we go through the scope decls for a matching declaration
	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const set = std::find_if(
			scope->op_sets.begin(), scope->op_sets.end(),
			[op = unary_op.op->kind](auto const &op_set) {
				return op == op_set.op;
			}
		);
		if (set != scope->op_sets.end())
		{
			for (auto &op : set->op_decls)
			{
				auto const match_level = get_function_call_match_level(op->body, unary_op, *this);
				if (match_level.min != -1)
				{
					possible_funcs.push_back({ match_level, &op->body });
				}
			}
		}
	}

	auto const scope_decl_count = possible_funcs.size();

	auto &export_decls = this->global_ctx._export_decls;
	auto const global_set = std::find_if(
		export_decls.op_sets.begin(), export_decls.op_sets.end(),
		[op = unary_op.op->kind](auto const &op_set) {
			return op == op_set.op;
		}
	);
	if (global_set != export_decls.op_sets.end())
	{
		for (auto &op : global_set->op_decls)
		{
			auto const match_level = get_function_call_match_level(op->body, unary_op, *this);
			if (match_level.min != -1)
			{
				possible_funcs.push_back({ match_level, &op->body });
			}
		}
	}

	auto const best = find_best_match(possible_funcs, scope_decl_count);
	if (best.first.min == -1)
	{
		report_undeclared_error();
		return error_result;
	}
	else
	{
		auto &ret_t = best.second->return_type;
		if (ret_t.is<ast::ts_reference>())
		{
			return {
				best.second,
				{ ast::expression::lvalue_reference, ast::remove_lvalue_reference(ret_t) }
			};
		}
		else
		{
			return {
				best.second,
				{ ast::expression::rvalue, ast::remove_const(ret_t) }
			};
		}
	}
}

auto parse_context::get_operation_body_and_type(ast::expr_binary_op const &binary_op)
	-> std::pair<ast::function_body *, ast::expression::expr_type_t>
{
	std::pair<ast::function_body *, ast::expression::expr_type_t> error_result = {
		nullptr, { ast::expression::rvalue, ast::typespec() }
	};

	if (!lex::is_overloadable_binary_operator(binary_op.op->kind))
	{
		auto type = get_non_overloadable_operation_type(
			binary_op.lhs.expr_type, binary_op.rhs.expr_type,
			binary_op.op->kind,
			*this
		);
		if (type.has_error())
		{
			this->report_error(binary_op, std::move(type.get_error()));
			return error_result;
		}
		else
		{
			return { nullptr, std::move(type.get_result()) };
		}
	}

	if (
		is_built_in_type(ast::remove_const(binary_op.lhs.expr_type.expr_type))
		&& is_built_in_type(ast::remove_const(binary_op.rhs.expr_type.expr_type))
	)
	{
		auto type = get_built_in_operation_type(
			binary_op.lhs.expr_type, binary_op.rhs.expr_type,
			binary_op.op->kind,
			*this
		);
		if (type.expr_type.kind() != ast::typespec::null)
		{
			return { nullptr, type };
		}
	}

	auto const report_undeclared_error = [&, this]()
	{
		if (binary_op.op->kind == lex::token::square_open)
		{
			this->report_error(
				binary_op,
				bz::format(
					"undeclared binary operator [] with types '{}' and '{}'",
					binary_op.lhs.expr_type.expr_type, binary_op.rhs.expr_type.expr_type
				)
			);
		}
		else
		{
			this->report_error(
				binary_op,
				bz::format(
					"undeclared binary operator {} with types '{}' and '{}'",
					binary_op.op->value,
					binary_op.lhs.expr_type.expr_type, binary_op.rhs.expr_type.expr_type
				)
			);
		}
	};

	bz::vector<std::pair<match_level, ast::function_body *>> possible_funcs = {};

	// we go through the scope decls for a matching declaration
	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const set = std::find_if(
			scope->op_sets.begin(), scope->op_sets.end(),
			[op = binary_op.op->kind](auto const &op_set) {
				return op == op_set.op;
			}
		);
		if (set != scope->op_sets.end())
		{
			for (auto &op : set->op_decls)
			{
				auto const match_level = get_function_call_match_level(op->body, binary_op, *this);
				if (match_level.min != -1)
				{
					possible_funcs.push_back({ match_level, &op->body });
				}
			}
		}
	}

	auto const scope_decl_count = possible_funcs.size();

	auto &export_decls = this->global_ctx._export_decls;
	auto const global_set = std::find_if(
		export_decls.op_sets.begin(), export_decls.op_sets.end(),
		[op = binary_op.op->kind](auto const &op_set) {
			return op == op_set.op;
		}
	);
	if (global_set != export_decls.op_sets.end())
	{
		for (auto &op : global_set->op_decls)
		{
			auto const match_level = get_function_call_match_level(op->body, binary_op, *this);
			if (match_level.min != -1)
			{
				possible_funcs.push_back({ match_level, &op->body });
			}
		}
	}

	auto const best = find_best_match(possible_funcs, scope_decl_count);
	if (best.first.min == -1)
	{
		report_undeclared_error();
		return error_result;
	}
	else
	{
		auto &ret_t = best.second->return_type;
		if (ret_t.is<ast::ts_reference>())
		{
			return {
				best.second,
				{ ast::expression::lvalue_reference, ast::remove_lvalue_reference(ret_t) }
			};
		}
		else
		{
			return {
				best.second,
				{ ast::expression::rvalue, ast::remove_const(ret_t) }
			};
		}
	}
}

auto parse_context::get_function_call_body_and_type(ast::expr_function_call const &func_call)
	-> std::pair<ast::function_body *, ast::expression::expr_type_t>
{
	std::pair<ast::function_body *, ast::expression::expr_type_t> error_result = {
		nullptr, { ast::expression::rvalue, ast::typespec() }
	};

	if (func_call.called.expr_type.type_kind == ast::expression::function_name)
	{
		bz_assert(func_call.called.is<ast::expr_identifier>());

		auto const id = func_call.called.get<ast::expr_identifier_ptr>()->identifier->value;
		bz::vector<std::pair<match_level, ast::function_body *>> possible_funcs = {};

		// we go through the scope decls for a matching declaration
		for (
			auto scope = this->scope_decls.rbegin();
			scope != this->scope_decls.rend();
			++scope
		)
		{
			auto const set = std::find_if(
				scope->func_sets.begin(), scope->func_sets.end(),
				[id](auto const &fn_set) {
					return id == fn_set.id;
				}
			);
			if (set != scope->func_sets.end())
			{
				for (auto &fn : set->func_decls)
				{
					auto const match_level = get_function_call_match_level(fn->body, func_call, *this);
					if (match_level.min != -1)
					{
						possible_funcs.push_back({ match_level, &fn->body });
					}
				}
			}
		}

		auto const scope_decl_count = possible_funcs.size();

		auto &export_decls = this->global_ctx._export_decls;
		auto const global_set = std::find_if(
			export_decls.func_sets.begin(), export_decls.func_sets.end(),
			[id](auto const &fn_set) {
				return id == fn_set.id;
			}
		);
		if (global_set != export_decls.func_sets.end())
		{
			for (auto &fn : global_set->func_decls)
			{
				auto const match_level = get_function_call_match_level(fn->body, func_call, *this);
				if (match_level.min != -1)
				{
					possible_funcs.push_back({ match_level, &fn->body });
				}
			}
		}

		auto const best = find_best_match(possible_funcs, scope_decl_count);
		if (best.first.min == -1)
		{
			this->report_error(func_call, "couldn't match the function call to any of the overloads");
			return error_result;
		}
		else
		{
			auto &ret_t = best.second->return_type;
			if (ret_t.is<ast::ts_reference>())
			{
				return {
					best.second,
					{ ast::expression::lvalue_reference, ast::remove_lvalue_reference(ret_t) }
				};
			}
			else
			{
				return {
					best.second,
					{ ast::expression::rvalue, ast::remove_const(ret_t) }
				};
			}
		}
	}
	else
	{
		// function call operator
		return error_result;
	}
}

auto parse_context::get_cast_body_and_type(ast::expr_cast const &cast)
	-> std::pair<ast::function_body *, ast::expression::expr_type_t>
{
	auto res = get_built_in_cast_type(cast.expr.expr_type, cast.type, *this);
	if (res.expr_type.kind() == ast::typespec::null)
	{
		this->report_error(cast, bz::format("invalid cast from '{}' to '{}'", cast.expr.expr_type.expr_type, cast.type));
	}
	return { nullptr, std::move(res) };
}

/*
ast::expression::expr_type_t parse_context::get_operation_type(ast::expr_unary_op const &unary_op)
{
	if (!lex::is_overloadable_unary_operator(unary_op.op->kind))
	{
		auto type = get_non_overloadable_operation_type(
			unary_op.expr.expr_type, unary_op.op->kind,
			*this
		);
		if (type.has_error())
		{
			this->report_error(unary_op, std::move(type.get_error()));
			return { ast::expression::rvalue, ast::typespec() };
		}
		else
		{
			return std::move(type.get_result());
		}
	}

	if (is_built_in_type(ast::remove_const(unary_op.expr.expr_type.expr_type)))
	{
		auto type = get_built_in_operation_type(
			unary_op.expr.expr_type, unary_op.op->kind,
			*this
		);
		if (type.expr_type.kind() != ast::typespec::null)
		{
			return type;
		}
	}

	auto const report_undeclared_error = [&, this]()
	{
		this->report_error(
			unary_op,
			bz::format(
				"undeclared unary operator {} with type '{}'",
				unary_op.op->value,
				unary_op.expr.expr_type.expr_type
			)
		);
	};

	auto &op_sets = this->global_ctx.get_local_decls(this->scope).op_sets;
	auto const set = std::find_if(
		op_sets.begin(), op_sets.end(),
		[&](auto &set) {
			return set.op == unary_op.op->kind;
		}
	);

	if (set == op_sets.end())
	{
		report_undeclared_error();
		return { ast::expression::rvalue, ast::typespec() };
	}

	for (auto op : set->op_decls)
	{
		if (op->body.params.size() != 1)
		{
			continue;
		}

		if (are_directly_matchable_types(unary_op.expr.expr_type, op->body.params[0].var_type))
		{
			auto &t = op->body.return_type;
			if (t.is<ast::ts_reference>())
			{
				return {
					ast::expression::lvalue_reference,
					t.get<ast::ts_reference_ptr>()->base
				};
			}
			else
			{
				return { ast::expression::rvalue, t };
			}
		}
	}

	report_undeclared_error();
	return { ast::expression::rvalue, ast::typespec() };
}

ast::expression::expr_type_t parse_context::get_operation_type(ast::expr_binary_op const &binary_op)
{
	if (!lex::is_overloadable_binary_operator(binary_op.op->kind))
	{
		auto type = get_non_overloadable_operation_type(
			binary_op.lhs.expr_type, binary_op.rhs.expr_type,
			binary_op.op->kind,
			*this
		);
		if (type.has_error())
		{
			this->report_error(binary_op, std::move(type.get_error()));
			return { ast::expression::rvalue, ast::typespec() };
		}
		else
		{
			return std::move(type.get_result());
		}
	}

	if (
		is_built_in_type(ast::remove_const(binary_op.lhs.expr_type.expr_type))
		&& is_built_in_type(ast::remove_const(binary_op.rhs.expr_type.expr_type))
	)
	{
		auto type = get_built_in_operation_type(
			binary_op.lhs.expr_type, binary_op.rhs.expr_type,
			binary_op.op->kind,
			*this
		);
		if (type.expr_type.kind() != ast::typespec::null)
		{
			return type;
		}
	}

	auto const report_undeclared_error = [&, this]()
	{
		if (binary_op.op->kind == lex::token::square_open)
		{
			this->report_error(
				binary_op,
				bz::format(
					"undeclared binary operator [] with types '{}' and '{}'",
					binary_op.lhs.expr_type.expr_type, binary_op.rhs.expr_type.expr_type
				)
			);
		}
		else
		{
			this->report_error(
				binary_op,
				bz::format(
					"undeclared binary operator {} with types '{}' and '{}'",
					binary_op.op->value,
					binary_op.lhs.expr_type.expr_type, binary_op.rhs.expr_type.expr_type
				)
			);
		}
	};

	auto &op_sets = this->global_ctx.get_local_decls(this->scope).op_sets;
	auto const set = std::find_if(
		op_sets.begin(),
		op_sets.end(),
		[&](auto &set) {
			return set.op == binary_op.op->kind;
		}
	);

	if (set == op_sets.end())
	{
		report_undeclared_error();
		return { ast::expression::rvalue, ast::typespec() };
	}

	for (auto op : set->op_decls)
	{
		if (op->body.params.size() != 2)
		{
			continue;
		}

		if (
			are_directly_matchable_types(binary_op.lhs.expr_type, op->body.params[0].var_type)
			&& are_directly_matchable_types(binary_op.rhs.expr_type, op->body.params[1].var_type)
		)
		{
			auto &t = op->body.return_type;
			if (t.is<ast::ts_reference>())
			{
				return {
					ast::expression::lvalue_reference,
					t.get<ast::ts_reference_ptr>()->base
				};
			}
			else
			{
				return { ast::expression::rvalue, t };
			}
		}
	}

	report_undeclared_error();
	return { ast::expression::rvalue, ast::typespec() };
}



ast::expression::expr_type_t parse_context::get_function_call_type(ast::expr_function_call const &func_call) const
{
	auto res = this->global_ctx.get_function_call_type(this->scope, func_call);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
		return { ast::expression::rvalue, ast::typespec() };
	}
	else
	{
		return std::move(res.get_result());
	}
}
*/

bool parse_context::is_convertible(ast::expression::expr_type_t const &from, ast::typespec const &to)
{
	return are_directly_matchable_types(from, to);
}

ast::type_info *parse_context::get_type_info(bz::string_view id) const
{
	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const it = std::find_if(
			scope->type_infos.rbegin(), scope->type_infos.rend(),
			[id](auto const &info) {
				return id == info.id;
			}
		);
		if (it != scope->type_infos.rend())
		{
			return it->info;
		}
	}

	auto &export_decls = this->global_ctx._export_decls;
	auto const global_it = std::find_if(
		export_decls.type_infos.begin(), export_decls.type_infos.end(),
		[id](auto const &info) {
			return id == info.id;
		}
	);

	if (global_it != export_decls.type_infos.end())
	{
		return global_it->info;
	}
	else
	{
		return nullptr;
	}
}

} // namespace ctx
