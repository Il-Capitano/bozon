/*

TODO:
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

// ^^ ties in nicely with the shortened variable declarations (let &const a = b;)

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
	probably this is the best idea...


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

*T         // pointer
&T         // lvalue reference
&&T        // rvalue reference
...... some kind of universal ref?  &&&type ???  #type ?
[ Ts ]     // tuple
[expr]T    // array
...T       // pack parameter? also variable?

let a: ...uint64 = [ 0, 1, 2, 3, 4, 5 ];  // ?

function max(a, b, c: ...)
{
	let result = a > b ? a : b;
	((c > result ? cast<void>(result = c) : cast<void> (0)), ...); // pack expansion
	return result;
}

function min(a, b, ...c)
{
	static_if (sizeof... c == 0)
	{
		return a < b ? a : b;
	}
	else
	{
		return min(min(a, b), c...);
	}
}


anonymous structs?

function make_person() // auto return type required
{
	// first_name as last::name deduced as str
	return [
		.first_name = "John",
		.last::name  = "Doe"
	];

	// explicit types
	return [
		.first_name: std::string = "John",
		.last::name:  std::string = "Doe"
	];
}

function main()
{
	let person = make_person();
	std::print("{} {}\n".format(person.first_name, person.last::name));

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





---- type declarations ----

let a = 0;             implicit typing
let b: float64 = 0;    explicit typing
let c: &float64 = b;   reference type
let d: & = b;          auto typing

a shorter and better looking syntax for a few cases
let &d = b;             shorter version for reference
let *e = &d;            maybe for pointers too?
let const f = "hello"   const
let &const g = f;       ref to const
const h = [0.0, "test"] shorter and easier to read

explicit types should not be allowed here

maybe for  const a = ...;  it should be
const a: float64 = 10;

function foo(&const array, const n)
is the same as
function foo(array: &const, n: const)




---- templates ----

// (1)
function<typename T> add(a: T, b: T) -> T
{ return a + b; }

// (2)
function add<typename T>(a: T, b: T) -> T
{ return a + b; }

// (3)
function add(a: typename T, b: T) -> T
{ return a + b; }

// of these (2) looks the best


struct<typename T> vec2d<T>
{
	x: T;
	y: T;
}

struct vec2d<typename T>
{
	x: T;
	y: T;
}

// prefer the second one here too

specializations are possible like this too


struct vec2d<*typename T>
{
	static_assert(false);
}






// ==== exceptions ====

should there be exceptions even?

throw ...;

try {} catch()

it's much easier if functions pointers must be noexcept/nothrow

e.g.

function foo()
{
	try
	{
		some_throwing_function();
	}
	catch (e) // type can be deduced, because there's no indirection that can throw
	{
		std::println(e.what());
	}
}





// ==== types ====

struct foo
{
	...
}

class ?


templates:

struct foo<typename T>
{ ... }
//     ^ no semicolon

let a: foo<int32>;
let a = foo<int32>();

// specialization
struct foo<const typename T>
{ ... }

let a: foo<const int32>;


struct vec2d
{
	// private, protected and public members??

private:
	x: float64;
	y: float64;

	// maybe different syntax
	.x: float64;
	.y: float64;
	private .z: float64; // access specifier for a specific element

	public constructor(_x, _y)
	[ .x = _x; .y = _y; ]
	{}

	public constructor(t: [float64, float64])
	[
		.x(t[0]);
		.y(t[1]);
	]
	{}

	destructor(&this)
	{
		std::println("vec2d::destructor(&)");
	}

	// move destructor
	destructor(&&this)
	{
		std::println("vec2d::destructor(&&)");
	}

	// should the destructor take this as an explicit argument??
	// if yes, it is inconsistent with the constructor

	function abs(&const this)
	{
		return std::hypot(this.x, this.y);
	}

	function abs_sqr(&const this)
	{
		return this.x * this.x + this.y * this.y;
	}

	function dot_prod(&const this, rhs: const vec2d)
	{
		return this.x * rhs.x + this.y * rhs.y;
	}
}

let v = vec2d(1, 2);
v.abs();
abs(v); // error

function abs_alt(v: vec2d)
{
	return std::hypot(v.x, v.y);
}

abs_alt(v);  // good
v.abs_alt(); // also good

why is this good?

in a templated function the .abs() syntax is more general
e.g.:
function get_abs(value)
{
	return value.abs();
}

let v = vec2d(1, 2);
let z = complex(1, 2); // has a global abs() function

get_abs(v); // uses the member function
get_abs(z); // uses the global function

also helps with namespacing!
e.g.:

import bz::complex;

function foo(z)
{
	...
	let abs_value = z.abs();
	...
}

let z1 = bz::complex(1, 2);
let z2 = std::complex(3, 4);
foo(z1);
foo(z2);

also begin and end functions for arrays
let a: [3]int32 = [ 1, 2, 3 ];
// bulit-in types can't have memeber functions,
// but global functions can use the same syntax, so it's good
let   it  = a.begin();
const end = a.end();

more general code can be written without using helper functions





struct complex
{
	operator float64 (&const this) {}
	cast<float64>(&const this) {}
	cast float64 (&const this) {}
	operator cast float64 (&const this) {}
	operator cast<float64>(&const this) {}
}


*/

#include "core.h"

#include "lexer.h"
#include "first_pass_parser.h"
#include "parser.h"




template<>
struct bz::formatter<ast::expression>
{
	static bz::string format(ast::expression const &expr, const char *, const char *)
	{
		switch (expr.kind())
		{
		case ast::expression::index<ast::expr_identifier>:
			return expr.get<ast::expr_identifier_ptr>()->identifier->value;

		case ast::expression::index<ast::expr_literal>:
			switch (auto &literal = expr.get<ast::expr_literal_ptr>(); literal->value.index())
			{
			case ast::expr_literal::integer_number:
				return bz::format("{}", literal->value.get<uint64_t>());

			case ast::expr_literal::floating_point_number:
				return bz::format("{}", literal->value.get<double>());

			case ast::expr_literal::string:
				return "\"(string)\"";

			case ast::expr_literal::character:
				return "'(char)'";

			case ast::expr_literal::bool_true:
				return "true";

			case ast::expr_literal::bool_false:
				return "false";

			case ast::expr_literal::null:
				return "null";

			default:
				assert(false);
				return "";
			}

		case ast::expression::index<ast::expr_tuple>:
		{
			auto &tuple = expr.get<ast::expr_tuple_ptr>();
			if (tuple->elems.size() == 0)
			{
				return "[]";
			}

			auto result = bz::format("[{}", tuple->elems[0]);
			for (size_t i = 1; i < tuple->elems.size(); ++i)
			{
				result += bz::format(", {}", tuple->elems[i]);
			}
			result += "]";
			return result;
		}

		case ast::expression::index<ast::expr_binary_op>:
		{
			auto &bin_op = expr.get<ast::expr_binary_op_ptr>();
			return bz::format(
				"({} {} {})", bin_op->lhs, bin_op->op->value, bin_op->rhs
			);
		}

		case ast::expression::index<ast::expr_unary_op>:
		{
			auto &un_op = expr.get<ast::expr_unary_op_ptr>();
			return bz::format("({} {})", un_op->op->value, un_op->expr);
		}

		case ast::expression::index<ast::expr_function_call>:
		{
			auto &fn_call = expr.get<ast::expr_function_call_ptr>();
			auto res = bz::format("{}(", fn_call->called);
			if (fn_call->params.size() > 0)
			{
				bool first = true;
				for (auto &p : fn_call->params)
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


void print_statement(ast::statement const &stmt, int indent_level = 0);

void print_declaration(ast::stmt_declaration const &decl, int indent_level = 0)
{
	auto indent = [indent_level](int plus_level = 0)
	{
		for (int i = 0; i < indent_level + plus_level; ++i)
		{
			bz::print("    ");
		}
	};

	switch (decl.kind())
	{
	case ast::stmt_declaration::index<ast::decl_variable>:
	{
		auto &var_decl = decl.get<ast::decl_variable_ptr>();
		indent();
		bz::printf("let {}: {}", var_decl->identifier->value, var_decl->var_type);
		if (var_decl->init_expr.has_value())
		{
			bz::printf(" = {};\n", var_decl->init_expr.get());
		}
		else
		{
			bz::print(";\n");
		}
		return;
	}

	case ast::stmt_declaration::index<ast::decl_function>:
	{
		auto &fn_decl = decl.get<ast::decl_function_ptr>();
		indent();
		bz::printf("function {}(", fn_decl->identifier->value);
		bool put_comma = false;
		for (auto &p : fn_decl->params)
		{
			if (put_comma)
			{
				bz::print(", ");
			}
			else
			{
				put_comma = true;
			}
			bz::printf("{}: {}", &*p.id ? p.id->value : "", p.var_type);
		}
		bz::printf(") -> {}\n", fn_decl->return_type);

		indent();
		bz::print("{\n");
		for (auto &s : fn_decl->body.statements)
		{
			print_statement(s, indent_level + 1);
		}
		indent();
		bz::print("}\n");
		return;
	}

	case ast::stmt_declaration::index<ast::decl_operator>:
	{
		auto &op_decl = decl.get<ast::decl_operator_ptr>();
		indent();
		if (op_decl->op->kind == token::paren_open)
		{
			bz::print("operator () (");
		}
		else if (op_decl->op->kind == token::square_open)
		{
			bz::print("operator [] (");
		}
		else
		{
			bz::printf("operator {} (", op_decl->op->value);
		}
		bool put_comma = false;
		for (auto &p : op_decl->params)
		{
			if (put_comma)
			{
				bz::print(", ");
			}
			else
			{
				put_comma = true;
			}
			bz::printf("{}: {}", &*p.id ? p.id->value : "", p.var_type);
		}
		bz::printf(") -> {}\n", op_decl->return_type);

		indent();
		bz::print("{\n");
		for (auto &s : op_decl->body.statements)
		{
			print_statement(s, indent_level + 1);
		}
		indent();
		bz::print("}\n");
		return;
	}

	case ast::stmt_declaration::index<ast::decl_struct>:
	{
		auto &struct_decl = decl.get<ast::decl_struct_ptr>();
		indent();
		bz::printf("struct {}\n", struct_decl->identifier->value);
		indent();
		bz::print("{\n");
		for (auto &var : struct_decl->member_variables)
		{
			indent(1);
			bz::printf("{}: {};\n", var.id->value, var.var_type);
		}
		indent();
		bz::print("}\n");
		return;
	}

	default:
		assert(false);
		return;
	}
}

void print_statement(ast::statement const &stmt, int indent_level)
{
	auto indent = [indent_level]()
	{
		for (int i = 0; i < indent_level; ++i)
		{
			bz::print("    ");
		}
	};

	switch (stmt.kind())
	{
	case ast::statement::index<ast::stmt_if>:
	{
		auto &if_stmt = stmt.get<ast::stmt_if_ptr>();
		indent();
		bz::printf("if ({})\n", if_stmt->condition);
		print_statement(if_stmt->then_block, indent_level);
		if (if_stmt->else_block.has_value())
		{
			indent();
			bz::printf("else\n");
			print_statement(if_stmt->else_block.get());
		}
		return;
	}

	case ast::statement::index<ast::stmt_while>:
	{
		auto &while_stmt = stmt.get<ast::stmt_while_ptr>();
		indent();
		bz::printf("while ({})\n", while_stmt->condition);
		print_statement(while_stmt->while_block, indent_level);
		return;
	}

	case ast::statement::index<ast::stmt_for>:
		assert(false);
		return;

	case ast::statement::index<ast::stmt_return>:
		indent();
		bz::printf("return {};\n", stmt.get<ast::stmt_return_ptr>()->expr);
		return;

	case ast::statement::index<ast::stmt_no_op>:
		indent();
		bz::print(";\n");
		return;

	case ast::statement::index<ast::stmt_compound>:
	{
		auto &cmp_stmt = stmt.get<ast::stmt_compound_ptr>();
		indent();
		bz::print("{\n");
		for (auto &s : cmp_stmt->statements)
		{
			print_statement(s, indent_level + 1);
		}
		indent();
		bz::print("}\n");
		return;
	}

	case ast::statement::index<ast::stmt_expression>:
		indent();
		bz::printf("{};\n", stmt.get<ast::stmt_expression_ptr>()->expr);
		return;

	case ast::statement::index<ast::stmt_declaration>:
		print_declaration(*stmt.get<ast::stmt_declaration_ptr>(), indent_level);
		return;

	default:
		assert(false);
		return;
	}
}

int main(void)
{
	assert(ast::size_of(ast::str_) == 16);
	assert(ast::align_of(ast::str_) == 8);

	assert(
		ast::make_ts_tuple(bz::vector<ast::typespec>{ ast::make_ts_base_type(ast::int32_) })
		== ast::make_ts_tuple(bz::vector<ast::typespec>{ ast::make_ts_base_type(ast::int32_) })
	);

	auto start = std::chrono::high_resolution_clock::now();

	src_file file("src/test.bz");

	auto after_tokenizing = std::chrono::high_resolution_clock::now();

	auto stream = file.tokens_begin();
	auto end    = file.tokens_end();

	auto statements = get_ast_statements(stream, end);
	assert(stream->kind == token::eof);

	auto after_first_pass = std::chrono::high_resolution_clock::now();

	for (auto &s : statements)
	{
		s.resolve();
	}

	auto after_resolving = std::chrono::high_resolution_clock::now();

	for (auto &s : statements)
	{
		print_statement(s);
	}

	bz::print("\n\n");
	bz::printf(
		"Tokenization:  {:>7.3f}ms\n"
		"First pass:    {:>7.3f}ms\n"
		"Resolving:     {:>7.3f}ms\n"
		"Whole parsing: {:>7.3f}ms\n"
		"Whole runtime: {:>7.3f}ms\n",
		(after_tokenizing - start).count() / 1'000'000.0,
		(after_first_pass - after_tokenizing).count() / 1'000'000.0,
		(after_resolving - after_first_pass).count() / 1'000'000.0,
		(after_resolving - after_tokenizing).count() / 1'000'000.0,
		(after_resolving - start).count() / 1'000'000.0
	);

	return 0;
}
