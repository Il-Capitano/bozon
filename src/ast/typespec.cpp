#include "typespec.h"
#include "statement.h"

namespace ast
{

lex::src_tokens typespec_view::get_src_tokens(void) const noexcept
{
	lex::src_tokens src_tokens = {};
	for (auto &node : this->nodes.reversed())
	{
		node.visit(bz::overload{
			[&](ts_unresolved const &unresolved) {
				src_tokens = { unresolved.tokens.begin, unresolved.tokens.begin, unresolved.tokens.end };
			},
			[&](ts_base_type const &base_type) {
				src_tokens = base_type.src_tokens;
			},
			[&](ts_void const &void_) {
				if (void_.void_pos != nullptr)
				{
					src_tokens = { void_.void_pos, void_.void_pos, void_.void_pos + 1 };
				}
			},
			[&](ts_function const &func_t) {
				src_tokens = func_t.src_tokens;
			},
			[&](ts_array const &array_t) {
				src_tokens = array_t.src_tokens;
			},
			[&](ts_array_slice const &array_slice_t) {
				src_tokens = array_slice_t.src_tokens;
			},
			[&](ts_tuple const &tuple) {
				src_tokens = tuple.src_tokens;
			},
			[&](ts_auto const &auto_) {
				if (auto_.auto_pos != nullptr)
				{
					src_tokens = { auto_.auto_pos, auto_.auto_pos, auto_.auto_pos + 1 };
				}
			},
			[&](ts_typename const &typename_) {
				if (typename_.typename_pos != nullptr)
				{
					src_tokens = { typename_.typename_pos, typename_.typename_pos, typename_.typename_pos + 1 };
				}
			},
			[&](ts_const const &const_) {
				if (src_tokens.begin == nullptr && const_.const_pos != nullptr)
				{
					src_tokens = { const_.const_pos, const_.const_pos, const_.const_pos + 1 };
				}
				else if (const_.const_pos != nullptr)
				{
					bz_assert(const_.const_pos < src_tokens.begin);
					src_tokens.begin = const_.const_pos;
					src_tokens.pivot = const_.const_pos;
				}
			},
			[&](ts_consteval const &consteval_) {
				if (src_tokens.begin == nullptr && consteval_.consteval_pos != nullptr)
				{
					src_tokens = { consteval_.consteval_pos, consteval_.consteval_pos, consteval_.consteval_pos + 1 };
				}
				else if (consteval_.consteval_pos != nullptr)
				{
					bz_assert(consteval_.consteval_pos < src_tokens.begin);
					src_tokens.begin = consteval_.consteval_pos;
					src_tokens.pivot = consteval_.consteval_pos;
				}
			},
			[&](ts_pointer const &pointer) {
				if (src_tokens.begin == nullptr && pointer.pointer_pos != nullptr)
				{
					src_tokens = { pointer.pointer_pos, pointer.pointer_pos, pointer.pointer_pos + 1 };
				}
				else if (pointer.pointer_pos != nullptr)
				{
					bz_assert(pointer.pointer_pos < src_tokens.begin);
					src_tokens.begin = pointer.pointer_pos;
					src_tokens.pivot = pointer.pointer_pos;
				}
			},
			[&](ts_lvalue_reference const &reference) {
				if (src_tokens.begin == nullptr && reference.reference_pos != nullptr)
				{
					src_tokens = { reference.reference_pos, reference.reference_pos, reference.reference_pos + 1 };
				}
				else if (reference.reference_pos != nullptr)
				{
					bz_assert(reference.reference_pos < src_tokens.begin);
					src_tokens.begin = reference.reference_pos;
					src_tokens.pivot = reference.reference_pos;
				}
			}
		});
	}
	return src_tokens;
}

bool typespec_view::is_safe_blind_get(void) const noexcept
{
	return [this]<typename ...Ts>(bz::meta::type_pack<Ts...>) {
		return ((!is_terminator_typespec<Ts> && this->is<Ts>()) || ...);
	}(typespec_types{});
}

typespec_view typespec_view::blind_get(void) const noexcept
{
	return typespec_view{ { this->nodes.begin() + 1, this->nodes.end() } };
}

typespec::typespec(arena_vector<typespec_node_t> _nodes)
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

void typespec::move_from(typespec_view pos, typespec &source)
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
		*it = std::move(*src_it);
	}
}

void typespec::move_from(typespec_view pos, typespec &&source)
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
		*it = std::move(*src_it);
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

typespec_view remove_pointer(typespec_view ts) noexcept
{
	if (ts.nodes.size() != 0 && ts.nodes.front().is<ts_pointer>())
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

	return ts.nodes.back().visit(bz::overload{
		[](ts_base_type const &) {
			return true;
		},
		[](ts_void const &) {
			return true;
		},
		[](ts_function const &fn_t) {
			for (auto &param : fn_t.param_types)
			{
				if (!is_complete(param))
				{
					return false;
				}
			}
			return is_complete(fn_t.return_type);
		},
		[](ts_array const &array_t) {
			return is_complete(array_t.elem_type);
		},
		[](ts_array_slice const &array_slice_t) {
			return is_complete(array_slice_t.elem_type);
		},
		[](ts_tuple const &tuple_t) {
			for (auto &t : tuple_t.types)
			{
				if (!is_complete(t))
				{
					return false;
				}
			}
			return true;
		},
		[](auto const &) {
			return false;
		}
	});
}

bz::u8string typespec_view::get_symbol_name(void) const
{
	bz::u8string result = "";
	for (auto &node : this->nodes)
	{
		node.visit(bz::overload{
			[&](ts_base_type const &base_type) {
				result += base_type.info->symbol_name;
			},
			[&](ts_array const &array_t) {
				result += bz::format("0A.{}.", array_t.sizes.size());
				for (auto const size : array_t.sizes)
				{
					result += bz::format("{}.", size);
				}
				result += array_t.elem_type.get_symbol_name();
			},
			[&](ts_array_slice const &array_slice_t) {
				result += bz::format("0S.{}", array_slice_t.elem_type.get_symbol_name());
			},
			[&](ts_void const &) {
				result += "void";
			},
			[&](ts_const const &) {
				result += "const.";
			},
			[&](ts_consteval const &) {
				result += "consteval.";
			},
			[&](ts_pointer const &) {
				result += "0P.";
			},
			[&](ts_lvalue_reference const &) {
				result += "0R.";
			},
			[&](ts_tuple const &tuple_t) {
				result += bz::format("0T.{}", tuple_t.types.size());
				for (auto const &elem_type : tuple_t.types)
				{
					result += '.';
					result += elem_type.get_symbol_name();
				}
			},
			[](auto const &) {
				bz_unreachable;
			}
		});
	}
	return result;
}

bz::u8string typespec::decode_symbol_name(
	bz::u8string_view::const_iterator &it,
	bz::u8string_view::const_iterator end
)
{
	constexpr bz::u8string_view pointer    = "0P.";
	constexpr bz::u8string_view reference  = "0R.";
	constexpr bz::u8string_view const_     = "const.";
	constexpr bz::u8string_view consteval_ = "consteval.";

	constexpr bz::u8string_view void_       = "void";
	constexpr bz::u8string_view array       = "0A.";
	constexpr bz::u8string_view array_slice = "0S.";
	constexpr bz::u8string_view tuple       = "0T.";

	auto const parse_int = [](bz::u8string_view str) {
		uint64_t result = 0;
		for (auto const c : str)
		{
			bz_assert(c >= '0' && c <= '9');
			result *= 10;
			result += c - '0';
		}
		return result;
	};

	bz::u8string result = "";

	while (it != end)
	{
		auto const symbol_name = bz::u8string_view(it, end);
		if (symbol_name.starts_with(pointer))
		{
			result += "*";
			it = bz::u8string_view::const_iterator(it.data() + pointer.size());
		}
		else if (symbol_name.starts_with(reference))
		{
			result += "&";
			it = bz::u8string_view::const_iterator(it.data() + reference.size());
		}
		else if (symbol_name.starts_with(const_))
		{
			result += "const ";
			it = bz::u8string_view::const_iterator(it.data() + const_.size());
		}
		else if (symbol_name.starts_with(consteval_))
		{
			result += "consteval ";
			it = bz::u8string_view::const_iterator(it.data() + consteval_.size());
		}
		else if (symbol_name.starts_with(array))
		{
			result += "[";
			auto const dim_begin = bz::u8string_view::const_iterator(it.data() + array.size());
			auto const dim_end = symbol_name.find(dim_begin, '.');
			auto const dim = parse_int(bz::u8string_view(dim_begin, dim_end));

			auto size_begin = dim_end + 1;
			for (auto const i : bz::iota(0, dim))
			{
				auto const size_end = symbol_name.find(size_begin, '.');
				auto const size = parse_int(bz::u8string_view(size_begin, size_end));
				if (i == 0)
				{
					result += bz::format("{}", size);
				}
				else
				{
					result += bz::format(", {}", size);
				}
				size_begin = size_end + 1;
			}
			it = size_begin;
			result += ": ";
			result += decode_symbol_name(it, end);
			result += "]";
			break;
		}
		else if (symbol_name.starts_with(array_slice))
		{
			it = bz::u8string_view::const_iterator(it.data() + array_slice.size());
			result += "[: ";
			result += decode_symbol_name(it, end);
			result += "]";
		}
		else if (symbol_name.starts_with(tuple))
		{
			result += "[";
			auto const types_count_begin = bz::u8string_view::const_iterator(it.data() + tuple.size());
			auto const types_count_end = symbol_name.find(types_count_begin, '.');
			auto const elem_types_count = parse_int(bz::u8string_view(types_count_begin, types_count_end));
			if (elem_types_count == 0)
			{
				result += "]";
			}
			else
			{
				it = types_count_end + 1;
				result += decode_symbol_name(it, end);
				for ([[maybe_unused]] auto const _ : bz::iota(1, elem_types_count))
				{
					result += ", ";
					bz_assert(*it == '.');
					++it;
					result += decode_symbol_name(it, end);
				}
				result += "]";
			}
			break;
		}
		else if (symbol_name.starts_with(void_))
		{
			result += "void";
			break;
		}
		else
		{
			result += type_info::decode_symbol_name(it, end);
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
	case typespec_node_t::index_of<ts_array>:
	{
		auto const &lhs_array_t = lhs_last.get<ts_array>();
		auto const &rhs_array_t = rhs_last.get<ts_array>();

		if (lhs_array_t.sizes.size() != rhs_array_t.sizes.size())
		{
			return false;
		}

		for (auto const [lhs_size, rhs_size] : bz::zip(lhs_array_t.sizes, rhs_array_t.sizes))
		{
			if (lhs_size != rhs_size)
			{
				return false;
			}
		}
		return lhs_array_t.elem_type == rhs_array_t.elem_type;
	}
	case typespec_node_t::index_of<ts_array_slice>:
	{
		auto const &lhs_array_slice_t = lhs_last.get<ts_array_slice>();
		auto const &rhs_array_slice_t = rhs_last.get<ts_array_slice>();

		return lhs_array_slice_t.elem_type == rhs_array_slice_t.elem_type;
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
		bz_unreachable;
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
		node.visit(bz::overload{
			[&](ast::ts_unresolved const &) {
				result += "<unresolved>";
			},
			[&](ast::ts_base_type const &base_type) {
				result += ast::type_info::decode_symbol_name(base_type.info->symbol_name);
			},
			[&](ast::ts_void const &) {
				result += "void";
			},
			[&](ast::ts_function const &func_t) {
				if (func_t.param_types.size() == 0)
				{
					result += bz::format("function() -> {}", func_t.return_type);
				}
				else
				{
					result += bz::format("function({}", func_t.param_types[0]);
					for (auto const &param_type : bz::array_view{ func_t.param_types.begin() + 1, func_t.param_types.end() })
					{
						result += bz::format(", {}", param_type);
					}
					result += bz::format(") -> {}", func_t.return_type);
				}
			},
			[&](ast::ts_array const &array_t) {
				result += "[";
				bz_assert(array_t.sizes.size() != 0);
				for (auto const size : bz::array_view{ array_t.sizes.begin(), array_t.sizes.end() - 1 })
				{
					result += bz::format("{}, ", size);
				}
				result += bz::format("{}: {}]", array_t.sizes.back(), array_t.elem_type);
			},
			[&](ast::ts_array_slice const &array_slice_t) {
				result += bz::format("[: {}]", array_slice_t.elem_type);
			},
			[&](ast::ts_tuple const &tuple_t) {
				if (tuple_t.types.size() == 0)
				{
					result += "[]";
				}
				else if (tuple_t.types.size() == 1)
				{
					result += bz::format("[{}]", tuple_t.types[0]);
				}
				else
				{
					result += bz::format("[{}", tuple_t.types[0]);
					for (auto const &type : bz::array_view{ tuple_t.types.begin() + 1, tuple_t.types.end() })
					{
						result += bz::format(", {}", type);
					}
					result += "]";
				}
			},
			[&](ast::ts_auto const &) {
				result += "auto";
			},
			[&](ast::ts_typename const &) {
				result += "typename";
			},
			[&](ast::ts_const const &) {
				result += "const ";
			},
			[&](ast::ts_consteval const &) {
				result += "consteval ";
			},
			[&](ast::ts_pointer const &) {
				result += '*';
			},
			[&](ast::ts_lvalue_reference const &) {
				result += '&';
			},
			[&](auto const &) {
				result += "<error-type>";
			}
		});
	}
	return result;
}
