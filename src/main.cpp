/*


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

set a flag in a register?   <--- (this is what I went with)
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
	// first_name and last_name deduced as str
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

	operator explicit cast<float64>(&const this) {}
}



Let's decide on how struct should be defined!!!

struct vec3
{
	.x: float64;
	.y: float64;
	.z: float64;
	// --------
	// I like this one better
	x: float64;
	y: float64;
	z: float64;


	constructor(_x, _y, _z)
	[ .x = _x, .y = _y, .z = _z]
	{}

	constructor(t: [float64, float64, float64])
	[ .x = t[0], .y = t[1], .z = t[2]]
	{}

	explicit constructor(v2: vec2)
	[ .x = v2.x, .y = v2.y, .z = 0 ]
	{}

	destructor() = default;

	function abs(&const this) -> float64
	{
		return std::hypot(this.x, this.y, this.z);
	}

	function abs_sqr(&const this) -> float64
	{
		return this.x * this.x + this.y * this.y + this.z * this.z;
	}

	function dot_prod(&const this, v: &const vec3) -> float64
	{
		return this.x * v.x + this.y * v.y + this.z * v.z;
	}

	cast<vec4>(&const this)
	{
		return [ this.x, this.y, this.z, 0 ];
	}

	explicit cast<vec2>(&const this)
	{
		return [ this.x, this.y ];
	}
}

// # is universal reference
function call_func(func, ...#args)
{
	return func(...std::forward<typeof args>(args));
}



struct vec<N: uint64, T: typename>
{
	// ...
}

using vec2<T: typename> = vec<2, T>;

function make_vec(tup: [...T: typename]) { ... }
function make_vec(tup: [...typename T]) { ... }
function make_vec(tup: [...typename T]) { ... }
function make_vec(tup: [...$T]) { ... }

function make_vec(tup: [N: uint64]T: typename) {}
function make_vec(tup: [$N: uint64]$T: typename) {}

function<N: uint64, T: typename> make_vec(tup: [N]T) {}
function make_vec<N: uint64, T: typename>(tup: [N]T) {}   <- this should be reserved for explicit templates

make_vec([0, 1, 2, 3]);
make_vec<4, int32>([0, 1, 2, 3])






 -- array syntax??

[10]int32    this feels familiar
[10: int32]  this should be easier to parse

[10][float64, float64]
[10: [float64, float64]]

[: float64]  size is deduced here
[10:]        type is deduced here
[:]          size and type are deduced here


in templated functions, types shouldn't be named I think.

e.g. instead of
function make_vec(tup: [$N: $T]) { ... }
we should have
function make_vec(tup: [:]) {
	const N = tup.size();
	using T = std::element_type<typeof tup>;
	using T = typeof tup[0];
}



template syntax should stay as <> and not ()
parsing would be easier and possibly more consistent if we did it the functional way,
but then we would lose the ability to have template deduction for types

e.g.
vec2<float64>(1.0, -3.0)  ->  vec2(1.0, -3.0)
with ()'s we couldn't do that
vec2(float64)(1.0, -3.0)  ->  ??







// only member variables in struct definition
struct vec2
{
	x: float64,
	y: float64,
}

// allow forward declarations for external libraries
struct FILE;

// member functions, constructors and destructors are seperate
// this allows to add member functions to incomplete types
implement vec2
{
	constructor() = default;
	destructor(&const this) = default;

	// should constructors return an instance??
	constructor(x, y)
	{
		// syntax ??
		return vec2([ .x = x, .y = y ]);
	}

	constructor(arr: #const [2: auto])
	{
		return vec2[ .x = arr[0], .y = arr[1] ];
	}

	constructor(tup: [#const auto, #const auto])
	{
		return vec2[ .x = tup[0], .y = tup[1] ];
	}

	function norm(#const this)
	{
		return std::hypot(this.x, this.y);
	}

	function abs(#const this)
	{
		return this.norm();
	}
}


implement FILE
{
	function close(#this)
	{
		std::libc::fclose(&this);
	}
}


// ...
let f = std::libc::fopen("data.txt");

process_data(f);

f->close();
f = null;







==== metafunctions ====

// in "std/meta/ast.bz"
export using expression = __builtin_expression_t;
export using expression_src_pos = __builtin_expression_src_pos_t;

export for expression
consteval function get_src_pos(expr: &const expression) -> &const expression_src_pos
{
	return __builtin_expression_get_src_pos(expr);
}

export for expression_src_pos
consteval function get_file_name(src_pos: &const expression_src_pos) -> str
{
	return __builtin_expression_src_pos_get_file_name(src_pos);
}

export for expression_src_pos
consteval function get_line_number(src_pos: &const expression_src_pos) -> usize
{
	return __builtin_expression_src_pos_get_line_number(src_pos);
}

export metafunction create_expression(expr: &const expression)
{
	-> (__builtin_create_expression(expr));
}

export consteval function is_debug() -> bool
{
	return __builtin_is_debug();
}


// in "assert.bz"
import std::meta::expression;

export metafunction assert(expr: &const std::meta::expression)
requires std::is_implicitly_convertible<expr.get_type(), bool>
{
	// use `->` to insert instructions
	-> {
		const predicate = std::meta::create_expression(expr) as bool;
		if (!predicate)
		{
			if consteval (std::meta::is_debug())
			{
				std::print(
					"assertion failed at {}:{}\n"
					"    expression: {}\n".format(
						std::meta::create_constant(expr.get_src_pos().get_file_name()),
						std::meta::create_constant(expr.get_src_pos().get_line_number()),
						std::meta::expression_as_string(expr)
					)
				);
				std::abort();
			}
			else
			{
				unreachable;
			}
		}
	};
}

export for std::result metafunction unwrap(expr: std::meta::expression)
requires std::is_result_type<std::remove_reference_const<expr.get_type()>>
{
	-> {
		const value = std::meta::create_expression(expr);
		if (value.has_error())
		{
			return value.get_error();
		}

		value.get_result()
	};
}



maybe metafunctions should return an expression or statement or declaration:

metafunction unwrap(expr: std::meta::expression) -> std::meta::expression
requires std::is_result_type<std::remove_reference_const<expr.get_type()>>
{
	return {
		const value = expr;
		if (value.has_error())
		{
			return value.get_error();
		}
		value.get_result()
	};
}

metafunction assert(expr: std::meta::expression) -> std::meta::statement
requries std::remove_reference_const<expr.get_type()> == bool
{
	return {
		const expr_result = expr;
		if (!expr_result)
		{
			std::print(
				std::stderr,
				"assertion failed at {expr.get_file_name()}:{expr.get_line_number()}\n"
				"    expression: {expr.as_string()}\n"f
			);
		}
	} // no semicolon?

	return const std::meta::make_identifier("tmp{expr.get_line_number()}"f) = tmp;
}



==== more general expressions ====

// instead of ternary
const a = if (condition) value1 else value2;
const a = { std::print("hello"); 3 }
//                                ^ missing semicolon means that is the evaluated value
// similar to const a = (std::print("hello"), 3);

const value = switch (c) {
	'0' => 0,
	'1' => 1,
	'2' => 2,
	// ...
	'a', 'A' => {
		std::print("hex number!\n");
		10
	},
};

this would be similar to immediately invoked lambdas in c++

auto const value = [&]() {
	switch (c)
	{
		// ...
	}
}();



==== array type syntax ====

[10]int32 is ambiguous
[10: int32] I like this, but we can't have operator : with this syntax
[10 of int32] maybe?   [10, 20 of float64]
^^ this needs another keyword, which may not be for the best

[10 :: int32] maybe? a bit ambiguous with ::int32
[10, 20 :: float64]

also would be nice, if there was an array slice type
[: int32]
function print_arr(arr: [: const int32])
{
	for (const elem in arr)
	{
		print(elem);
		print(", ");
	}
	print('\n');
}

instead of
function print_arr(arr: std::slice<const int32>)
{
	for (const elem in arr)
	{
		print(elem);
		print(", ");
	}
	print('\n');
}

maybe this is just useless feature bloat






==== template syntax ====

struct vec<N: uint64, T: typename>
{
	c: [N: T],
}

specialization:
this wouldn't allow operator : to be a thing,
as N: uint64 would be ambiguous
maybe specializations should have parenthesis around the template parameter

struct vec<2 as uint64, T: typename> { ... }
struct vec<(2 as uint64), T: typename> { ... }

struct vec<N: uint64, T: &typename> = delete;

struct some_type<T: std::vector<typename>>
struct some_type<T: std::vector> // should this be allowed???


new idea for specialization:

struct some_type<T: typename>  { ... }
struct some_type<&(T: typename)> { ... }
struct some_type<std::vector<T: typename>> { ... }

struct some_type<std::static_vector<T: typename, N: usize>> { ... }
//                                  ^^^^^^^^^^^^^^^^^^^^^ multiple captures








==== if consteval ====

if consteval (condition)
{
	function foo() { ... }
}
else
{
	function foo(n: int32) { ... }
}

const val = if consteval (condition) a else b;






==== parsing ====

implementing compound expression with the current system is a bit of a hassle
I would need to either change function declarations to have just a token range for the body
or have to maintain an expr_compound and a stmt_compound type seperately

changing function_body to have a variant of bz::vector<statement> and lex::token_range could work
this has negative performance implication though, as we would have to reparse templated functions
maybe that's not too bad...

having a more uniform system of expr_if, expr_compound, ... should make life easier and the language
more consistent



{
	std::print("hello, {}\n".std::format("world!"));
}



==== slices ====

slice type: [: float64]
multi-dimension slices??  [,: float64]
[, 10: int32] == [: [10: int32]]
[10,: int32] == [10: [: int32]]  maybe not allowed because it's a bit confusing

let arr: [4: int32] = [ ... ];
const slice = arr[:];  // arr[0] to arr[4] (one past the end)
const slice = arr[1:]; // arr[1] to arr[4]
const slice = arr[:2]; // arr[0] to arr[2]
const slice = arr[:5]; // UB? checked in debug mode? maybe?

[: T] => { T*, T* } in LLVM IR

operator[](v: &vector<T: typename>, s: __slice_index_t) -> [: T]
{
	return __builtin_slice_from_pointers(v.data + s.begin, v.data + s.end);
}

operator[](v: &const vector<T: typename>, s: __slice_index_t) -> [: const T]
{
	return __builtin_slice_from_pointers(v.data + s.begin, v.data + s.end);
}




Maybe an interactive shell could be done with this language, kind of like python.
Would be JIT compiled, would allow IO in comptime code (?)

consteval f = read_file("file_name.txt");
// use f



struct vec2d
{
	.x: float64;
	.y: float64;
}

struct string
{
	._data_begin: *uint8;
	._data_end:   *uint8;
	._alloc_end:  *uint8;

	constructor(s: str)
	{
		const size = __builtin_str_size(s);
		const [alloc_begin, alloc_end] = allocate_memory(size);
		memcpy(alloc_begin, __builtin_str_begin(s), size));
		return string[
			._data_begin = alloc_begin,
			._data_end   = alloc_begin + size,
			._alloc_end  = alloc_end
		];
	}

	constructor(other: &const string)
	{
		const size = other.size();
		const [alloc_begin, alloc_end] = allocate_memory(size);
		memcpy(alloc_begin, other._data_begin, size));
		return string[
			._data_begin = alloc_begin,
			._data_end   = alloc_begin + size,
			._alloc_end  = alloc_end
		];
	}

	constructor(other: move string) = default;

	destructor(&this)
	{
		deallocate(this._data_begin, this._data_end);
	}

	destructor(move this) = default;
}

export function size(s: #const string) -> usize
{
	return __builtin_str_size(s);
}

export function length(s: #const string) -> usize
{
	return __builtin_str_length(s);
}

export operator as str (s: #const string) -> str
{
	return __builtin_str_from_ptrs(s._data_begin, s._data_end);
}



export struct vector<T: typename>
{
	// ...
}

export function size(v: #const vector) -> usize  // implicit generic function
{
	return (v._data_end - v._data_begin) as usize;
}

// ...

import std::vector;

function main()
{
	let v = std::vector::<int32>(); // explicit templating?  similar to `template std::vector<int>` in C++
	let v = std::vector<int32>();
}

// example of when ::<...> is needed

function foo(T: typename)
{
	let val = T.inner_template_type::<int32>();
	// instead of
	let val = T.inner_template_type<int32>();
}

struct foo<T> {} // implicitly typename argument
struct foo<...Ts> {} // variadic

*/

#include "ctx/global_context.h"
#include "crash_handling.h"
#include "timer.h"

#if defined(NDEBUG) || defined(_WIN32)
// use std::exit instead of returning to avoid LLVM corrupting the heap
// this happens rarely in debug mode, but is common for release builds
#define return_from_main(val) std::exit(return_zero_on_error ? 0 : (val))
#else
#define return_from_main(val) return (return_zero_on_error ? 0 : (val))
#endif // NDEBUG

int main(int argc, char const **argv)
{
#ifdef __has_feature
#if !__has_feature(address_sanitizer)
	register_crash_handlers();
#endif // !__has_feature(address_sanitizer)
#else // __has_feature
	register_crash_handlers();
#endif // __has_feature

	auto in_ms = [](auto time)
	{
		return static_cast<double>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(time).count()
		) * 1e-6;
	};

	auto t = timer();

	{
		ctx::global_context global_ctx;

		t.start_section("command line parse time");
		if (!global_ctx.parse_command_line(argc, argv))
		{
			global_ctx.report_and_clear_errors_and_warnings();
			return_from_main(1);
		}
		t.end_section();

		if (compile_until <= compilation_phase::parse_command_line)
		{
			global_ctx.report_and_clear_errors_and_warnings();
			return_from_main(0);
		}

		t.start_section("initialization time");
		if (!global_ctx.initialize_llvm())
		{
			global_ctx.report_and_clear_errors_and_warnings();
			return_from_main(2);
		}
		if (!global_ctx.initialize_builtins())
		{
			global_ctx.report_and_clear_errors_and_warnings();
			return_from_main(2);
		}
		t.end_section();

		t.start_section("global symbol parse time");
		if (!global_ctx.parse_global_symbols())
		{
			global_ctx.report_and_clear_errors_and_warnings();
			return_from_main(3);
		}
		t.end_section();

		if (compile_until <= compilation_phase::parse_global_symbols)
		{
			global_ctx.report_and_clear_errors_and_warnings();
			return_from_main(0);
		}

		t.start_section("parse time");
		if (!global_ctx.parse())
		{
			global_ctx.report_and_clear_errors_and_warnings();
			return_from_main(4);
		}
		t.end_section();

		if (compile_until <= compilation_phase::parse)
		{
			global_ctx.report_and_clear_errors_and_warnings();
			return_from_main(0);
		}

		t.start_section("bitcode emission time");
		if (!global_ctx.emit_bitcode())
		{
			global_ctx.report_and_clear_errors_and_warnings();
			return_from_main(5);
		}
		t.end_section();

		// report all errors and warnings now, so we don't have to wait for optimization
		// and file emission
		global_ctx.report_and_clear_errors_and_warnings();

		t.start_section("optimization time");
		if (!global_ctx.optimize())
		{
			global_ctx.report_and_clear_errors_and_warnings();
			return_from_main(6);
		}
		t.end_section();

		t.start_section("file emission time");
		if (!global_ctx.emit_file())
		{
			global_ctx.report_and_clear_errors_and_warnings();
			return_from_main(7);
		}
		t.end_section();

		if (compile_until <= compilation_phase::emit_bitcode)
		{
			global_ctx.report_and_clear_errors_and_warnings();
			return_from_main(0);
		}
	}

	if (do_profile)
	{
		auto const compilation_time = t.get_total_duration();
		auto const front_end_time   =
			t.get_section_duration("command line parse time")
			+ t.get_section_duration("initialization time")
			+ t.get_section_duration("global symbol parse time")
			+ t.get_section_duration("parse time")
			+ t.get_section_duration("bitcode emission time");
		auto const llvm_time        =
			t.get_section_duration("optimization time")
			+ t.get_section_duration("file emission time");

		bz::print("successful compilation in {:8.3f}ms\n", in_ms(compilation_time));
		bz::print("front-end time:           {:8.3f}ms\n", in_ms(front_end_time));
		bz::print("LLVM time:                {:8.3f}ms\n", in_ms(llvm_time));
		for (auto const &[name, begin, end] : t.timing_sections)
		{
			bz::print("{:25} {:8.3f}ms\n", bz::format("{}:", name), in_ms(end - begin));
		}

#ifdef BOZON_PROFILE_ALLOCATIONS
		bz::print("allocations:              {:8}\n", ast::arena_allocator::get_allocation_count());
		bz::print("deallocations:            {:8}\n", ast::arena_allocator::get_deallocation_count());
		bz::print("total allocation size:    {:8}\n", ast::arena_allocator::get_total_allocation_size());
#endif // BOZON_PROFILE_ALLOCATIONS
	}

	return_from_main(0);
}
