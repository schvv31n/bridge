- ~Syscalls~
- ~Functions~
- ~Base type system~
- ~Basic arithmetic~
- ~Bitwise operations~
- ~Type casting~
- ~`bool` type and logical operations~
- ~Bracketed exressions~
- ~Conditions~
- ~Loops~
- ~Referencing and dereferencing~
- ~Complex assignment expressions (`+=`, `&=`, etc.)~
- ~Array variables~
- ~Pointer arithmetic~
- ~BRP: includes~
- ~BRP: macros~
- ~BRP: multiline macros~
- ~BRP: conditional compilation~
- ~BRP: character literals~
- ~BRP: function-like macros~
- ~Array initializers~
- ~Bytecode optimization: level 1 (failed)~
- ~`static` variables~
- Rewrite Bridge Virtual Machine from a register-based to a stack-based one
- Rename standard integer types:
```
	int8 -> s8
	int16 -> s16
	int32 -> s32
	int64 -> s64
```
	also add pointer-sized integer, `sptr`, just like C's `intptr_t`
- Unsigned integer types: u8, u16, u32, uptr, u64
- Floating-point numbers: f32, f64, fptr

- Classes
	Example: ```
		type Bytes {
			s8& data;
			sptr length;
			public i64 size() -> this.length;
			public:
				Bytes concat(Bytes other): ...
		}
	```

- Operator overloads
	Example: ```
		Rstream& RStream.builtin << (str s): ...
	```

- Implicit return variable, like `result` keyword in Nim

- BRP: enhanced integer literals:
	Specifying number of kilobytes:
		malloc(5KB); // same as malloc(5 * 1024);
	Specifying number of megabytes:
		malloc(5MB); // same as malloc(5 * 1024 * 1024);
	Specifying number of gigabytes:
		malloc(5GB); // same as malloc(5 * 1024 * 1024 * 1024);

- Callable types
	In custom types, it can be added as an operator `()` overload
	Syntax for function types is the following:
		String <- (s64&) f; // defines `f` as a reference to a function that accepts a reference to a 64-bit integer and returns a `String` object

- Exceptions
	Theoretically, any type can be thrown as an exception 
	```
	s64 div(s64 x, s64 y):
		if (y == 0) throw MathError() else return x / y;

	```
- Bytecode optimizations
- x86-64 support
- Traits and some kinda metaprogramming
