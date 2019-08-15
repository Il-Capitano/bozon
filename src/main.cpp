/*

TODO:
	- check identifiers while parsing
		kind of done, template syntax stays the same, but expressions
		need to be parsed seperately, once all glabal symbols are known

	- expression type evaluation for auto types
		kind of also done, need to clean up the function call and operator type checks
	- array type
	- make a char type (8 bit? unicode?)
		leaning towards unicode, as an 8-bit representation could be
		just a uint8, and a char could be a string storing a utf8 character

		possibly have a seperate type for an ascii character?
		 -- probably not...

		a standard string type should be utf8, and we could have a seperate
		ascii_string

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



// templates:

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
// in file vec2d.asdf

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
// file memory.asdf

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




a * vector<int>() - 3;  // not ambiguious
a * vector<int>(3) - 3; // ambiguious

((a * vector) < int) > (3 - 3)

(a * vector<int>(3)) - 3

'a' '*' 'vector' '<' 'int' '>' '(' '3' ')' '-' '3'

';'

*/


#include "core.h"

#include "lexer.h"
#include "first_pass_parser.h"
#include "parser.h"


#include "variant.h"

void print_typespec(ast_typespec_ptr const &typespec)
{
	if (!typespec)
	{
		return;
	}

	switch (typespec->kind)
	{
	case type::constant:
		std::cout << "const ";
		print_typespec(typespec->base);
		break;

	case type::pointer:
		std::cout << '*';
		print_typespec(typespec->base);
		break;

	case type::reference:
		std::cout << '&';
		print_typespec(typespec->base);
		break;

	case type::name:
		std::cout << typespec->name;
		break;
	}
}

void print_expr(ast_expression_ptr const &expr)
{
	if (!expr)
	{
		return;
	}

	switch (expr->kind)
	{
	case expression::identifier:
		std::cout << expr->identifier->value;
		break;

	case expression::literal:
		switch (expr->literal->kind)
		{
		case literal::integer_number:
			std::cout << expr->literal->integer_value;
			break;

		case literal::floating_point_number:
			std::cout << expr->literal->floating_point_value;
			break;

		case literal::string:
			std::cout << "\"(string)\"";
			break;

		case literal::character:
			std::cout << "'(char)'";
			break;

		case literal::bool_true:
			std::cout << "true";
			break;

		case literal::bool_false:
			std::cout << "false";
			break;

		case literal::null:
			std::cout << "null";
			break;

		default:
			assert(false);
			break;
		}
		break;

	case expression::binary_op:
		std::cout << '(';
		print_expr(expr->binary_op->lhs);
		std::cout << ' ' << get_token_value(expr->binary_op->op) << ' ';
		print_expr(expr->binary_op->rhs);
		std::cout << ')';
		break;

	case expression::unary_op:
		std::cout << '(';
		std::cout << get_token_value(expr->unary_op->op) << ' ';
		print_expr(expr->unary_op->expr);
		std::cout << ')';
		break;

	case expression::function_call_op:
		print_expr(expr->function_call_op->called);
		std::cout << '(';
		if (expr->function_call_op->params.size() > 0)
		{
			for (auto &p : expr->function_call_op->params)
			{
				print_expr(p);
				std::cout << ", ";
			}
			std::cout << "\b\b)";
		}
		else
		{
			std::cout << ')';
		}
		break;
	}
}

void print_stmt(ast_statement_ptr const &stmt, int level = 0)
{
	if (!stmt)
	{
		return;
	}

	auto indent = [&]()
	{
		for (int i = 0; i < level; ++i)
		{
			std::cout << "    ";
		}
	};
	indent();

	switch (stmt->kind)
	{
	case statement::if_statement:
		std::cout << "if (";
		print_expr(stmt->if_statement->condition);
		std::cout << ")\n";
		print_stmt(stmt->if_statement->if_block, level);
		if (stmt->if_statement->else_block)
		{
			indent();
			std::cout << "else\n";
			print_stmt(stmt->if_statement->else_block, level);
		}
		break;

	case statement::while_statement:
		std::cout << "while (";
		print_expr(stmt->while_statement->condition);
		std::cout << ")\n";
		print_stmt(stmt->while_statement->while_block, level);
		break;

	case statement::for_statement:
		std::cerr << "for statment not yet implemented\n";
		exit(1);

	case statement::return_statement:
		std::cout << "return ";
		print_expr(stmt->return_statement->expr);
		std::cout << ";\n";
		break;

	case statement::no_op_statement:
		std::cout << ";\n";
		break;

	case statement::variable_decl_statement:
		std::cout << "let " << stmt->variable_decl_statement->identifier;
		if (stmt->variable_decl_statement->typespec)
		{
			std::cout << ": ";
			print_typespec(stmt->variable_decl_statement->typespec);
		}
		if (stmt->variable_decl_statement->init_expr)
		{
			std::cout << " = ";
			print_expr(stmt->variable_decl_statement->init_expr);
		}
		std::cout << ";\n";
		break;

	case statement::compound_statement:
		std::cout << "{\n";
		for (auto &s : stmt->compound_statement->statements)
		{
			print_stmt(s, level + 1);
		}
		indent();
		std::cout << "}\n";
		break;

	case statement::expression_statement:
		print_expr(stmt->expression_statement->expr);
		std::cout << ";\n";
		break;

	case statement::struct_definition:
		std::cout << "(struct definition)\n";
		break;

	case statement::function_definition:
		std::cout << "function " << stmt->function_definition->identifier << '(';
		{
		int i = 0;
		for (auto &p : stmt->function_definition->params)
		{
			if (i != 0)
			{
				std::cout << ", ";
			}
			std::cout << p->identifier << ": ";
			print_typespec(p->typespec);
			++i;
		}
		}
		std::cout << ')';
		if (stmt->function_definition->return_typespec)
		{
			std::cout << " -> ";
			print_typespec(stmt->function_definition->return_typespec);
			std::cout << '\n';
		}
		std::cout << "{\n";
		for (auto &s : stmt->function_definition->body->statements)
		{
			print_stmt(s, level + 1);
		}
		indent();
		std::cout << "}\n";
		break;

	case statement::operator_definition:
		std::cout << "function " << get_token_value(stmt->operator_definition->op) << '(';
		{
		int i = 0;
		for (auto &p : stmt->operator_definition->params)
		{
			if (i != 0)
			{
				std::cout << ", ";
			}
			std::cout << p->identifier << ": ";
			print_typespec(p->typespec);
			++i;
		}
		}
		std::cout << ')';
		if (stmt->operator_definition->return_typespec)
		{
			std::cout << " -> ";
			print_typespec(stmt->operator_definition->return_typespec);
			std::cout << '\n';
		}
		std::cout << "{\n";
		for (auto &s : stmt->operator_definition->body->statements)
		{
			print_stmt(s, level + 1);
		}
		indent();
		std::cout << "}\n";
		break;

	default:
		std::cerr << "Error in print_stmt()\n";
		exit(1);
	}
}



int main(void)
{
	auto v = variant<int, double>::make<int>(3);

	auto v_int = v.get_if<int>();
	auto v_double = v.get_if<double>();

	assert(v_int != nullptr);
	assert(v_double == nullptr);

	std::cout << *v_int << '\n';

	auto v2 = v.make<double>(2.5);

	auto v2_int = v2.get_if<int>();
	auto v2_double = v2.get_if<double>();

	assert(v2_int == nullptr);
	assert(v2_double != nullptr);

	std::cout << *v2_double << '\n';

	auto v3 = v2;
	auto v4 = std::move(v);

	std::cout << "v3: " << v3 << '\n';
	std::cout << "v4: " << v4 << '\n';

	std::cout << "success\n";

	return 0;


	lexer_init();
	token_stream stream("src/test.txt");
	auto fp_statements = get_fp_statements(stream);

	std::vector<ast_statement_ptr> statements;
	for (auto &s : fp_statements)
	{
		statements.emplace_back(make_ast_statement(s));
	}

	for (auto &s : statements)
	{
		print_stmt(s);
	}

	return 0;
}
