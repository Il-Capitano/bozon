#ifndef AST_TYPE_PROTOTYPE_H
#define AST_TYPE_PROTOTYPE_H

#include "core.h"

namespace ast
{

struct type_prototype;

enum class builtin_type_kind : uint8_t
{
	i1, i8, i16, i32, i64,
	f32, f64,
	void_,
};

bool is_integer_kind(builtin_type_kind kind);
bool is_floating_point_kind(builtin_type_kind kind);

struct builtin_type
{
	builtin_type_kind kind;
};

struct pointer_type
{
};

struct aggregate_type
{
	bz::vector<type_prototype const *> elems;
	bz::vector<size_t> offsets;
	bool has_padding;
};

struct array_type
{
	type_prototype const *elem_type;
	size_t size;

	bool operator == (array_type const &rhs) const = default;
};


using type_base_t = bz::variant<builtin_type, pointer_type, aggregate_type, array_type>;

struct type_prototype : type_base_t
{
	size_t size;
	size_t align;

	template<typename T>
	type_prototype(T &&t, size_t _size, size_t _align)
		: type_base_t(std::forward<T>(t)),
		  size(_size),
		  align(_align)
	{}

	bool is_builtin(void) const
	{
		return this->is<builtin_type>();
	}

	builtin_type_kind get_builtin_kind(void) const
	{
		bz_assert(this->is_builtin());
		return this->get<builtin_type>().kind;
	}

	bool is_integer_type(void) const
	{
		return this->is_builtin() && is_integer_kind(this->get_builtin_kind());
	}

	bool is_floating_point_type(void) const
	{
		return this->is_builtin() && is_floating_point_kind(this->get_builtin_kind());
	}

	bool is_void(void) const
	{
		return this->is_builtin() && this->get_builtin_kind() == builtin_type_kind::void_;
	}

	bool is_pointer(void) const
	{
		return this->is<pointer_type>();
	}

	bool is_aggregate(void) const
	{
		return this->is<aggregate_type>();
	}

	bz::array_view<type_prototype const * const> get_aggregate_types(void) const
	{
		bz_assert(this->is_aggregate());
		return this->get<aggregate_type>().elems;
	}

	bz::array_view<size_t const> get_aggregate_offsets(void) const
	{
		bz_assert(this->is_aggregate());
		return this->get<aggregate_type>().offsets;
	}

	bool is_array(void) const
	{
		return this->is<array_type>();
	}

	type_prototype const *get_array_element_type(void) const
	{
		bz_assert(this->is_array());
		return this->get<array_type>().elem_type;
	}

	size_t get_array_size(void) const
	{
		bz_assert(this->is_array());
		return this->get<array_type>().size;
	}

	bool is_simple_value_type(void) const
	{
		return this->is_builtin() || this->is_pointer();
	}

	bool has_padding(void) const
	{
		if (this->is_aggregate())
		{
			return this->get<aggregate_type>().has_padding;
		}
		else if (this->is_array())
		{
			return this->get_array_element_type()->has_padding();
		}
		else
		{
			return false;
		}
	}

	bz::u8string to_string(void) const;

	friend bool operator == (type_prototype const &lhs, type_prototype const &rhs)
	{
		if (lhs.index() != rhs.index())
		{
			return false;
		}

		switch (lhs.index())
		{
		static_assert(variant_count == 4);
		case index_of<builtin_type>:
			return lhs.get_builtin_kind() == rhs.get_builtin_kind();
		case index_of<pointer_type>:
			return true;
		case index_of<aggregate_type>:
			return lhs.get_aggregate_types() == rhs.get_aggregate_types();
		case index_of<array_type>:
			return lhs.get_array_element_type() == rhs.get_array_element_type() && lhs.get_array_size() == rhs.get_array_size();
		default:
			return true;
		}
	}
};

} // namespace ast

template<>
struct std::hash<bz::array_view<ast::type_prototype const * const>>
{
	std::size_t operator () (bz::array_view<ast::type_prototype const * const> elem_types) const
	{
		std::size_t result = 0x9e84a579e70fd986; // start with a random value
		for (auto const elem_type : elem_types)
		{
			auto const elem_result = std::hash<ast::type_prototype const *>()(elem_type);
			// combine hashes
			result = (result << 3) + (result >> 7); // mangle result a bit
			result ^= elem_result;
		}
		return result;
	}
};

template<>
struct std::hash<ast::array_type>
{
	std::size_t operator () (ast::array_type const &array_type) const
	{
		auto const elem_hash = std::hash<ast::type_prototype const *>()(array_type.elem_type);
		auto const size_hash = std::hash<size_t>()(array_type.size);
		return elem_hash ^ ((size_hash << 3) + (size_hash >> 7));
	}
};


namespace ast
{

struct type_prototype_set_t
{
	type_prototype_set_t(size_t pointer_size);

	type_prototype_set_t(type_prototype_set_t const &) = delete;
	type_prototype_set_t(type_prototype_set_t &&) = delete;
	type_prototype_set_t &operator = (type_prototype_set_t const &) = delete;
	type_prototype_set_t &operator = (type_prototype_set_t &&) = delete;

	std::unordered_map<bz::array_view<type_prototype const * const>, type_prototype *> aggregate_map;
	std::unordered_map<array_type, type_prototype *> array_map;
	// stable addresses for every type
	std::list<type_prototype> aggregate_and_array_types;
	bz::array<type_prototype, 8> builtin_types;
	type_prototype pointer;

	type_prototype const *get_builtin_type(builtin_type_kind kind);
	type_prototype const *get_pointer_type(void);
	type_prototype const *get_aggregate_type(bz::array_view<type_prototype const * const> elem_types);
	type_prototype const *get_array_type(type_prototype const *elem_type, size_t size);
};

} // namespace ast

#endif // AST_TYPE_PROTOTYPE_H
