#include "typespec.h"
#include "statement.h"

namespace ast
{

lex::src_tokens typespec_view::get_src_tokens(void) const noexcept
{
	lex::src_tokens src_tokens = {};
	for (auto &node : bz::reversed(this->nodes))
	{
		switch (node.index())
		{
		case typespec_node_t::index_of<ts_unresolved>:
		{
			auto const tokens = node.get<ts_unresolved>().tokens;
			src_tokens = { tokens.begin, tokens.begin, tokens.end };
			break;
		}
		case typespec_node_t::index_of<ts_base_type>:
			src_tokens = node.get<ts_base_type>().src_tokens;
			break;
		case typespec_node_t::index_of<ts_void>:
		{
			auto const void_pos = node.get<ts_void>().void_pos;
			if (void_pos != nullptr)
			{
				src_tokens = { void_pos, void_pos, void_pos + 1 };
			}
			break;
		}
		case typespec_node_t::index_of<ts_function>:
			src_tokens = node.get<ts_function>().src_tokens;
			break;
		case typespec_node_t::index_of<ts_tuple>:
			src_tokens = node.get<ts_tuple>().src_tokens;
			break;
		case typespec_node_t::index_of<ts_auto>:
		{
			auto const auto_pos = node.get<ts_auto>().auto_pos;
			if (auto_pos != nullptr)
			{
				src_tokens = { auto_pos, auto_pos, auto_pos + 1 };
			}
			break;
		}
		case ast::typespec_node_t::index_of<ast::ts_typename>:
		{
			auto const typename_pos = node.get<ts_typename>().typename_pos;
			if (typename_pos != nullptr)
			{
				src_tokens = { typename_pos, typename_pos, typename_pos + 1 };
			}
			break;
		}
		case ast::typespec_node_t::index_of<ast::ts_const>:
		{
			auto const const_pos = node.get<ts_const>().const_pos;
			if (src_tokens.begin == nullptr && const_pos != nullptr)
			{
				src_tokens = { const_pos, const_pos, const_pos + 1 };
			}
			else if (const_pos != nullptr)
			{
				bz_assert(const_pos < src_tokens.begin);
				src_tokens.begin = const_pos;
				src_tokens.pivot = const_pos;
			}
			break;
		}
		case ast::typespec_node_t::index_of<ast::ts_consteval>:
		{
			auto const consteval_pos = node.get<ts_consteval>().consteval_pos;
			if (src_tokens.begin == nullptr && consteval_pos != nullptr)
			{
				src_tokens = { consteval_pos, consteval_pos, consteval_pos + 1 };
			}
			else if (consteval_pos != nullptr)
			{
				bz_assert(consteval_pos < src_tokens.begin);
				src_tokens.begin = consteval_pos;
				src_tokens.pivot = consteval_pos;
			}
			break;
		}
		case ast::typespec_node_t::index_of<ast::ts_pointer>:
		{
			auto const pointer_pos = node.get<ts_pointer>().pointer_pos;
			if (src_tokens.begin == nullptr && pointer_pos != nullptr)
			{
				src_tokens = { pointer_pos, pointer_pos, pointer_pos + 1 };
			}
			else if (pointer_pos != nullptr)
			{
				bz_assert(pointer_pos < src_tokens.begin);
				src_tokens.begin = pointer_pos;
				src_tokens.pivot = pointer_pos;
			}
			break;
		}
		case ast::typespec_node_t::index_of<ast::ts_lvalue_reference>:
		{
			auto const reference_pos = node.get<ts_lvalue_reference>().reference_pos;
			if (src_tokens.begin == nullptr && reference_pos != nullptr)
			{
				src_tokens = { reference_pos, reference_pos, reference_pos + 1 };
			}
			else if (reference_pos != nullptr)
			{
				bz_assert(reference_pos < src_tokens.begin);
				src_tokens.begin = reference_pos;
				src_tokens.pivot = reference_pos;
			}
			break;
		}
		default:
			bz_assert(false);
			break;
		}
	}
	return src_tokens;
}

typespec_view typespec_view::blind_get(void) const noexcept
{
	return typespec_view{ { this->nodes.begin() + 1, this->nodes.end() } };
}

typespec::typespec(bz::vector<typespec_node_t> _nodes)
	: nodes(std::move(_nodes))
{}

typespec::typespec(typespec_view ts)
	: nodes(ts.nodes)
{}

bool typespec::is_unresolved(void) const noexcept
{
	return this->nodes.size() == 1 && this->nodes.front().is<ts_unresolved>();
}

uint64_t typespec_view::kind(void) const noexcept
{
	return this->nodes.size() == 0 ? uint64_t(-1) : this->nodes.front().index();
}

void typespec::clear(void) noexcept
{
	this->nodes.clear();
}

void typespec::copy_from(typespec_view pos, typespec_view source)
{
	auto const it_pos = pos.nodes.begin();
	bz_assert(it_pos >= this->nodes.begin() && it_pos < this->nodes.end());
	bz_assert(pos.nodes.end() == this->nodes.end());
	bz_assert(source.nodes.end() < this->nodes.begin() || source.nodes.begin() >= this->nodes.end());
	auto const it_index = static_cast<size_t>(it_pos - this->nodes.begin());
	this->nodes.resize(it_index + source.nodes.size());
	auto it = this->nodes.begin() + it_index;
	auto const end = this->nodes.end();
	auto src_it = source.nodes.begin();
	for (; it != end; ++it, ++src_it)
	{
		*it = *src_it;
	}
}


typespec_view remove_lvalue_reference(typespec_view ts) noexcept
{
	if (ts.nodes.size() != 0 && ts.nodes.front().is<ts_lvalue_reference>())
	{
		return typespec_view{{ ts.nodes.begin() + 1, ts.nodes.end() }};
	}
	else
	{
		return ts;
	}
}

typespec_view remove_const_or_consteval(typespec_view ts) noexcept
{
	if (
		ts.nodes.size() != 0
		&&(ts.nodes.front().is<ast::ts_const>() || ts.nodes.front().is<ast::ts_consteval>())
	)
	{
		return typespec_view{{ ts.nodes.begin() + 1, ts.nodes.end() }};
	}
	else
	{
		return ts;
	}
}

bool is_complete(typespec_view ts) noexcept
{
	if (ts.nodes.size() == 0)
	{
		return false;
	}

	auto &last = ts.nodes.back();
	switch (last.index())
	{
	case typespec_node_t::index_of<ts_base_type>:
	case typespec_node_t::index_of<ts_void>:
		return true;
	case typespec_node_t::index_of<ts_function>:
	{
		auto &fn_t = last.get<ts_function>();
		for (auto &param : fn_t.param_types)
		{
			if (!is_complete(param))
			{
				return false;
			}
		}
		return is_complete(fn_t.return_type);
	}
	case typespec_node_t::index_of<ts_tuple>:
	{
		auto &tuple_t = last.get<ts_tuple>();
		for (auto &t : tuple_t.types)
		{
			if (!is_complete(t))
			{
				return false;
			}
		}
		return true;
	}
	case typespec_node_t::index_of<ts_auto>:
	case typespec_node_t::index_of<ts_typename>:
		return false;
	default:
		return false;
	}
}

bool is_instantiable(typespec_view ts) noexcept
{
	if (ts.nodes.size() == 0)
	{
		return false;
	}

	auto &last = ts.nodes.back();
	switch (last.index())
	{
	case typespec_node_t::index_of<ts_base_type>:
		return (last.get<ts_base_type>().info->flags & type_info::instantiable) != 0;
	case typespec_node_t::index_of<ts_void>:
		return false;
	case typespec_node_t::index_of<ts_function>:
	{
		auto &fn_t = last.get<ts_function>();
		for (auto &param : fn_t.param_types)
		{
			if (!is_instantiable(param))
			{
				return false;
			}
		}
		return fn_t.return_type.is<ts_void>() || is_instantiable(fn_t.return_type);
	}
	case typespec_node_t::index_of<ts_tuple>:
	{
		auto &tuple_t = last.get<ts_tuple>();
		for (auto &t : tuple_t.types)
		{
			if (!is_instantiable(t))
			{
				return false;
			}
		}
		return true;
	}
	case typespec_node_t::index_of<ts_auto>:
	case typespec_node_t::index_of<ts_typename>:
		return false;
	default:
		return false;
	}
}

bz::u8string get_symbol_name_for_type(typespec_view ts)
{
	bz::u8string result = "";
	for (auto &node : ts.nodes)
	{
		switch (node.index())
		{
		case typespec_node_t::index_of<ts_unresolved>:
			bz_assert(false);
			break;
		case typespec_node_t::index_of<ts_base_type>:
			result += node.get<ts_base_type>().info->name;
			break;
		case typespec_node_t::index_of<ts_void>:
			result += "void";
			break;
		case typespec_node_t::index_of<ts_function>:
			bz_assert(false);
			break;
		case typespec_node_t::index_of<ts_tuple>:
			bz_assert(false);
			break;
		case typespec_node_t::index_of<ts_auto>:
			bz_assert(false);
			break;
		case typespec_node_t::index_of<ts_typename>:
			bz_assert(false);
			break;
		case typespec_node_t::index_of<ts_const>:
			result += "const.";
			break;
		case typespec_node_t::index_of<ts_consteval>:
			result += "consteval.";
			break;
		case typespec_node_t::index_of<ts_pointer>:
			result += "0P.";
			break;
		case typespec_node_t::index_of<ts_lvalue_reference>:
			result += "0R.";
			break;
		default:
			break;
		}
	}
	return result;
}

bool operator == (typespec_view lhs, typespec_view rhs)
{
	if (lhs.nodes.size() != rhs.nodes.size())
	{
		return false;
	}
	if (lhs.is_empty())
	{
		return true;
	}

	for (auto const &[lhs_node, rhs_node] : bz::zip(lhs.nodes, rhs.nodes))
	{
		if (lhs_node.index() != rhs_node.index())
		{
			return false;
		}
	}

	auto const &lhs_last = lhs.nodes.back();
	auto const &rhs_last = rhs.nodes.back();

	switch (lhs_last.index())
	{
	case typespec_node_t::index_of<ts_base_type>:
	{
		auto const lhs_info = lhs_last.get<ts_base_type>().info;
		auto const rhs_info = rhs_last.get<ts_base_type>().info;
		return lhs_info == rhs_info;
	}
	case typespec_node_t::index_of<ts_void>:
		return true;
	case typespec_node_t::index_of<ts_function>:
	{
		auto const &lhs_fn_t = lhs_last.get<ts_function>();
		auto const &rhs_fn_t = rhs_last.get<ts_function>();
		if (lhs_fn_t.param_types.size() != rhs_fn_t.param_types.size())
		{
			return false;
		}
		for (auto const &[lhs_p, rhs_p] : bz::zip(lhs_fn_t.param_types, rhs_fn_t.param_types))
		{
			if (lhs_p != rhs_p)
			{
				return false;
			}
		}
		return lhs_fn_t.return_type == rhs_fn_t.return_type;
	}
	case typespec_node_t::index_of<ts_tuple>:
	{
		auto const &lhs_tuple_t = lhs_last.get<ts_tuple>();
		auto const &rhs_tuple_t = rhs_last.get<ts_tuple>();
		if (lhs_tuple_t.types.size() != rhs_tuple_t.types.size())
		{
			return false;
		}
		for (auto const &[lhs_t, rhs_t] : bz::zip(lhs_tuple_t.types, rhs_tuple_t.types))
		{
			if (lhs_t != rhs_t)
			{
				return false;
			}
		}
		return true;
	}
	case typespec_node_t::index_of<ts_auto>:
		return true;
	case typespec_node_t::index_of<ts_typename>:
		return true;
	default:
		bz_assert(false);
		return false;
	}
}

} // namespace ast

bz::u8string bz::formatter<ast::typespec>::format(ast::typespec const &typespec, bz::u8string_view fmt_str)
{
	return bz::formatter<ast::typespec_view>::format(typespec, fmt_str);
}

bz::u8string bz::formatter<ast::typespec_view>::format(ast::typespec_view typespec, bz::u8string_view)
{
	bz::u8string result = "";
	for (auto &node : typespec.nodes)
	{
		switch (node.index())
		{
		case ast::typespec_node_t::index_of<ast::ts_unresolved>:
			result += "<unresolved>";
			break;
		case ast::typespec_node_t::index_of<ast::ts_base_type>:
			result += node.get<ast::ts_base_type>().info->name;
			break;
		case ast::typespec_node_t::index_of<ast::ts_void>:
			result += "void";
			break;
		case ast::typespec_node_t::index_of<ast::ts_function>:
		case ast::typespec_node_t::index_of<ast::ts_tuple>:
			bz_assert(false);
			break;
		case ast::typespec_node_t::index_of<ast::ts_auto>:
			result += "auto";
			break;
		case ast::typespec_node_t::index_of<ast::ts_typename>:
			result += "typename";
			break;
		case ast::typespec_node_t::index_of<ast::ts_const>:
			result += "const ";
			break;
		case ast::typespec_node_t::index_of<ast::ts_consteval>:
			result += "consteval ";
			break;
		case ast::typespec_node_t::index_of<ast::ts_pointer>:
			result += '*';
			break;
		case ast::typespec_node_t::index_of<ast::ts_lvalue_reference>:
			result += '&';
			break;
		default:
			result += "<error-type>";
			break;
		}
	}
	return result;
}
