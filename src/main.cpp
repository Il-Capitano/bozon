/*

TODO:
	- expression type evaluation for auto types
		kind of also done, need to clean up the function call and operator type checks
	- array type
	- user-defined type implementation






{
	let v: vector = { 1.0, 2.5, -0.9 };
	let w = std::move(v);
} // move d-tor of v

{
	let v: vector = { 1.0, 2.5, -0.9 };
	if (...)
	{
		let w = std::move(v);
	}
} // move d-tor or regular d-tor ??

set a flag in a register?
have different stack unwind labels?
maybe it could only be used when the compiler decides to?















struct<typename T> vector
{
private:
	_data: *T;
	_size: uint64;

public:
	constructor(init_list: { T... })
	[
		._data = new uint8[sizeof init_list];
		._size = sizeof init_list / sizeof T;
	]
	{
		const n: uint64... = 1 .. init_list.count();   // (?)
		(new(this._data + n) T(init_list[n])), ...;
		 ^^^ new operator syntax not final   ^ this is required, so it is parsed as an operator
		                ... by default makes a comma seperated list, not a comma operator list ?? (not sure)
	}

	operator [] (&const this, n: uint64) -> &const T
	{
		return *(this._data + n);
	}

	operator [] (&this, n: uint64) -> &T
	{
		return *(this._data + n);
	}

	function at(&const this, n: uint64) const -> &const T
	{
		if (n < this._size)
		{
			return this[n];
		}
		else
		{
			// error
		}
	}

	function at(&this, n: uint64) -> &T
	{
		if (n < this._size)
		{
			return this[n];
		}
		else
		{
			// error
		}
	}

	function size(&const this) -> uint64
	{
		return this._size;
	}
}

operator<typename T> [] (v: &const vector<T>, n: uint64) -> &const T
{
	return *(v._data + n);
}

//                     ˇˇˇˇˇˇ inferred template
operator [] (v: &const vector, n: uint64) -> &const (typeof v)::value_type
{
	DEBUG_ASSERT(n < v.size());
	return *(v._data + n);
}

operator [] (v: &vector, n: uint64) -> &(typeof v)::value_type
{
	DEBUG_ASSERT(n < v.size());
	return *(v._data + n);
}

let v: vector = { 1.0, 1.1, 5.3, math::sqrt(2.0) };

// typeof v == vector<float64>



types:
  integers:
	uint8
	uint16
	uint32
	uint64
	int8
	int16
	int32
	int64
	uint   uint32?
	int    int32?

  reals:
	float32
	float64

  other:
	bool
	void
	char



operators:
  unary:
	+x
	-x
	~x
	++x
	--x
	&x  address-of
	*x  de-reference
	!x

	x...

  binary:
	x = x
	x + x   x += x
	x - x   x -= x
	x * x   x *= x
	x / x   x /= x
	x % x   x %= x
	x | x   x |= x
	x & x   x &= x
	x ^ x   x ^= x
	x << x   x <<= x
	x >> x   x >>= x
	x || x
	x && x
	x ^^ x
	x == x
	x <=> x  (?)

	x .. x




// packages
import math; // will be in 'math' namespace
import vector as *;   // will be in global namspace
import array as arr;  // will be in 'arr' namespace

export // default namespacing (filename)
{
}

export as std  // exported as being in namespace std
{
}

export as *  // exported to the global namespace
{            // useful for operators for example
}



math::sqrt(2);
let v : vector<int>;


alternatively:
have the namespacing based on folders eg:

import std::vector;  // same as #include <std/vector>

so a regular 'import math;' would just import it into the global namespace
probably should be able to override it somehow (both import and export)





// templates:

not sure I like this syntax, maybe something simmilar to c++?

function<typename T> add(lhs: T, rhs: T)
{ return lhs + rhs; }

// note: these two are not the same!!  the template version should consider conversions?

function add(lhs, rhs)
requires (typeof lhs == typeof rhs)
{ return lhs + rhs; }

function<uint64 N> size(array: [N])
{ return N; }

function<uint64 N> begin(array: [N])
{ return &array[0]; }

functino<uint64 N> end(array: [N])
{ return &array[0] + N; }

operator<typename T1, typename T2> + (lhs: complex<T1>, rhs: complex<T2>)
	-> complex<typeof (lhs.re + rhs.re)>
{ return { lhs.re + rhs.re, lhs.im + rhs.im }; }

// same thing, but shorter
operator + (lhs: complex, rhs: complex)
	-> complex<typeof (lhs.re + rhs.re)>
{ return {lhs.re + rhs.re, lhs.im + rhs.im }; }

struct<typename T, typename U> pair
{
	first: T;
	second: U;
}

// templated variables?? (must be const)
const<int N> bit: uint64 = 1u << N;


// variable declaration
let x = 0;
let x: double = 0;
let x: double;

// implicit auto
//      ˇˇˇˇˇˇ === &const auto
let y: &const = x;
let y: & = x;   // same as   let y : &auto = x;

function add(lhs, rhs) { return lhs + rhs; }
function add(lhs: &const, rhs: &const) { return lhs + rhs; }

// tuple
typeof { 1, 2 } == { int, int };

{ 1, 2.4 }[1] == 2.4;

typeof 1..4 == [4]int;  // ??
maybe 1..4 -> 1,2,3,4
maybe it's just dependent on the context
in a for loop it is like [4]int, elsewhere not sure...
maybe a comma list  (like std::index_sequence)

let ...indicies: size_t = 0..9;
let indicies: ...size_t = 0..9;
let indicies: size_t... = 0..9;

struct<typename T> vector
{
	// ...

	// &this may be required here? maybe not...
	// doesn't really make sense, as at construction
	// constness doesn't matter
	constructor(tuple: &&{...T})
	[
		._data = new byte_t[sizeof tuple];
		._size = sizeof tuple / sizeof T;
	]
	{
		let ...i = 0 .. (sizeof tuple / sizeof T - 1)
		(new(this._data + i) T(std::forward<T>(tuple[i]))), ...;
		// which one ??
		T::constructor(this._data + i, std::forward<typeof tuple[i]>(tuple[i])), ...;
	}

	// ...
}


function<typename T> make_at_address(ptr: *T, args: ...&&)
{
	new(ptr) T(std::forward<typeof args>(args)...);
	        // ^^^^^^   std::forward may not be the same...
}


let v: vector = { 1..3 };  // v == { 1, 2, 3 }

typeof "Hello" == str ? string_literal;


for (i in 1..10)
{
	io::print("{}\n", i);
}

let v: vector<int> = { 1, 2, 3, 4, 5, 6 };
let v: vector      = { 1, 2, 3, 4, 5, 6 };

for (i in v)
{
	i *= 2;  // changes the elements of the vector
	         // i is implicitly   auto &   -> === for (i: & in v)
	         // maybe not? would be more consistent with the rest of the language
}

for (i: int in v)
{
	i *= 2;  // does not change the elements of the vector
	         // i was explicitly declared as int, so a copy was made
}

// traditional for loop
for (let i = 0; i < v.size(); ++i)
{
	v[i] *= 2;
}


let s: string = "Hello";
let s = "Hello"s;

s = s .. " World!";

s = s .. 3;


struct<typename T> vec2d
	requires tt::is_one_of<T, float32, float64>  // ::value ?
{
	x: T;
	y: T;

	constructor(x: T, y: T)
		[ x = x; y = y; ]
	{}

	constructor(t: std::tuple<T, T>)
		[ x = t[0]; y = t[1]; ]
	{}

	function length(this: &const)
	{
		return math::sqrt(this.x * this.x + this.y * this.y);
	}

	function normalize(this: &)
	{
		let len = this.length();
		let ilen = 1 / len;

		this.x *= ilen;
		this.y *= ilen;
	}
}


let a = vec2d<float>(1, 2);      // calls the (x, y) constructor
let v: vec2d<float> = { 1, 2 };  // calls the tuple constructor
let b = 1.0f; // float
let result: vec2d<double> = a * b;


operator + (lhs : vec2d<auto>, rhs : vec2d<auto>)
	-> vec2d<typeof (lhs.x + rhs.x)>
{ return { lhs.x + rhs.x, lhs.y + rhs.y }; }

operator + (lhs: vec2d, rhs: vec2d) -> vec2d
{ return { lhs.x + rhs.x, lhs.y + rhs.y }; }

operator - (lhs: vec2d, rhs: vec2d) -> vec2d
{ return { lhs.x - rhs.x, lhs.y - rhs.y }; }

operator * (lhs: vec2d, rhs) -> vec2d
{ return { lhs.x * rhs, lhs.y * rhs }; }

operator * (lhs, rhs: vec2d) -> vec2d
{ return { lhs * rhs.x, lhs * rhs.y }; }

operator / (lhs: vec2d, rhs) -> vec2d
{ return { lhs.x / rhs, lhs.y / rhs }; }




//////////////////////////////////////////////
// in file vec2d.bz

import math;

struct vec2d
{
	x: float64;
	y: float64;

	constructor(x: float64, y: float64)
		: x(x), y(y)   // not final syntax
	{}

	constructor(t: std::tuple<float64, float64>)
		: x(t[0]), y(t[0])
	{}
}

function length(v : vec2d)
{
	return math::sqrt(v.x * v.x + v.y * v.y);
}

function normalize(v : vec2d&)
{
	let len = v.length();  // valid, because function length takes a vec2d as its first argument
	let ilen = 1 / len;
	v.x *= ilen;
	v.y *= ilen;
}

const v : vec2d = { 1.4, 32.5 };
v.normalize();  // error: v is const, normalize isn't
v.length();  // ok



////////////////////////////////////////////////
// file memory.bz

struct<typename T> unique_ptr
{
private:
	_data: T *;

public:
	constructor()
		: _data(null)
	{}

	constructor(data: T *)
		: _data(data)
	{}

	constructor(: unique_ptr<T> &&) = default;
	constructor(: unique_ptr<T> &)  = delete;

	destructor()
	{
		delete this._data;
	}

	destructor && () = default;

	operator = (this: &, rhs: unique_ptr<T> &&) -> unique_ptr<T> &
	{
		delete this._data;
		this._data = rhs._data;
		return this;
	}

	operator = (this: &, : unique_ptr<T> &) = delete;


	function get(this: const &) -> const T *
	{
		return this._data;
	}

	function get(this: &) -> T *
	{
		return this._data;
	}

	function release(this: &) -> T *
	{
		let ptr = this._data;
		this._data = null;
		return ptr;
	}
}




maybe tuples should use [] instead of {}
let t: [int32, float64];
let t = [1, 10.5];
let v: vec2d = [0.0, 5.2];
vec2d[ 0.0, 1.0 ];    // <- maybe this should be allowed? not sure... in c++ it's a mess...
vec2d([ 0.0, 1.0 ]);  // this is a bit more consistent

let [a, b] = get_pair();
[a, b] = get_pair();



for loops

// by value
for (elem in elems)
{ ... }

// by reference
for (&elem in elems)
{ ... }

// by pointer
for (*elem in elems)
{ ... }

// iterate backwards
for (e in <elems)

// iterate forwards (default)
for (e in >elems)

for (i in 0..10)
for (i in 0..v.size())

what should the type of 0..10 be?

operator overloading shouldn't be allowed on basic types,
so it can't really be a library feature

should the language have a built-in range type?
	-- it could? but that would bring in inconsistencies
should this be a special case for for loops?
	-- probably not

operator .. should be overloadable for basic types?
	-- what about other operators? what makes this special?
	-- sounds like unnecessary feature bloat




lambdas

(x, y) => { return x * y; }
(x, y) => x * y

let get_value = x => x.value;
let val = get_value(token);

get_value expands to something like:

let get_value: struct _get_value
{
	//             ??
	operator () (x: &) -> typeof x.value
	{
		return x.value;
	}
};


file endings as .bz? .boz? .bozon?
.bozon is too long...
.boz sounds funny...



ranges

language or library feature?
if language:
	for (i in 0..10)  is easy, as operator .. returns a range
	if it was library we have the problem illustrated a bit further up
else:
	we have the problem of having to allow operator overloading on built-in types


let v: []int = [0, 2, 4, 2, 5];
v[0..3] could also return a range, so instead of
for (let _i = 0; _i < 3; ++_i) { let i = v[_i]; ... }
we could write
for (i in v[0..3]) { ... }
maybe operator : ??
similar to matlab
v[0:3];
v[0:=3];
inclusive or exclusive?





types:

*type      // pointer
&type      // lvalue reference
&&type     // rvalue reference
...... some kind of universal ref?  &&&type ???  #type ?
[ types ]  // tuple
[expr]type // array


anonymous structs?

function make_person() // auto return type required
{
	// first_name as last_name deduced as str
	return [
		.first_name = "John",
		.last_name  = "Doe"
	];

	// explicit types
	return [
		.first_name: std::string = "John",
		.last_name:  std::string = "Doe"
	];
}

function main()
{
	let person = make_person();
	std::print("{} {}\n".format(person.first_name, person.last_name));

	// can work like a tuple, so person[0] == person.first_name
	let [first, last] = make_person();
	std::print("{} {}\n".format(first, last));

	return 0;
}



syntax:

template<typename T>
using [..]T = std::vector<T>;

template<typename T>
using !*T = std::unique_ptr<T>;

template<typename T>
using ?T = std::optional<T>;

...

some number of tokens before a typename

'[' has to be matched by a ']' and so on...

so

[..][int32, int32] == std::vector<[int32, int32]>;

[..][..]float64 == std::vector<std::vector<float64>>;






*/

#include "core.h"

#include "lexer.h"
#include "first_pass_parser.h"
#include "parser.h"


template<>
struct bz::formatter<ast_typespec_ptr>
{
	static bz::string format(ast_typespec_ptr const &typespec, const char *, const char *)
	{
		if (!typespec)
		{
			return "";
		}

		switch (typespec->kind())
		{
		case ast_typespec::constant:
			return bz::format("const {}", typespec->get<ast_typespec::constant>().base);

		case ast_typespec::pointer:
			return bz::format("*{}", typespec->get<ast_typespec::pointer>().base);

		case ast_typespec::reference:
			return bz::format("&{}", typespec->get<ast_typespec::reference>().base);

		case ast_typespec::name:
			return typespec->get<ast_typespec::name>().name;

		case ast_typespec::function:
		{
			auto &fn = typespec->get<ast_typespec::function>();
			bz::string res = "function(";

			bool put_comma = false;
			for (auto &type : fn.argument_types)
			{
				if (put_comma)
				{
					res += bz::format(", {}", type);
				}
				else
				{
					res += bz::format("{}", type);
					put_comma = true;
				}
			}

			res += bz::format(") -> {}", fn.return_type);

			return res;
		}

		case ast_typespec::none:
			return "<error type>";

		default:
			assert(false);
			return "";
		}
	}
};


template<>
struct bz::formatter<ast_expression_ptr>
{
	static bz::string format(ast_expression_ptr const &expr, const char *, const char *)
	{
		if (!expr)
		{
			return "";
		}

		switch (expr->kind())
		{
		case ast_expression::identifier:
			return expr->get<ast_expression::identifier>().value;

		case ast_expression::literal:
			switch (auto &literal = expr->get<ast_expression::literal>(); literal.kind())
			{
			case ast_expr_literal::integer_number:
				return bz::format("{}", literal.integer_value);

			case ast_expr_literal::floating_point_number:
				return bz::format("{}", literal.floating_point_value);

			case ast_expr_literal::string:
				return "\"(string)\"";

			case ast_expr_literal::character:
				return "'(char)'";

			case ast_expr_literal::bool_true:
				return "true";

			case ast_expr_literal::bool_false:
				return "false";

			case ast_expr_literal::null:
				return "null";

			default:
				assert(false);
				return "";
			}

		case ast_expression::binary_op:
		{
			auto &bin_op = expr->get<ast_expression::binary_op>();
			return bz::format(
				"({} {} {})", bin_op.lhs, get_token_value(bin_op.op), bin_op.rhs
			);
		}

		case ast_expression::unary_op:
		{
			auto &un_op = expr->get<ast_expression::unary_op>();
			return bz::format("({} {})", get_token_value(un_op.op), un_op.expr);
		}

		case ast_expression::function_call_op:
		{
			auto &fn_call = expr->get<ast_expression::function_call_op>();
			auto res = bz::format("{}(", fn_call.called);
			if (fn_call.params.size() > 0)
			{
				bool first = true;
				for (auto &p : fn_call.params)
				{
					if (first)
					{
						res += bz::format("{}", p);
						first = false;
					}
					else
					{
						res += bz::format(", {}", p);
					}
				}
			}
			res += ')';
			return res;
		}
		}
		return "";
	}
};


template<>
struct bz::formatter<ast_statement_ptr>
{
	static bz::string format(ast_statement_ptr const &stmt, const char *spec, const char *spec_end)
	{
		if (!stmt)
		{
			return "";
		}

		uint32_t level = 0;
		while (spec != spec_end)
		{
			level *= 10;
			level += *spec - '0';
			++spec;
		}

		bz::string res = "";

		auto indent = [&]()
		{
			for (uint32_t i = 0; i < level; ++i)
			{
				res += "    ";
			}
		};
		indent();

		switch (stmt->kind())
		{
		case ast_statement::if_statement:
		{
			auto &if_stmt = stmt->get<ast_statement::if_statement>();
			res += bz::format(
				"if ({})\n{:{}}", if_stmt.condition, level, if_stmt.then_block
			);
			if (if_stmt.else_block)
			{
				indent();
				res += bz::format("else\n{:{}}", level, if_stmt.else_block);
			}
			break;
		}

		case ast_statement::while_statement:
		{
			auto &while_stmt = stmt->get<ast_statement::while_statement>();
			res += bz::format(
				"while ({})\n{:{}}", while_stmt.condition, level, while_stmt.while_block
			);
			break;
		}

		case ast_statement::for_statement:
			assert(false);
			break;

		case ast_statement::return_statement:
			res += bz::format("return {};\n", stmt->get<ast_statement::return_statement>().expr);
			break;

		case ast_statement::no_op_statement:
			res += ";\n";
			break;

		case ast_statement::compound_statement:
		{
			auto &comp_stmt = stmt->get<ast_statement::compound_statement>();
			res += "{\n";
			for (auto &s : comp_stmt.statements)
			{
				res += bz::format("{:{}}", level + 1, s);
			}
			indent();
			res += "}\n";
			break;
		}

		case ast_statement::expression_statement:
			res += bz::format("{};\n", stmt->get<ast_statement::expression_statement>().expr);
			break;

		case ast_statement::declaration_statement:
		{
			auto &decl = stmt->get<ast_statement::declaration_statement>();
			switch (decl.kind())
			{
				case ast_stmt_declaration::variable_declaration:
				{
					auto &var_decl = decl.get<ast_stmt_declaration::variable_declaration>();
					res += bz::format(
						"let {}: {}{}{};\n", var_decl.identifier->value, var_decl.typespec,
						var_decl.init_expr ? " = " : "", var_decl.init_expr
					);
					break;
				}
				case ast_stmt_declaration::function_declaration:
				{
					auto &fn_decl = decl.get<ast_stmt_declaration::function_declaration>();
					res += bz::format("function {}(", fn_decl.identifier->value);
					int i = 0;
					for (auto &p : fn_decl.params)
					{
						if (i == 0) res += bz::format("{}: {}", p.id, p.type);
						else        res += bz::format(", {}: {}", p.id, p.type);
						++i;
					}
					res += bz::format(") -> {}\n", fn_decl.return_type);
					indent();
					res += "{\n";

					for (auto &stmt : fn_decl.body.statements)
					{
						res += bz::format("{:{}}", level + 1, stmt);
					}

					indent();
					res += "}\n";
					break;
				}
				case ast_stmt_declaration::operator_declaration:
				{
					auto &op_decl = decl.get<ast_stmt_declaration::operator_declaration>();
					res += bz::format("operator {}(", op_decl.op->value);
					int i = 0;
					for (auto &p : op_decl.params)
					{
						if (i == 0) res += bz::format("{}: {}", p.id, p.type);
						else        res += bz::format(", {}: {}", p.id, p.type);
						++i;
					}
					res += bz::format(") -> {}\n", op_decl.return_type);
					indent();
					res += "{\n";

					for (auto &stmt : op_decl.body.statements)
					{
						res += bz::format("{:{}}", level + 1, stmt);
					}

					indent();
					res += "}\n";
					break;
				}
				case ast_stmt_declaration::struct_declaration:
				default:
					assert(false);
					break;
			}
			break;
		}
		}
		return res;
	}
};

int main(void)
{
	lexer_init();
	src_tokens file("src/test.bz");

	auto stream = file.begin();
	auto end    = file.end();

	auto statements = get_ast_statements(stream, end);
	assert(stream->kind == token::eof);

	for (auto &s : statements)
	{
		s->resolve();
	}

	for (auto &s : statements)
	{
		bz::printf("{}", s);
	}

	return 0;
}
