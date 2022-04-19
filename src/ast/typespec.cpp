#include "typespec.h"
#include "statement.h"

namespace ast
{

/*

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
			},
			[&](ts_auto_reference const &auto_ref) {
				if (src_tokens.begin == nullptr && auto_ref.auto_reference_pos != nullptr)
				{
					src_tokens = {
						auto_ref.auto_reference_pos,
						auto_ref.auto_reference_pos,
						auto_ref.auto_reference_pos + 1
					};
				}
				else if (auto_ref.auto_reference_pos != nullptr)
				{
					bz_assert(auto_ref.auto_reference_pos < src_tokens.begin);
					src_tokens.begin = auto_ref.auto_reference_pos;
					src_tokens.pivot = auto_ref.auto_reference_pos;
				}
			},
			[&](ts_auto_reference_const const &auto_const_ref) {
				if (src_tokens.begin == nullptr && auto_const_ref.auto_reference_const_pos != nullptr)
				{
					src_tokens = {
						auto_const_ref.auto_reference_const_pos,
						auto_const_ref.auto_reference_const_pos,
						auto_const_ref.auto_reference_const_pos + 1
					};
				}
				else if (auto_const_ref.auto_reference_const_pos != nullptr)
				{
					bz_assert(auto_const_ref.auto_reference_const_pos < src_tokens.begin);
					src_tokens.begin = auto_const_ref.auto_reference_const_pos;
					src_tokens.pivot = auto_const_ref.auto_reference_const_pos;
				}
			}
		});
	}
	return src_tokens;
}

*/

bool typespec_view::is_safe_blind_get(void) const noexcept
{
	return [this]<typename ...Ts>(bz::meta::type_pack<Ts...>) {
		return ((!is_terminator_typespec<Ts> && this->is<Ts>()) || ...);
	}(typespec_types{});
}

typespec_view typespec_view::blind_get(void) const noexcept
{
	return typespec_view{ this->src_tokens, { this->nodes.begin() + 1, this->nodes.end() } };
}

bool typespec_view::is_typename(void) const noexcept
{
	if (this->nodes.empty())
	{
		return false;
	}

	return this->nodes.back().visit(bz::overload{
		[](ts_unresolved const &) {
			return false;
		},
		[](ts_base_type const &) {
			return false;
		},
		[](ts_void const &) {
			return false;
		},
		[](ts_function const &) {
			return false;
		},
		[](ts_array const &array_t) {
			return array_t.elem_type.is_typename();
		},
		[](ts_array_slice const &array_slice_t) {
			return array_slice_t.elem_type.is_typename();
		},
		[](ts_tuple const &tuple_t) {
			return tuple_t.types.is_any([](auto const &elem_type) { return elem_type.is_typename(); });
		},
		[](ts_auto const &) {
			return false;
		},
		[](ts_typename const &) {
			return true;
		},
		[](ts_const const &) {
			bz_unreachable;
			return false;
		},
		[](ts_consteval const &) {
			bz_unreachable;
			return false;
		},
		[](ts_pointer const &) {
			bz_unreachable;
			return false;
		},
		[](ts_lvalue_reference const &) {
			bz_unreachable;
			return false;
		},
		[](ts_move_reference const &) {
			bz_unreachable;
			return false;
		},
		[](ts_auto_reference const &) {
			bz_unreachable;
			return false;
		},
		[](ts_auto_reference_const const &) {
			bz_unreachable;
			return false;
		},
		[](ts_variadic const &) {
			bz_unreachable;
			return false;
		},
	});
}

typespec::typespec(lex::src_tokens _src_tokens, arena_vector<typespec_node_t> _nodes)
	: src_tokens(_src_tokens), nodes(std::move(_nodes))
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
	bz_assert(source.nodes.end() <= this->nodes.begin() || source.nodes.begin() >= this->nodes.end());
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
	this->move_from(pos, source);
}


template<typename ...Kinds>
static typespec_view remove_kind_helper(typespec_view ts) noexcept
{
	if (ts.nodes.size() != 0 && (ts.nodes.front().is<Kinds>() || ...))
	{
		return typespec_view{ ts.src_tokens, { ts.nodes.begin() + 1, ts.nodes.end() } };
	}
	else
	{
		return ts;
	}
}

typespec_view remove_lvalue_reference(typespec_view ts) noexcept
{
	return remove_kind_helper<ts_lvalue_reference>(ts);
}

typespec_view remove_pointer(typespec_view ts) noexcept
{
	return remove_kind_helper<ts_pointer>(ts);
}

typespec_view remove_const_or_consteval(typespec_view ts) noexcept
{
	return remove_kind_helper<ts_const, ts_consteval>(ts);
}

typespec_view remove_lvalue_or_move_reference(typespec_view ts) noexcept
{
	return remove_kind_helper<ts_lvalue_reference, ts_move_reference>(ts);
}

bool is_complete(typespec_view ts) noexcept
{
	if (ts.nodes.size() == 0)
	{
		return false;
	}

	auto const is_auto_ref_or_variadic = ts.nodes.front().is_any<ts_auto_reference, ts_auto_reference_const, ts_variadic>();

	return !is_auto_ref_or_variadic && ts.nodes.back().visit(bz::overload{
		[](ts_base_type const &base_t) {
			return !base_t.info->is_generic();
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

bool needs_destructor(typespec_view ts) noexcept
{
	ts = remove_const_or_consteval(ts);
	if (ts.is<ts_base_type>())
	{
		auto const &info = *ts.get<ts_base_type>().info;
		return info.destructor != nullptr
			|| info.member_variables.is_any([](auto const member) {
				return needs_destructor(member->get_type());
			});
	}
	else if (ts.is<ts_tuple>())
	{
		return ts.get<ts_tuple>().types.is_any([](auto const &type) {
			return needs_destructor(type);
		});
	}
	else if (ts.is<ts_array>())
	{
		return needs_destructor(ts.get<ts_array>().elem_type);
	}
	else
	{
		return false;
	}
}

bool is_non_trivial(typespec_view ts) noexcept
{
	ts = remove_const_or_consteval(ts);
	if (ts.is<ts_base_type>())
	{
		auto const &info = *ts.get<ts_base_type>().info;
		return info.destructor != nullptr || info.copy_constructor != nullptr
			|| info.member_variables.is_any([](auto const member) {
				return is_non_trivial(member->get_type());
			});
	}
	else if (ts.is<ts_tuple>())
	{
		return ts.get<ts_tuple>().types.is_any([](auto const &type) {
			return is_non_trivial(type);
		});
	}
	else if (ts.is<ts_array>())
	{
		return is_non_trivial(ts.get<ts_array>().elem_type);
	}
	else
	{
		return false;
	}
}

bool is_default_constructible(typespec_view ts) noexcept
{
	ts = remove_const_or_consteval(ts);
	if (ts.is<ts_base_type>())
	{
		return ts.get<ts_base_type>().info->is_default_constructible();
	}
	else if (ts.is<ts_tuple>())
	{
		return ts.get<ts_tuple>().types.is_all([](auto const &type) {
			return is_default_constructible(type);
		});
	}
	else if (ts.is<ts_array>())
	{
		return is_default_constructible(ts.get<ts_array>().elem_type);
	}
	else if (ts.is<ts_lvalue_reference>() || ts.is<ts_move_reference>())
	{
		return false;
	}
	else
	{
		// pointers, slices, function pointers are trivially copy constructible
		return true;
	}
}

bool is_copy_constructible(typespec_view ts) noexcept
{
	ts = remove_const_or_consteval(ts);
	if (ts.is<ts_base_type>())
	{
		return ts.get<ts_base_type>().info->is_copy_constructible();
	}
	else if (ts.is<ts_tuple>())
	{
		return ts.get<ts_tuple>().types.is_all([](auto const &type) {
			return is_copy_constructible(type);
		});
	}
	else if (ts.is<ts_array>())
	{
		return is_copy_constructible(ts.get<ts_array>().elem_type);
	}
	else
	{
		// pointers, slices, function pointers are trivially copy constructible
		return true;
	}
}

bool is_trivially_copy_constructible(typespec_view ts) noexcept
{
	ts = remove_const_or_consteval(ts);
	if (ts.is<ts_base_type>())
	{
		return ts.get<ts_base_type>().info->is_trivially_copy_constructible();
	}
	else if (ts.is<ts_tuple>())
	{
		return ts.get<ts_tuple>().types.is_all([](auto const &type) {
			return is_trivially_copy_constructible(type);
		});
	}
	else if (ts.is<ts_array>())
	{
		return is_trivially_copy_constructible(ts.get<ts_array>().elem_type);
	}
	else
	{
		// pointers, slices, function pointers are trivially copy constructible
		return true;
	}
}

bool is_trivially_destructible(typespec_view ts) noexcept
{
	ts = remove_const_or_consteval(ts);
	if (ts.is<ts_base_type>())
	{
		return ts.get<ts_base_type>().info->is_trivially_destructible();
	}
	else if (ts.is<ts_tuple>())
	{
		return ts.get<ts_tuple>().types.is_all([](auto const &type) {
			return is_trivially_destructible(type);
		});
	}
	else if (ts.is<ts_array>())
	{
		return is_trivially_destructible(ts.get<ts_array>().elem_type);
	}
	else
	{
		// pointers, slices, function pointers are trivially destructible
		return true;
	}
}

bool is_trivial(typespec_view ts) noexcept
{
	ts = remove_const_or_consteval(ts);
	if (ts.is<ts_base_type>())
	{
		return ts.get<ts_base_type>().info->is_trivial();
	}
	else if (ts.is<ts_tuple>())
	{
		return ts.get<ts_tuple>().types.is_all([](auto const &type) {
			return is_trivial(type);
		});
	}
	else if (ts.is<ts_array>())
	{
		return is_trivial(ts.get<ts_array>().elem_type);
	}
	else
	{
		// pointers, slices, function pointers are trivial
		return true;
	}
}

bool is_default_zero_initialized(typespec_view ts) noexcept
{
	ts = remove_const_or_consteval(ts);
	if (ts.is<ts_base_type>())
	{
		return ts.get<ts_base_type>().info->is_default_zero_initialized();
	}
	else if (ts.is<ts_tuple>())
	{
		return ts.get<ts_tuple>().types.is_all([](auto const &type) {
			return is_default_zero_initialized(type);
		});
	}
	else if (ts.is<ts_array>())
	{
		return is_default_zero_initialized(ts.get<ts_array>().elem_type);
	}
	else if (ts.is<ts_lvalue_reference>() || ts.is<ts_move_reference>())
	{
		return false;
	}
	else
	{
		// pointers, slices, function pointers are zero initialized
		return true;
	}
}

bz::u8string typespec_view::get_symbol_name(void) const
{
	static_assert(typespec_types::size() == 17);
	bz::u8string result = "";
	for (auto &node : this->nodes)
	{
		node.visit(bz::overload{
			[&](ts_base_type const &base_type) {
				result += base_type.info->symbol_name;
			},
			[&](ts_array const &array_t) {
				result += bz::format("0A.{}.", array_t.size);
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
			[&](ts_move_reference const &) {
				result += "0M.";
			},
			[&](ts_tuple const &tuple_t) {
				result += bz::format("0T.{}", tuple_t.types.size());
				for (auto const &elem_type : tuple_t.types)
				{
					result += '.';
					result += elem_type.get_symbol_name();
				}
			},
			[&](ts_typename const &) {
				result += "0N";
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
	static_assert(typespec_types::size() == 17);
	constexpr bz::u8string_view pointer        = "0P.";
	constexpr bz::u8string_view reference      = "0R.";
	constexpr bz::u8string_view move_reference = "0M.";
	constexpr bz::u8string_view const_         = "const.";
	constexpr bz::u8string_view consteval_     = "consteval.";

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
		else if (symbol_name.starts_with(move_reference))
		{
			result += "move ";
			it = bz::u8string_view::const_iterator(it.data() + move_reference.size());
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
			bool first = true;
			do
			{
				auto const size_end = symbol_name.find(it, '.');
				auto const size = parse_int(bz::u8string_view(it, size_end));
				if (first)
				{
					first = false;
					result += bz::format("{}", size);
				}
				else
				{
					result += bz::format(", {}", size);
				}
				it = size_end + 1;
			} while (bz::u8string_view(it, end).starts_with(array));
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

		return lhs_array_t.size == rhs_array_t.size && lhs_array_t.elem_type == rhs_array_t.elem_type;
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
	static_assert(ast::typespec_types::size() == 17);
	bz::u8string result = "";
	for (auto &node : typespec.nodes)
	{
		node.visit(bz::overload{
			[&](ast::ts_unresolved const &) {
				result += "<unresolved>";
			},
			[&](ast::ts_base_type const &base_type) {
				result += base_type.info->get_typename_as_string();
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
				ast::ts_array const *current_array_t = &array_t;
				result += bz::format("{}", current_array_t->size);
				while (current_array_t->elem_type.is<ast::ts_array>())
				{
					current_array_t = &current_array_t->elem_type.get<ast::ts_array>();
					result += bz::format(", {}", current_array_t->size);
				}
				result += bz::format(": {}]", current_array_t->elem_type);
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
			[&](ast::ts_move_reference const &) {
				result += "move ";
			},
			[&](ast::ts_auto_reference const &) {
				result += '#';
			},
			[&](ast::ts_auto_reference_const const &) {
				result += "##";
			},
			[&](ast::ts_variadic const &) {
				result += "...";
			},
			[&](auto const &) {
				result += "<error-type>";
			}
		});
	}
	if (result == "")
	{
		result = "<error-type>";
	}
	return result;
}
