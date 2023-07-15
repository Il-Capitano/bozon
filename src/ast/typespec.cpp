#include "typespec.h"
#include "statement.h"

namespace ast
{

bool typespec_view::is_safe_blind_get(void) const noexcept
{
	return this->modifiers.not_empty();
}

typespec_view typespec_view::blind_get(void) const noexcept
{
	return typespec_view{ this->src_tokens, { this->modifiers.begin() + 1, this->modifiers.end() }, this->terminator };
}

bool typespec_view::is_typename(void) const noexcept
{
	if (this->is_empty())
	{
		return false;
	}

	return this->terminator->visit(bz::overload{
		[](ts_unresolved const &) {
			return false;
		},
		[](ts_base_type const &) {
			return false;
		},
		[](ts_enum const &) {
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
	});
}

bool typespec_view::is_optional_pointer_like(void) const noexcept
{
	if (this->modifiers.size() >= 2
		&& this->modifiers[0].is<ts_optional>()
		&& (this->modifiers[1].is<ts_pointer>() || this->modifiers[1].is<ast::ts_lvalue_reference>())
	)
	{
		return true;
	}
	else
	{
		return this->modifiers.size() == 1
			&& this->modifiers[0].is<ast::ts_optional>()
			&& this->terminator->is<ast::ts_function>();
	}
}

bool typespec_view::is_optional_pointer(void) const noexcept
{
	return this->modifiers.size() >= 2
		&& this->modifiers[0].is<ts_optional>()
		&& this->modifiers[1].is<ts_pointer>();
}

typespec_view typespec_view::get_optional_pointer(void) const noexcept
{
	bz_assert(this->is_optional_pointer());
	return this->get<ts_optional>().get<ts_pointer>();
}

bool typespec_view::is_optional_reference(void) const noexcept
{
	return this->modifiers.size() >= 2
		&& this->modifiers[0].is<ts_optional>()
		&& this->modifiers[1].is<ts_lvalue_reference>();
}

typespec_view typespec_view::get_optional_reference(void) const noexcept
{
	bz_assert(this->is_optional_reference());
	return this->get<ts_optional>().get<ts_lvalue_reference>();
}

bool typespec_view::is_optional_function(void) const noexcept
{
	return this->modifiers.size() == 1
		&& this->modifiers[0].is<ts_optional>()
		&& this->terminator->is<ts_function>();
}

ts_function const &typespec_view::get_optional_function(void) const noexcept
{
	bz_assert(this->is_optional_function());
	return this->get<ts_optional>().get<ts_function>();
}

typespec::typespec(
	lex::src_tokens const &_src_tokens,
	arena_vector<modifier_typespec_node_t> _modifiers,
	ast_unique_ptr<terminator_typespec_node_t> _terminator
)
	: src_tokens(_src_tokens),
	  modifiers(std::move(_modifiers)),
	  terminator(std::move(_terminator))
{}

typespec::typespec(typespec_view ts)
	: src_tokens(ts.src_tokens),
	  modifiers(ts.modifiers),
	  terminator()
{
	if (ts.terminator != nullptr)
	{
		this->terminator = make_ast_unique<terminator_typespec_node_t>(*ts.terminator);
	}
}

typespec::typespec(typespec const &other)
	: src_tokens(other.src_tokens),
	  modifiers(other.modifiers),
	  terminator()
{
	if (other.terminator != nullptr)
	{
		this->terminator = make_ast_unique<terminator_typespec_node_t>(*other.terminator);
	}
}

typespec &typespec::operator = (typespec const &other)
{
	if (this == &other)
	{
		return *this;
	}

	this->src_tokens = other.src_tokens;
	this->modifiers = other.modifiers;
	if (this->terminator != nullptr && other.terminator != nullptr)
	{
		*this->terminator = *other.terminator;
	}
	else if (other.terminator != nullptr)
	{
		this->terminator = make_ast_unique<terminator_typespec_node_t>(*other.terminator);
	}
	else
	{
		this->terminator = nullptr;
	}

	return *this;
}

bool typespec::is_unresolved(void) const noexcept
{
	return this->terminator != nullptr && this->modifiers.empty() && this->terminator->is<ts_unresolved>();
}

bool typespec::is_empty(void) const noexcept
{
	return this->terminator == nullptr;
}

bool typespec::not_empty(void) const noexcept
{
	return this->terminator != nullptr;
}

ts_function &typespec::get_optional_function(void) noexcept
{
	bz_assert(this->is_optional_function());
	return this->terminator->get<ts_function>();
}

uint64_t typespec_view::modifier_kind(void) const noexcept
{
	return this->modifiers.empty() ? uint64_t(-1) : this->modifiers[0].index();
}

uint64_t typespec_view::terminator_kind(void) const noexcept
{
	return this->terminator == nullptr ? uint64_t(-1) : this->terminator->index();
}

bool typespec_view::same_kind_as(typespec_view const &other) const noexcept
{
	if (this->modifiers.not_empty())
	{
		return other.modifiers.not_empty() && this->modifier_kind() == other.modifier_kind();
	}
	else
	{
		return other.modifiers.empty() && this->terminator_kind() == other.terminator_kind();
	}
}

void typespec::clear(void) noexcept
{
	this->modifiers.clear();
	this->terminator = nullptr;
}

void typespec::copy_from(typespec_view pos, typespec_view source)
{
	auto const it_pos = pos.modifiers.empty() ? this->modifiers.end() : pos.modifiers.begin();
	bz_assert(it_pos >= this->modifiers.begin() && it_pos <= this->modifiers.end());
	bz_assert(pos.modifiers.empty() || pos.modifiers.end() == this->modifiers.end());
	bz_assert(source.modifiers.end() <= this->modifiers.begin() || source.modifiers.begin() >= this->modifiers.end());
	bz_assert(pos.terminator == this->terminator.get());
	auto const it_index = static_cast<size_t>(it_pos - this->modifiers.begin());
	this->modifiers.resize(it_index + source.modifiers.size());
	auto it = this->modifiers.begin() + it_index;
	auto const end = this->modifiers.end();
	auto src_it = source.modifiers.begin();
	for (; it != end; ++it, ++src_it)
	{
		*it = *src_it;
	}
	bz_assert(this->terminator != nullptr);
	bz_assert(source.terminator != nullptr);
	*this->terminator = *source.terminator;
}

void typespec::move_from(typespec_view pos, typespec &source)
{
	auto const it_pos = pos.modifiers.empty() ? this->modifiers.end() : pos.modifiers.begin();
	bz_assert(it_pos >= this->modifiers.begin() && it_pos <= this->modifiers.end());
	bz_assert(pos.modifiers.empty() || pos.modifiers.end() == this->modifiers.end());
	bz_assert(source.modifiers.end() < this->modifiers.begin() || source.modifiers.begin() >= this->modifiers.end());
	bz_assert(pos.terminator == this->terminator.get());
	auto const it_index = static_cast<size_t>(it_pos - this->modifiers.begin());
	this->modifiers.resize(it_index + source.modifiers.size());
	auto it = this->modifiers.begin() + it_index;
	auto const end = this->modifiers.end();
	auto src_it = source.modifiers.begin();
	for (; it != end; ++it, ++src_it)
	{
		*it = std::move(*src_it);
	}
	bz_assert(this->terminator != nullptr);
	bz_assert(source.terminator != nullptr);
	this->terminator = std::move(source.terminator);
}

void typespec::move_from(typespec_view pos, typespec &&source)
{
	this->move_from(pos, source);
}


size_t typespec_hash::operator () (typespec_view ts) const noexcept
{
	size_t result = 0;
	for (auto const &modifier : ts.modifiers)
	{
		result = hash_combine(result, std::hash<size_t>()(modifier.index()));
	}

	if (ts.terminator != nullptr)
	{
		result = hash_combine(result, std::hash<size_t>()(ts.terminator->index()));
		static_assert(terminator_typespec_types::size() == 10);
		if (ts.terminator->is<ts_base_type>())
		{
			result = hash_combine(result, std::hash<void const *>()(ts.terminator->get<ts_base_type>().info));
		}
		else if (ts.terminator->is<ts_enum>())
		{
			result = hash_combine(result, std::hash<void const *>()(ts.terminator->get<ts_enum>().decl));
		}
		else if (ts.terminator->is<ts_function>())
		{
			auto const &function_type = ts.terminator->get<ts_function>();
			result = hash_combine(result, (*this)(function_type.return_type));
			for (auto const &param_type : function_type.param_types)
			{
				result = hash_combine(result, (*this)(param_type));
			}
		}
		else if (ts.terminator->is<ts_array>())
		{
			auto const &array_type = ts.terminator->get<ts_array>();
			result = hash_combine(result, std::hash<size_t>()(array_type.size));
			result = hash_combine(result, (*this)(array_type.elem_type));
		}
		else if (ts.terminator->is<ts_array_slice>())
		{
			auto const &slice_type = ts.terminator->get<ts_array_slice>();
			result = hash_combine(result, (*this)(slice_type.elem_type));
		}
		else if (ts.terminator->is<ts_tuple>())
		{
			auto const &tuple_type = ts.terminator->get<ts_tuple>();
			for (auto const &elem_type : tuple_type.types)
			{
				result = hash_combine(result, (*this)(elem_type));
			}
		}
	}

	return result;
}


template<typename ...Kinds>
static typespec_view remove_kind_helper(typespec_view ts) noexcept
{
	if (ts.modifiers.not_empty() && (ts.modifiers[0].is<Kinds>() || ...))
	{
		return typespec_view{ ts.src_tokens, { ts.modifiers.begin() + 1, ts.modifiers.end() }, ts.terminator };
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

typespec_view remove_optional(typespec_view ts) noexcept
{
	return remove_kind_helper<ts_optional>(ts);
}

typespec_view remove_consteval(typespec_view ts) noexcept
{
	return remove_kind_helper<ts_consteval>(ts);
}

typespec_view remove_mut(typespec_view ts) noexcept
{
	return remove_kind_helper<ts_mut>(ts);
}

typespec_view remove_mutability_modifiers(typespec_view ts) noexcept
{
	return remove_kind_helper<ts_mut, ts_consteval>(ts);
}

typespec_view remove_lvalue_or_move_reference(typespec_view ts) noexcept
{
	return remove_kind_helper<ts_lvalue_reference, ts_move_reference>(ts);
}

typespec_view remove_any_reference(typespec_view ts) noexcept
{
	return remove_kind_helper<ts_lvalue_reference, ts_move_reference, ts_auto_reference, ts_auto_reference_const>(ts);
}

bool is_complete(typespec_view ts) noexcept
{
	if (ts.is_empty())
	{
		return false;
	}

	auto const is_auto_ref_or_variadic = ts.modifiers.not_empty()
		&& (
			ts.modifiers[0].is<ts_variadic>()
			|| ts.modifiers.is_any([](auto const &mod) {
				return mod.template is_any<ts_auto_reference, ts_auto_reference_const>();
			})
		);

	static_assert(typespec_types::size() == 19);
	return !is_auto_ref_or_variadic && ts.terminator->visit(bz::overload{
		[](ts_base_type const &base_t) {
			return !base_t.info->is_generic();
		},
		[](ts_enum const &) {
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
			return array_t.size != 0 && is_complete(array_t.elem_type);
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

template<
	bool (type_info::*base_type_property_func)(void) const,
	bool default_value, typename ...exception_types
>
static bool type_property_helper(typespec_view ts) noexcept
{
	ts = remove_mutability_modifiers(ts);
	if ((ts.is<exception_types>() || ...))
	{
		return !default_value;
	}
	else if (ts.is<ts_base_type>())
	{
		bz_assert(ts.get<ts_base_type>().info->state >= ast::resolve_state::members);
		return (ts.get<ts_base_type>().info->*base_type_property_func)();
	}
	else if (ts.is<ts_tuple>())
	{
		return ts.get<ts_tuple>().types.is_all([](auto const &type) {
			return type_property_helper<base_type_property_func, default_value, exception_types...>(type);
		});
	}
	else if (ts.is<ts_array>())
	{
		return type_property_helper<base_type_property_func, default_value, exception_types...>(ts.get<ts_array>().elem_type);
	}
	else if (ts.is<ts_optional>())
	{
		if (ts.is_optional_pointer_like() || ts.is_optional_reference())
		{
			return bz::meta::is_in_types<ast::ts_pointer, exception_types...> ? !default_value : default_value;
		}
		else
		{
			return type_property_helper<base_type_property_func, default_value, exception_types...>(ts.get<ts_optional>());
		}
	}
	else
	{
		return default_value;
	}
}

bool is_trivially_relocatable(typespec_view ts) noexcept
{
	return type_property_helper<
		&type_info::is_trivially_relocatable,
		false, ts_enum, ts_pointer, ts_array_slice
	>(ts);
}

bz::u8string typespec_view::get_symbol_name(void) const
{
	bz_assert(this->not_empty());
	static_assert(typespec_types::size() == 19);
	bz::u8string result = "";
	for (auto &modifier : this->modifiers)
	{
		modifier.visit(bz::overload{
			[&](ts_mut const &) {
				result += "mut.";
			},
			[&](ts_consteval const &) {
				result += "consteval.";
			},
			[&](ts_pointer const &) {
				result += "0P.";
			},
			[&](ts_optional const &) {
				result += "0O.";
			},
			[&](ts_lvalue_reference const &) {
				result += "0R.";
			},
			[&](ts_move_reference const &) {
				result += "0M.";
			},
			[&](ts_variadic const &) {
				result += "0V";
			},
			[](auto const &) {
				bz_unreachable;
			}
		});
	}
	this->terminator->visit(bz::overload{
		[&](ts_base_type const &base_type) {
			result += base_type.info->symbol_name;
		},
		[&](ts_enum const &enum_type) {
			if (enum_type.decl->id.is_qualified)
			{
				result += bz::format("enum.{}", enum_type.decl->id.format_as_unqualified());
			}
			else
			{
				result += bz::format("non_global_enum.{}", enum_type.decl->id.format_as_unqualified());
			}
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
		[&](ts_function const &fn_t) {
			result += bz::format("0F.{}.{}", static_cast<int>(fn_t.cc), fn_t.param_types.size());
			for (auto const &param_type : fn_t.param_types)
			{
				result += '.';
				result += param_type.get_symbol_name();
			}
			result += '.';
			result += fn_t.return_type.get_symbol_name();
		},
		[](auto const &) {
			bz_unreachable;
		}
	});

	return result;
}

bz::u8string typespec::decode_symbol_name(
	bz::u8string_view::const_iterator &it,
	bz::u8string_view::const_iterator end
)
{
	static_assert(typespec_types::size() == 19);
	constexpr bz::u8string_view pointer        = "0P.";
	constexpr bz::u8string_view optional       = "0O.";
	constexpr bz::u8string_view reference      = "0R.";
	constexpr bz::u8string_view move_reference = "0M.";
	constexpr bz::u8string_view mut            = "mut.";
	constexpr bz::u8string_view consteval_     = "consteval.";
	constexpr bz::u8string_view variadic       = "0V.";

	constexpr bz::u8string_view void_       = "void";
	constexpr bz::u8string_view array       = "0A.";
	constexpr bz::u8string_view array_slice = "0S.";
	constexpr bz::u8string_view tuple       = "0T.";
	constexpr bz::u8string_view typename_   = "0N.";
	constexpr bz::u8string_view function    = "0F.";
	constexpr bz::u8string_view global_enum     = "enum.";
	constexpr bz::u8string_view non_global_enum = "non_global_enum.";

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
		else if (symbol_name.starts_with(optional))
		{
			result += "?";
			it = bz::u8string_view::const_iterator(it.data() + optional.size());
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
		else if (symbol_name.starts_with(mut))
		{
			result += "mut ";
			it = bz::u8string_view::const_iterator(it.data() + mut.size());
		}
		else if (symbol_name.starts_with(consteval_))
		{
			result += "consteval ";
			it = bz::u8string_view::const_iterator(it.data() + consteval_.size());
		}
		else if (symbol_name.starts_with(variadic))
		{
			result += "...";
			it = bz::u8string_view::const_iterator(it.data() + variadic.size());
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
		else if (symbol_name.starts_with(typename_))
		{
			result += "typename";
			break;
		}
		else if (symbol_name.starts_with(function))
		{
			auto const cc_begin = bz::u8string_view::const_iterator(it.data() + tuple.size());
			auto const cc_end = symbol_name.find(cc_begin, '.');
			auto const cc = static_cast<abi::calling_convention>(parse_int(bz::u8string_view(cc_begin, cc_end)));
			result += bz::format("function \"{}\" (", abi::to_string(cc));
			auto const params_count_begin = bz::u8string_view::const_iterator(cc_end.data() + 1);
			auto const params_count_end = symbol_name.find(params_count_begin, '.');
			auto const params_count = parse_int(bz::u8string_view(params_count_begin, params_count_end));

			if (params_count == 0)
			{
				result += ')';
			}
			else
			{
				it = params_count_end + 1;
				result += decode_symbol_name(it, end);
				for ([[maybe_unused]] auto const _ : bz::iota(1, params_count))
				{
					result += ", ";
					bz_assert(*it == '.');
					++it;
					result += decode_symbol_name(it, end);
				}
				result += ")";
			}
			bz_assert(*it == '.');
			++it;
			result += bz::format(" -> {}", decode_symbol_name(it, end));

			break;
		}
		else if (symbol_name.starts_with(global_enum))
		{
			auto const begin = bz::u8string_view::const_iterator(it.data() + global_enum.size());
			result += bz::u8string_view(begin, end);
			break;
		}
		else if (symbol_name.starts_with(non_global_enum))
		{
			auto const begin = bz::u8string_view::const_iterator(it.data() + non_global_enum.size());
			result += bz::u8string_view(begin, end);
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
	if (lhs.is_empty() || rhs.is_empty())
	{
		return lhs.is_empty() && rhs.is_empty();
	}
	if (lhs.modifiers.size() != rhs.modifiers.size())
	{
		return false;
	}

	for (auto const &[lhs_modifier, rhs_modifier] : bz::zip(lhs.modifiers, rhs.modifiers))
	{
		if (lhs_modifier.index() != rhs_modifier.index())
		{
			return false;
		}
	}

	auto const &lhs_terminator = *lhs.terminator;
	auto const &rhs_terminator = *rhs.terminator;

	if (lhs_terminator.index() != rhs_terminator.index())
	{
		return false;
	}

	static_assert(terminator_typespec_types::size() == 10);
	switch (lhs_terminator.index())
	{
	case terminator_typespec_node_t::index_of<ts_base_type>:
	{
		auto const lhs_info = lhs_terminator.get<ts_base_type>().info;
		auto const rhs_info = rhs_terminator.get<ts_base_type>().info;
		return lhs_info == rhs_info;
	}
	case terminator_typespec_node_t::index_of<ts_enum>:
	{
		auto const lhs_decl = lhs_terminator.get<ts_enum>().decl;
		auto const rhs_decl = rhs_terminator.get<ts_enum>().decl;
		return lhs_decl == rhs_decl;
	}
	case terminator_typespec_node_t::index_of<ts_void>:
		return true;
	case terminator_typespec_node_t::index_of<ts_function>:
	{
		auto const &lhs_fn_t = lhs_terminator.get<ts_function>();
		auto const &rhs_fn_t = rhs_terminator.get<ts_function>();
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
	case terminator_typespec_node_t::index_of<ts_array>:
	{
		auto const &lhs_array_t = lhs_terminator.get<ts_array>();
		auto const &rhs_array_t = rhs_terminator.get<ts_array>();

		return lhs_array_t.size == rhs_array_t.size && lhs_array_t.elem_type == rhs_array_t.elem_type;
	}
	case terminator_typespec_node_t::index_of<ts_array_slice>:
	{
		auto const &lhs_array_slice_t = lhs_terminator.get<ts_array_slice>();
		auto const &rhs_array_slice_t = rhs_terminator.get<ts_array_slice>();

		return lhs_array_slice_t.elem_type == rhs_array_slice_t.elem_type;
	}
	case terminator_typespec_node_t::index_of<ts_tuple>:
	{
		auto const &lhs_tuple_t = lhs_terminator.get<ts_tuple>();
		auto const &rhs_tuple_t = rhs_terminator.get<ts_tuple>();
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
	case terminator_typespec_node_t::index_of<ts_auto>:
		return true;
	case terminator_typespec_node_t::index_of<ts_typename>:
		return true;
	default:
		bz_unreachable;
	}
}

type_prototype const *get_type_prototype(ast::typespec_view type, type_prototype_set_t &type_prototype_set)
{
	static_assert(ast::typespec_types::size() == 19);
	if (type.modifiers.empty())
	{
		switch (type.terminator_kind())
		{
		case ast::terminator_typespec_node_t::index_of<ast::ts_base_type>:
		{
			auto const info = type.get<ast::ts_base_type>().info;
			bz_assert(info->state >= ast::resolve_state::members);
			bz_assert(info->prototype != nullptr);
			return info->prototype;
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_enum>:
			return get_type_prototype(type.get<ast::ts_enum>().decl->underlying_type, type_prototype_set);
		case ast::terminator_typespec_node_t::index_of<ast::ts_void>:
			return type_prototype_set.get_builtin_type(builtin_type_kind::void_);
		case ast::terminator_typespec_node_t::index_of<ast::ts_function>:
			return type_prototype_set.get_pointer_type();
		case ast::terminator_typespec_node_t::index_of<ast::ts_array>:
		{
			auto &arr_t = type.get<ast::ts_array>();
			auto elem_t = get_type_prototype(arr_t.elem_type, type_prototype_set);
			return type_prototype_set.get_array_type(elem_t, arr_t.size);
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_array_slice>:
		{
			auto const pointer_type = type_prototype_set.get_pointer_type();
			type_prototype const *pointer_pair[2] = { pointer_type, pointer_type };
			return type_prototype_set.get_aggregate_type(pointer_pair);
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_tuple>:
		{
			auto &tuple_t = type.get<ast::ts_tuple>();
			auto const types = tuple_t.types
				.transform([&type_prototype_set](auto const &ts) { return get_type_prototype(ts, type_prototype_set); })
				.template collect<ast::arena_vector>();
			return type_prototype_set.get_aggregate_type(types);
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_auto>:
			bz_unreachable;
		case ast::terminator_typespec_node_t::index_of<ast::ts_unresolved>:
			bz_unreachable;
		case ast::terminator_typespec_node_t::index_of<ast::ts_typename>:
			bz_unreachable;
		default:
			bz_unreachable;
		}
	}
	else
	{
		switch (type.modifier_kind())
		{
		case ast::modifier_typespec_node_t::index_of<ast::ts_mut>:
			return get_type_prototype(type.get<ast::ts_mut>(), type_prototype_set);
		case ast::modifier_typespec_node_t::index_of<ast::ts_consteval>:
			return get_type_prototype(type.get<ast::ts_consteval>(), type_prototype_set);
		case ast::modifier_typespec_node_t::index_of<ast::ts_pointer>:
		case ast::modifier_typespec_node_t::index_of<ast::ts_lvalue_reference>:
		case ast::modifier_typespec_node_t::index_of<ast::ts_move_reference>:
			return type_prototype_set.get_pointer_type();
		case ast::modifier_typespec_node_t::index_of<ast::ts_optional>:
		{
			if (type.is_optional_pointer_like())
			{
				return type_prototype_set.get_pointer_type();
			}
			else
			{
				auto const inner_type = get_type_prototype(type.get<ast::ts_optional>(), type_prototype_set);
				type_prototype const *types[2] = { inner_type, type_prototype_set.get_builtin_type(builtin_type_kind::i1) };
				return type_prototype_set.get_aggregate_type(types);
			}
		}
		default:
			bz_unreachable;
		}
	}
}

} // namespace ast

bz::u8string bz::formatter<ast::typespec>::format(ast::typespec const &typespec, bz::u8string_view fmt_str)
{
	return bz::formatter<ast::typespec_view>::format(typespec, fmt_str);
}

bz::u8string bz::formatter<ast::typespec_view>::format(ast::typespec_view typespec, bz::u8string_view)
{
	static_assert(ast::typespec_types::size() == 19);
	if (typespec.is_empty())
	{
		return "<error-type>";
	}

	bz::u8string result = "";
	for (auto &modifier : typespec.modifiers)
	{
		modifier.visit(bz::overload{
			[&](ast::ts_mut const &) {
				result += "mut ";
			},
			[&](ast::ts_consteval const &) {
				result += "consteval ";
			},
			[&](ast::ts_pointer const &) {
				result += '*';
			},
			[&](ast::ts_optional const &) {
				result += '?';
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
	typespec.terminator->visit(bz::overload{
		[&](ast::ts_unresolved const &) {
			result += "<unresolved>";
		},
		[&](ast::ts_base_type const &base_type) {
			result += base_type.info->get_typename_as_string();
		},
		[&](ast::ts_enum const &enum_type) {
			result += enum_type.decl->id.format_as_unqualified();
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
			if (current_array_t->size != 0)
			{
				result += bz::format("{}", current_array_t->size);
			}
			else
			{
				result += "??";
			}
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
	});
	return result;
}
