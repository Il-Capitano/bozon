#ifndef CODEGEN_C_TYPES_H
#define CODEGEN_C_TYPES_H

#include "core.h"
#include "ast/type_prototype.h"
#include "ast/allocator.h"

namespace codegen::c
{

enum class type_modifier : uint8_t
{
	pointer,
	const_pointer,
};

struct pointer_modifier_info_t
{
	// the information of pointer modifiers are packed into 64 bits:
	// - the lower 6 bits store the number of pointer modifiers on the type
	// - the remaining bits act as a bitset, storing whether the pointer points
	//   to a const type (1 bit) or a mutable type (0 bit)
	// if the type has more than 58 pointer modifiers, then we just use a typedef
	uint64_t _value = 0;

	static inline constexpr uint64_t count_bits = 6;
	static inline constexpr uint64_t max_count = 64 - count_bits;
	static inline constexpr uint64_t count_mask = (uint64_t(1) << count_bits) - 1;
	static_assert(max_count <= count_mask);

	size_t get_pointer_modifiers_count(void) const
	{
		return this->_value & count_mask;
	}

	type_modifier top(void) const
	{
		auto const count = this->get_pointer_modifiers_count();
		auto const top_bit = (this->_value & (uint64_t(1) << (count + count_bits - 1)));
		if (top_bit == 0)
		{
			return type_modifier::pointer;
		}
		else
		{
			return type_modifier::const_pointer;
		}
	}

	bool empty(void) const
	{
		return this->_value == 0;
	}

	bool not_empty(void) const
	{
		return this->_value != 0;
	}

	[[nodiscard]] bool push(type_modifier modifier_kind)
	{
		auto const count = this->get_pointer_modifiers_count();
		if (count == max_count)
		{
			return false;
		}

		if (modifier_kind == type_modifier::const_pointer)
		{
			this->_value |= uint64_t(1) << (count + count_bits);
		}
		this->_value += 1; // increase count by one
		return true;
	}

	type_modifier pop(void)
	{
		auto const count = this->get_pointer_modifiers_count();
		bz_assert(count != 0);
		auto const top_bit = this->_value & (uint64_t(1) << (count + count_bits - 1));
		this->_value -= 1; // reduce count by one
		if (top_bit == 0)
		{
			return type_modifier::pointer;
		}
		else
		{
			this->_value ^= top_bit;
			return type_modifier::const_pointer;
		}
	}

	bool operator == (pointer_modifier_info_t const &rhs) const = default;

	size_t hash(void) const
	{
		return std::hash<uint64_t>()(this->_value);
	}

	struct reverse_iterator
	{
		uint64_t value;
		uint64_t current_bit;

		type_modifier operator * (void) const
		{
			return (this->value & this->current_bit) == 0 ? type_modifier::pointer : type_modifier::const_pointer;
		}

		bool operator == (reverse_iterator const &rhs) const = default;

		reverse_iterator &operator ++ (void)
		{
			this->current_bit <<= 1;
			return *this;
		}
	};

	bz::basic_range<reverse_iterator, reverse_iterator> reversed_range(void) const
	{
		auto const count = this->get_pointer_modifiers_count();
		auto const bits = this->_value;
		auto const begin_bit = uint64_t(1) << count_bits;
		auto const end_bit = count + count_bits >= 64 ? 0 : uint64_t(1) << (count + count_bits);
		return {
			reverse_iterator{ .value = bits, .current_bit = begin_bit },
			reverse_iterator{ .value = bits, .current_bit = end_bit },
		};
	}
};

struct type
{
	static inline constexpr uint32_t invalid_index = std::numeric_limits<uint32_t>::max();

	struct struct_reference
	{
		uint32_t index;

		bool operator == (struct_reference const &rhs) const = default;

		static constexpr struct_reference invalid(void)
		{
			return { invalid_index };
		}
	};

	struct typedef_reference
	{
		uint32_t index;

		bool operator == (typedef_reference const &rhs) const = default;

		static constexpr typedef_reference invalid(void)
		{
			return { invalid_index };
		}
	};

	struct array_reference
	{
		uint32_t index;

		bool operator == (array_reference const &rhs) const = default;

		static constexpr array_reference invalid(void)
		{
			return { invalid_index };
		}
	};

	struct function_reference
	{
		uint32_t index;

		bool operator == (function_reference const &rhs) const = default;

		static constexpr function_reference invalid(void)
		{
			return { invalid_index };
		}
	};

	using type_terminator_t = bz::variant<struct_reference, typedef_reference, array_reference, function_reference>;

	pointer_modifier_info_t modifier_info;
	type_terminator_t terminator;

	type(void) = default;

	explicit type(type_terminator_t _terminator)
		: modifier_info(),
		  terminator(_terminator)
	{}

	bool is_pointer(void) const
	{
		return this->modifier_info.not_empty();
	}

	std::pair<type, type_modifier> get_pointer(void) const
	{
		bz_assert(this->is_pointer());
		auto result = *this;
		auto const modifier = result.modifier_info.pop();
		return { result, modifier };
	}

	bool is_typedef(void) const
	{
		return this->modifier_info.empty() && this->terminator.is<typedef_reference>();
	}

	typedef_reference get_typedef(void) const
	{
		bz_assert(this->is_typedef());
		return this->terminator.get<typedef_reference>();
	}

	bool is_struct(void) const
	{
		return this->modifier_info.empty() && this->terminator.is<struct_reference>();
	}

	struct_reference get_struct(void) const
	{
		bz_assert(this->is_struct());
		return this->terminator.get<struct_reference>();
	}

	bool is_array(void) const
	{
		return this->modifier_info.empty() && this->terminator.is<array_reference>();
	}

	array_reference get_array(void) const
	{
		bz_assert(this->is_array());
		return this->terminator.get<array_reference>();
	}

	bool is_function(void) const
	{
		return this->modifier_info.empty() && this->terminator.is<function_reference>();
	}

	function_reference get_function(void) const
	{
		bz_assert(this->is_function());
		return this->terminator.get<function_reference>();
	}

	bool operator == (type const &rhs) const = default;

	size_t hash(void) const
	{
		size_t result = this->modifier_info.hash();
		result = hash_combine(result, std::hash<size_t>()(this->terminator.index()));
		switch (this->terminator.index())
		{
		static_assert(type_terminator_t::variant_count == 4);
		case type_terminator_t::index_of<struct_reference>:
			result = hash_combine(result, std::hash<uint32_t>()(this->terminator.get<struct_reference>().index));
			break;
		case type_terminator_t::index_of<typedef_reference>:
			result = hash_combine(result, std::hash<uint32_t>()(this->terminator.get<typedef_reference>().index));
			break;
		case type_terminator_t::index_of<array_reference>:
			result = hash_combine(result, std::hash<uint32_t>()(this->terminator.get<array_reference>().index));
			break;
		case type_terminator_t::index_of<function_reference>:
			result = hash_combine(result, std::hash<uint32_t>()(this->terminator.get<function_reference>().index));
			break;
		}
		return result;
	}
};

static_assert(sizeof (type) == 16);
static_assert(std::is_trivially_copy_constructible_v<type>);

struct struct_type_t
{
	ast::arena_vector<type> members;
};

struct typedef_type_t
{
	type aliased_type;
};

struct array_type_t
{
	type elem_type;
	size_t size;
};

struct function_type_t
{
	type return_type;
	ast::arena_vector<type> param_types;
};

struct type_set_t
{
	struct struct_type_view_t
	{
		bz::array_view<type const> members;

		bool operator == (struct_type_view_t const &rhs) const = default;

		struct hash
		{
			size_t operator () (struct_type_view_t const &struct_type) const
			{
				size_t result = 0;
				for (auto const &member : struct_type.members)
				{
					result = hash_combine(result, member.hash());
				}
				return result;
			}
		};
	};

	struct typedef_type_view_t
	{
		type aliased_type;

		bool operator == (typedef_type_view_t const &rhs) const = default;

		struct hash
		{
			size_t operator () (typedef_type_view_t const &typedef_type) const
			{
				return typedef_type.aliased_type.hash();
			}
		};
	};

	struct array_type_view_t
	{
		type elem_type;
		size_t size;

		bool operator == (array_type_view_t const &rhs) const = default;

		struct hash
		{
			size_t operator () (array_type_view_t const &array_type) const
			{
				return hash_combine(array_type.elem_type.hash(), std::hash<size_t>()(array_type.size));
			}
		};
	};

	struct function_type_view_t
	{
		type return_type;
		bz::array_view<type const> param_types;

		bool operator == (function_type_view_t const &rhs) const = default;

		struct hash
		{
			size_t operator () (function_type_view_t const &function_type) const
			{
				size_t result = function_type.return_type.hash();
				for (auto const &param_type : function_type.param_types)
				{
					result = hash_combine(result, param_type.hash());
				}
				return result;
			}
		};
	};

	std::unordered_map<struct_type_view_t, type::struct_reference, struct_type_view_t::hash> struct_types_map;
	std::unordered_map<typedef_type_view_t, type::typedef_reference, typedef_type_view_t::hash> typedef_types_map;
	std::unordered_map<array_type_view_t, type::array_reference, array_type_view_t::hash> array_types_map;
	std::unordered_map<function_type_view_t, type::function_reference, function_type_view_t::hash> function_types_map;

	bz::vector<struct_type_t> struct_types;
	bz::vector<typedef_type_t> typedef_types;
	bz::vector<array_type_t> array_types;
	bz::vector<function_type_t> function_types;

	bz::vector<bz::u8string> struct_type_names;
	bz::vector<bz::u8string> typedef_type_names;
	bz::vector<bz::u8string> array_type_names;
	bz::vector<bz::u8string> function_type_names;

	struct_type_t const &get_struct_type(type::struct_reference struct_ref) const;
	typedef_type_t const &get_typedef_type(type::typedef_reference typedef_ref) const;
	array_type_t const &get_array_type(type::array_reference array_ref) const;
	function_type_t const &get_function_type(type::function_reference function_ref) const;

	bz::u8string_view get_struct_type_name(type::struct_reference struct_ref) const;
	bz::u8string_view get_typedef_type_name(type::typedef_reference typedef_ref) const;
	bz::u8string_view get_array_type_name(type::array_reference array_ref) const;
	bz::u8string_view get_function_type_name(type::function_reference function_ref) const;

	std::pair<type::struct_reference, bool> add_struct_type(struct_type_t struct_type);
	std::pair<type::typedef_reference, bool> add_typedef_type(typedef_type_t typedef_type);
	std::pair<type::array_reference, bool> add_array_type(array_type_t array_type);
	std::pair<type::function_reference, bool> add_function_type(function_type_t function_type);

	void add_struct_type_name(type::struct_reference struct_ref, bz::u8string struct_type_name);
	void add_typedef_type_name(type::typedef_reference typedef_ref, bz::u8string typedef_type_name);
	void add_array_type_name(type::array_reference array_ref, bz::u8string array_type_name);
	void add_function_type_name(type::function_reference function_ref, bz::u8string function_type_name);

	type::struct_reference add_unique_struct(struct_type_t struct_type);
	type::typedef_reference add_unique_typedef(typedef_type_t typedef_type);

	void modify_struct(type::struct_reference struct_ref, struct_type_t struct_type);
};

} // namespace codegen::c

#endif // CODEGEN_C_TYPES_H
