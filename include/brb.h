#ifndef _BRB_
#define _BRB_

#include <br.h>
#define ARENA_ASSERT(expr) assert(expr, #expr)
#include <external/arena.h>
#include <brp.h>

typedef enum {
/* 
Data Types:
 	i8 - a byte
	i16 - a 2-byte value
	i32 - a 4-byte value
	ptr - a pointer-sized value
	i64 - an 8-byte value
	int - any of the above

Notes:
*/
	BRB_OP_NOP,      // [] -> nop -> []
	// does nothing
	BRB_OP_END,      // [] -> end -> []
	// stops execution of the program
	BRB_OP_I8,       // [] -> i8 <x> -> [i8]
	// pushes 1 byte-sized integer literal <x> onto the stack
	BRB_OP_I16,      // [] -> i16 <x> -> [i16]
	// pushes 2 byte-sized integer literal <x> onto the stack
	BRB_OP_I32,      // [] -> i32 <x> -> [i32]
	// pushes 4 byte-sized integer literal <x> onto the stack
	BRB_OP_I64,      // [] -> i64 <x> -> [i64]
	// pushes 8 byte-sized integer literal <x> onto the stack
	BRB_OP_PTR,     // [] -> ptr <x> -> [ptr]
	// pushes pointer-sized integer literal <x> onto the stack
	BRB_OP_ADDR,     // [A, *] -> addr <i> -> [A, *, ptr]
	// pushes address of the stack item at index <i> onto the stack; <i> refers to the index on the stack after pushing the address
	BRB_OP_DBADDR,   // [] -> dbaddr <i> -> [ptr]
	// pushes address of data block at index <i> onto the stack
	BRB_OP_LD,       // [A:ptr] -> ld <T> -> [<T>]
	// replace address A with item of type <T> loaded from it
	BRB_OP_STR,      // [A:ptr, B] -> str -> []
	// store B at the address A; same as *A = B;
	BRB_OP_SYS,
	// executes system procedure <f> with arguments from the stack
	/* System calls:
		[A:ptr] -> sys exit // exits the program with exit code A
		[A:ptr, B:ptr, C:ptr] -> sys write -> [ptr] // write C bytes from address B to file descriptor A
		[A:ptr, B:ptr, C:ptr] -> sys read -> [ptr] // read C bytes from file descriptor A to address B
	*/
	BRB_OP_BUILTIN,  // [] -> builtin <id> -> ptr
	// places a pointer-sized built-in constant on top of the stack
/* TODO
	BRB_OP_ADD,      // [A:int, B:int] -> add -> [int]
	// replace A and B with their sum
	BRB_OP_ADDI, 	 // [A:int] -> add-i <n> -> [int]
	// increment A by <n>
	BRB_OP_ADDI@8,   // [A:ptr] -> add-i@8 <n> -> [A:ptr]
	// increment an `i8` at address A by <n>
	BRB_OP_ADDI@16,  // [A:ptr] -> add-i@16 <n> -> [A:ptr]
	// increment an `i16` at address A by <n>
	BRB_OP_ADDI@32,  // [A:ptr] -> add-i@32 <n> -> [A:ptr]
	// increment an `i32` at address A by <n>
	BRB_OP_ADDI@P,   // [A:ptr] -> add-i@p <n>  -> [A:ptr]
	// increment a `ptr` at address A by <n>
	BRB_OP_ADDI@64,  // [A:ptr] -> add-i@64 <n> -> [A:ptr]
	// increment an `i64` at address A by <n>

	BRB_OP_SUB,      // [A:int, B:int] -> sub -> [int]
	// replace A and B with their difference
	BRB_OP_SUBI, 	 // [A:int] -> sub-i <n> -> [int]
	// decrement A by <n>
	BRB_OP_SUBI@8,   // [A:ptr] -> sub-i@8 <n> -> [A:ptr]
	// decrement an `i8` at address A by <n>
	BRB_OP_SUBI@16,  // [A:ptr] -> sub-i@16 <n> -> [A:ptr]
	// decrement an `i16` at address A by <n>
	BRB_OP_SUBI@32,  // [A:ptr] -> sub-i@32 <n> -> [A:ptr]
	// decrement an `i32` at address A by <n>
	BRB_OP_SUBI@P,   // [A:ptr] -> sub-i@p <n>  -> [A:ptr]
	// decrement a `ptr` at address A by <n>
	BRB_OP_SUBI@64,  // [A:ptr] -> sub-i@64 <n> -> [A:ptr]
	// decrement an `i64` at address A by <n>

	BRB_OP_MUL,      // [A:int, B:int] -> mul -> [int]
	// replace A and B with their product
	BRB_OP_MULI, 	 // [A:int] -> mul-i <n> -> [int]
	// multiply A by <n>
	BRB_OP_MULI@8,   // [A:ptr] -> mul-i@8 <n> -> [A:ptr]
	// multiply an `i8` at address A by <n>
	BRB_OP_MULI@16,  // [A:ptr] -> mul-i@16 <n> -> [A:ptr]
	// multiply an `i16` at address A by <n>
	BRB_OP_MULI@32,  // [A:ptr] -> mul-i@32 <n> -> [A:ptr]
	// multiply an `i32` at address A by <n>
	BRB_OP_MULI@P,   // [A:ptr] -> mul-i@p <n>  -> [A:ptr]
	// multiply a `ptr` at address A by <n>
	BRB_OP_MULI@64,  // [A:ptr] -> mul-i@64 <n> -> [A:ptr]
	// multiply an `i64` at address A by <n>

	BRB_OP_DIV,      // [A:int, B:int] -> div -> [int]
	// replace A and B with their quotient; division is unsigned
	BRB_OP_DIVI, 	 // [A:int] -> div-i <n> -> [int]
	// divide A by <n>; division is unsigned
	BRB_OP_DIVI@8,   // [A:ptr] -> div-i@8 <n> -> [A:ptr]
	// divide an `i8` at address A by <n>; division is unsigned
	BRB_OP_DIVI@16,  // [A:ptr] -> div-i@16 <n> -> [A:ptr]
	// divide an `i16` at address A by <n>; division is unsigned
	BRB_OP_DIVI@32,  // [A:ptr] -> div-i@32 <n> -> [A:ptr]
	// divide an `i32` at address A by <n>; division is unsigned
	BRB_OP_DIVI@P,   // [A:ptr] -> div-i@p <n>  -> [A:ptr]
	// divide a `ptr` at address A by <n>; division is unsigned
	BRB_OP_DIVI@64,  // [A:ptr] -> div-i@64 <n> -> [A:ptr]
	// divide an `i64` at address A by <n>; division is unsigned

	BRB_OP_DIVS,     // [A:int, B:int] -> divs -> [int]
	// replace A and B with their quotient; divsision is signed
	BRB_OP_DIVSI, 	 // [A:int] -> divs-i <n> -> [int]
	// divide A by <n>; division is signed
	BRB_OP_DIVSI@8,  // [A:ptr] -> divs-i@8 <n> -> [A:ptr]
	// divide an `i8` at address A by <n>; division is signed
	BRB_OP_DIVSI@16, // [A:ptr] -> divs-i@16 <n> -> [A:ptr]
	// divide an `i16` at address A by <n>; division is signed
	BRB_OP_DIVSI@32, // [A:ptr] -> divs-i@32 <n> -> [A:ptr]
	// divide an `i32` at address A by <n>; division is signed
	BRB_OP_DIVSI@P,  // [A:ptr] -> divs-i@p <n>  -> [A:ptr]
	// divide a `ptr` at address A by <n>; division is signed
	BRB_OP_DIVSI@64, // [A:ptr] -> divs-i@64 <n> -> [A:ptr]
	// divide an `i64` at address A by <n>; division is signed

	BRB_OP_MOD,      // [A:int, B:int] -> mod -> [int]
	// replace A and B with the remainder of A / B; division is unsigned
	BRB_OP_MODI, 	 // [A:int] -> mod-i <n> -> [int]
	// replace A with the remainder of A / <n>; division is unsigned
	BRB_OP_MODI@8,   // [A:ptr] -> mod-i@8 <n> -> [A:ptr]
	// replace an `i8` at address A with the remainder of it divided by <n>; division is unsigned
	BRB_OP_MODI@16,  // [A:ptr] -> mod-i@16 <n> -> [A:ptr]
	// replace an `i16` at address A with the remainder of it divided by <n>; division is unsigned
	BRB_OP_MODI@32,  // [A:ptr] -> mod-i@32 <n> -> [A:ptr]
	// replace an `i32` at address A with the remainder of it divided by <n>; division is unsigned
	BRB_OP_MODI@P,   // [A:ptr] -> mod-i@p <n>  -> [A:ptr]
	// replace an `ptr` at address A with the remainder of it divided by <n>; division is unsigned
	BRB_OP_MODI@64,  // [A:ptr] -> mod-i@64 <n> -> [A:ptr]
	// replace an `i64` at address A with the remainder of it divided by <n>; division is unsigned

	BRB_OP_MODS,     // [A:int, B:int] -> mods -> [int]
	// replace A and B with the remainder of A / B; division is signed
	BRB_OP_MODSI, 	 // [A:int] -> mods-i <n> -> [int]
	// replace A with the remainder of A / <n>; division is signed
	BRB_OP_MODSI@8,  // [A:ptr] -> mods-i@8 <n> -> [A:ptr]
	// replace an `i8` at address A with the remainder of it divided by <n>; division is signed
	BRB_OP_MODSI@16, // [A:ptr] -> mods-i@16 <n> -> [A:ptr]
	// replace an `i16` at address A with the remainder of it divided by <n>; division is signed
	BRB_OP_MODSI@32, // [A:ptr] -> mods-i@32 <n> -> [A:ptr]
	// replace an `i32` at address A with the remainder of it divided by <n>; division is signed
	BRB_OP_MODSI@P,  // [A:ptr] -> mods-i@p <n>  -> [A:ptr]
	// replace an `ptr` at address A with the remainder of it divided by <n>; division is signed
	BRB_OP_MODSI@64, // [A:ptr] -> mods-i@64 <n> -> [A:ptr]
	// replace an `i64` at address A with the remainder of it divided by <n>; division is signed

	BRB_OP_AND,      // [A:int, B:int] -> and -> [int]
	// replace A and B with the result of bitwise AND operation on A and B
	BRB_OP_ANDI,     // [A:int] -> and-i <n> -> [int]
	// replace A with the result of bitwise AND operation on A and <n>
	BRB_OP_ANDI@8,   // [A:ptr] -> and-i@8 <n> -> [A:ptr]
	// replace an `i8` at address A with the result of bitwise AND operation on the value and <n>
	BRB_OP_ANDI@16,  // [A:ptr] -> and-i@16 <n> -> [A:ptr]
	// replace an `i16` at address A with the result of bitwise AND operation on the value and <n>
	BRB_OP_ANDI@32,  // [A:ptr] -> and-i@32 <n> -> [A:ptr]
	// replace an `i32` at address A with the result of bitwise AND operation on the value and <n>
	BRB_OP_ANDI@P,   // [A:ptr] -> and-i@p <n> -> [A:ptr]
	// replace an `ptr` at address A with the result of bitwise AND operation on the value and <n>
	BRB_OP_ANDI@64,  // [A:ptr] -> and-i@64 <n> -> [A:ptr]
	// replace an `i64` at address A with the result of bitwise AND operation on the value and <n>

	BRB_OP_OR,       // [A:int, B:int] -> or -> [int]
	// replace A and B with the result of bitwise OR operation on A and B
	BRB_OP_ORI,      // [A:int] -> or-i <n> -> [int]
	// replace A with the result of bitwise OR operation on A and <n>
	BRB_OP_ORI@8,    // [A:ptr] -> or-i@8 <n> -> [A:ptr]
	// replace an `i8` at address A with the result of bitwise OR operation on the value and <n>
	BRB_OP_ORI@16,   // [A:ptr] -> or-i@16 <n> -> [A:ptr]
	// replace an `i16` at address A with the result of bitwise OR operation on the value and <n>
	BRB_OP_ORI@32,   // [A:ptr] -> or-i@32 <n> -> [A:ptr]
	// replace an `i32` at address A with the result of bitwise OR operation on the value and <n>
	BRB_OP_ORI@P,    // [A:ptr] -> or-i@p <n> -> [A:ptr]
	// replace an `ptr` at address A with the result of bitwise OR operation on the value and <n>
	BRB_OP_ORI@64,   // [A:ptr] -> or-i@64 <n> -> [A:ptr]
	// replace an `i64` at address A with the result of bitwise OR operation on the value and <n>

	BRB_OP_XOR,      // [A:int, B:int] -> xor -> [int]
	// replace A and B with the result of bitwise XOR operation on A and B
	BRB_OP_XORI,     // [A:int] -> xor-i <n> -> [int]
	// replace A with the result of bitwise XOR operation on A and <n>
	BRB_OP_XORI@8,   // [A:ptr] -> xor-i@8 <n> -> [A:ptr]
	// replace an `i8` at address A with the result of bitwise XOR operation on the value and <n>
	BRB_OP_XORI@16,  // [A:ptr] -> xor-i@16 <n> -> [A:ptr]
	// replace an `i16` at address A with the result of bitwise XOR operation on the value and <n>
	BRB_OP_XORI@32,  // [A:ptr] -> xor-i@32 <n> -> [A:ptr]
	// replace an `i32` at address A with the result of bitwise XOR operation on the value and <n>
	BRB_OP_XORI@P,   // [A:ptr] -> xor-i@p <n> -> [A:ptr]
	// replace an `ptr` at address A with the result of bitwise XOR operation on the value and <n>
	BRB_OP_XORI@64,  // [A:ptr] -> xor-i@64 <n> -> [A:ptr]
	// replace an `i64` at address A with the result of bitwise XOR operation on the value and <n>

	BRB_OP_SHL,      // [A:int, B:int] -> shl -> [int]
	// replace A and B by A shifted by B bits to the left; B must be in range [0, 64)
	BRB_OP_SHLI,     // [A:int] -> shl-i <n> -> [int]
	// shift A by <n> bits to the left; <n> must be in range [0, 64)
	BRB_OP_SHLI@8,   // [A:ptr] -> shl-i@8 <n> -> [A:ptr]
	// shift an `i8` at address A by <n> bits to the left; <n> must be in range [0, 64)
	BRB_OP_SHLI@16,  // [A:ptr] -> shl-i@16 <n> -> [A:ptr]
	// shift an `i16` at address A by <n> bits to the left; <n> must be in range [0, 64)
	BRB_OP_SHLI@32,  // [A:ptr] -> shl-i@32 <n> -> [A:ptr]
	// shift an `i32` at address A by <n> bits to the left; <n> must be in range [0, 64)
	BRB_OP_SHLI@P,   // [A:ptr] -> shl-i@p <n> -> [A:ptr]
	// shift an `ptr` at address A by <n> bits to the left; <n> must be in range [0, 64)
	BRB_OP_SHLI@64,  // [A:ptr] -> shl-i@64 <n> -> [A:ptr]
	// shift an `i64` at address A by <n> bits to the left; <n> must be in range [0, 64)

	BRB_OP_SHR,      // [A:int, B:int] -> shr -> [int]
	// replace A and B by A shifted by B bits to the right, shifting in zeros; B must be in range [0, 64)
	BRB_OP_SHRI,     // [A:int] -> shr-i <n> -> [int]
	// shift A by <n> bits to the right, shifting in zeros; <n> must be in range [0, 64)
	BRB_OP_SHRI@8,   // [A:ptr] -> shr-i@8 <n> -> [A:ptr]
	// shift an `i8` at address A by <n> bits to the right, shifting in zeros; <n> must be in range [0, 64)
	BRB_OP_SHRI@16,  // [A:ptr] -> shr-i@16 <n> -> [A:ptr]
	// shift an `i16` at address A by <n> bits to the right, shifting in zeros; <n> must be in range [0, 64)
	BRB_OP_SHRI@32,  // [A:ptr] -> shr-i@32 <n> -> [A:ptr]
	// shift an `i32` at address A by <n> bits to the right, shifting in zeros; <n> must be in range [0, 64)
	BRB_OP_SHRI@P,   // [A:ptr] -> shr-i@p <n> -> [A:ptr]
	// shift an `ptr` at address A by <n> bits to the right, shifting in zeros; <n> must be in range [0, 64)
	BRB_OP_SHRI@64,  // [A:ptr] -> shr-i@64 <n> -> [A:ptr]
	// shift an `i64` at address A by <n> bits to the right, shifting in zeros; <n> must be in range [0, 64)

	BRB_OP_SHRS,     // [A:int, B:int] -> shrs -> [int]
	// replace A and B by A shifted by B bits to the right, shifting in copies of the sign bit; B must be in range [0, 64)
	BRB_OP_SHRSI,    // [A:int] -> shrs-i <n> -> [int]
	// shift A by <n> bits to the right, shifting in copies of the sign bit; <n> must be in range [0, 64)
	BRB_OP_SHRSI@8,  // [A:ptr] -> shrs-i@8 <n> -> [A:ptr]
	// shift an `i8` at address A by <n> bits to the right, shifting in copies of the sign bit; <n> must be in range [0, 64)
	BRB_OP_SHRSI@16, // [A:ptr] -> shrs-i@16 <n> -> [A:ptr]
	// shift an `i16` at address A by <n> bits to the right, shifting in copies of the sign bit; <n> must be in range [0, 64)
	BRB_OP_SHRSI@32, // [A:ptr] -> shrs-i@32 <n> -> [A:ptr]
	// shift an `i32` at address A by <n> bits to the right, shifting in copies of the sign bit; <n> must be in range [0, 64)
	BRB_OP_SHRSI@P,  // [A:ptr] -> shrs-i@p <n> -> [A:ptr]
	// shift an `ptr` at address A by <n> bits to the right, shifting in copies of the sign bit; <n> must be in range [0, 64)
	BRB_OP_SHRSI@64, // [A:ptr] -> shrs-i@64 <n> -> [A:ptr]
	// shift an `i64` at address A by <n> bits to the right, shifting in copies of the sign bit; <n> must be in range [0, 64)

	BRB_OP_NOT,      // [A:int] -> inv -> [int]
	// invertsthe bits of A
	BRB_OP_NOT@8,    // [A:ptr] -> inv-@8 -> [A:ptr]
	// invertsthe bits of an `i8` at address A
	BRB_OP_NOT@16,   // [A:ptr] -> inv-@16 -> [A:ptr]
	// invertsthe bits of an `i16` at address A
	BRB_OP_NOT@32,   // [A:ptr] -> inv-@32 -> [A:ptr]
	// invertsthe bits of an `i32` at address A
	BRB_OP_NOT@P,    // [A:ptr] -> inv-@p -> [A:ptr]
	// invertsthe bits of an `ptr` at address A
	BRB_OP_NOT@64,   // [A:ptr] -> inv-@64 -> [A:ptr]
	// invertsthe bits of an `i64` at address A

	BRB_OP_NEW,      // [] -> new <T> -> [<T>]
	// create new item of type <T> on top of the stack; content of the item is undefined
	BRB_OP_NEWZ,     // [] -> new-z <T> -> [<T>]
	// create new item of type <T> on top of the stack, with every byte initialized to 0
	BRB_OP_DUP,      // [A:x...] -> dup <i> -> [A:x...A:x]
	// duplicates stack item at index <i> on top of the stack
	BRB_OP_SWAP,     // [A:x...B:x] -> swap <i> -> [B:x...A:x]
	// swaps the contents of A and B, A being the stack head and B being at index <i>
	BRB_OP_SLICE,    // [A] -> slice <n, offset> -> [A]
	// replaces A with its slice of <n> bytes, starting from <offset>
	BRB_OP_JOIN,     // [...] -> join <n> -> [A]
	// combine <n> items on top of the stack into a single item

	BRB_OP_EQU,      // [A:x, B:x] -> equ -> i8
	// compare A and B and replace them with a byte, the value of which will be 1 if they are equal, and 0 otherwise
	BRB_OP_NEQ,      // [A:x, B:x] -> neq -> i8
	// compare A and B and replace them with a byte, the value of which will be 1 if they are equal, and 0 otherwise
	BRB_OP_LTU,      // [A:int, B:int] -> ltu -> i8
	// compare A and B and replace them with a byte, the value of which will be 1 if A < B, and 0 otherwise; comparison is unsigned
	BRB_OP_LTS,      // [A:int, B:int] -> lts -> i8
	// compare A and B and replace them with a byte, the value of which will be 1 if A < B, and 0 otherwise; comparison is signed
	BRB_OP_GTU,      // [A:int, B:int] -> gtu -> i8
	// compare A and B and replace them with a byte, the value of which will be 1 if A > B, and 0 otherwise; comparison is unsigned
	BRB_OP_GTS,      // [A:int, B:int] -> gts -> i8
	// compare A and B and replace them with a byte, the value of which will be 1 if A > B, and 0 otherwise; comparison is signed
	BRB_OP_LEU,      // [A:int, B:int] -> leu -> i8
	// compare A and B and replace them with a byte, the value of which will be 1 if A <= B, and 0 otherwise; comparison is unsigned
	BRB_OP_LES,      // [A:int, B:int] -> les -> i8
	// compare A and B and replace them with a byte, the value of which will be 1 if A <= B, and 0 otherwise; comparison is signed
	BRB_OP_GEU,      // [A:int, B:int] -> geu -> i8
	// compare A and B and replace them with a byte, the value of which will be 1 if A >= B, and 0 otherwise; comparison is unsigned
	BRB_OP_GES,      // [A:int, B:int] -> ges -> i8
	// compare A and B and replace them with a byte, the value of which will be 1 if A >= B, and 0 otherwise; comparison is signed
	BRB_OP_GOTO,     // [] -> goto <i> -> []
	// jump to operation at index <i>
	BRB_OP_GOTOIF,   // [A:int] -> gotoif <i> -> []
	// jump to operation at index <i> if A is not zero
	BRB_OP_GOTOIFN,  // [A:int] -> gotoifn <i> -> []
	// jump to operation at index <i> if A is zero

	BRB_OP_CM,       // [A:ptr, B:int] -> cm <n> -> [A:ptr]
	// copy <n> bytes from address B to address A, pop B from the stack; the addresses must not overlap
	BRB_OP_CMV,      // [A;ptr, B:ptr, C:int] -> cm-v -> [A:ptr]
	// copy C bytes from address B to address A, pop B and C from the stack; the address must not overlap
	BRB_OP_CMO,      // [A:ptr, B:int] -> cm-o <n> -> [A:ptr]
	// copy <n> bytes from address B to address A, pop B from the stack; the addresses may overlap
	BRB_OP_CMOV,     // [A;ptr, B:ptr, C:int] -> cm-ov -> [A:ptr]
	// copy C bytes from address B to address A, pop B and C from the stack; the address may overlap
	BRB_OP_ZM,       // [A:ptr] -> zm <n> -> [A:ptr]
	// set <n> bytes at address A to zero
	BRB_OP_ZMV,      // [A:ptr, B:int] -> zm-v -> [A:ptr]
	// sets B bytes at address A to zero, pop B from the stack
	BRB_OP_FM,       // [A:ptr, B] -> fm <n> -> [A:ptr]
	// fill memory at address A with <n> copies of B, pop B from the stack
	BRB_OP_FMV,      // [A:ptr, B, C:int] -> fm-v -> [A:ptr]
	// fill memory at address A with C copies of B, pop B and C from the stack
	BRB_OP_LDI,	 // [A:ptr, B:int] -> ld-i <T> -> [<T>]
	// load item of type <T> from address A at index B
	BRB_OP_STRI,	 // [A:ptr, B:int, C] -> str-i -> []
	// write C to address A at index B

	BRB_OP_CI8,      // [A:int] -> ci8 -> [A:i8]
	// convert A to an `i8`
	BRB_OP_CI16,     // [A:int] -> ci16 -> [A:i16]
	// convert A to an `i16`, zero-extending if the resulting value is larger
	BRB_OP_CI16S,     // [A:int] -> ci16-s -> [A:i16]
	// convert A to an `i16`, sign-extending if the resulting value is larger
	BRB_OP_CI32,     // [A:int] -> ci32 -> [A:i32]
	// convert A to an `i32`, zero-extending if the resulting value is larger
	BRB_OP_CI32S,     // [A:int] -> ci32-s -> [A:i32]
	// convert A to an `i32`, sign-extending if the resulting value is larger
	BRB_OP_CIP,      // [A:int] -> cip ->  [A:ptr]
	// convert A to an `ptr`, zero-extending if the resulting value is larger
	BRB_OP_CIPS,      // [A:int] -> cip-s ->  [A:ptr]
	// convert A to an `ptr`, sign-extending if the resulting value is larger
	BRB_OP_CI64,     // [A:int] -> ci64 -> [A:i64]
	// convert A to an `i64`, zero-extending if the resulting value is larger
	BRB_OP_CI64S,     // [A:int] -> ci64-s -> [A:i64]
	// convert A to an `i64`, sign-extending if the resulting value is larger
*/
	BRB_N_OPS
} BRB_OpType;

extern const sbuf BRB_opNames[];

#define BRB_OPF_OPERAND_INT8 1
#define BRB_OPF_OPERAND_INT  2
#define BRB_OPF_OPERAND_TYPE 4
#define BRB_OPF_OPERAND_VAR_NAME     (BRB_OPF_OPERAND_INT  |  8)
#define BRB_OPF_OPERAND_DB_NAME      (BRB_OPF_OPERAND_INT  | 16)
#define BRB_OPF_OPERAND_SYSCALL_NAME (BRB_OPF_OPERAND_INT8 | 32)
#define BRB_OPF_OPERAND_BUILTIN      (BRB_OPF_OPERAND_INT8 | 64)
#define BRB_OPF_HAS_OPERAND   \
	( BRB_OPF_OPERAND_INT8 \
	| BRB_OPF_OPERAND_INT   \
	| BRB_OPF_OPERAND_TYPE   )

static const uint8_t BRB_opFlags[] = {
	[BRB_OP_NOP] = 0,
	[BRB_OP_END] = 0,
	[BRB_OP_I8] = BRB_OPF_OPERAND_INT8,
	[BRB_OP_I16] = BRB_OPF_OPERAND_INT,
	[BRB_OP_I32] = BRB_OPF_OPERAND_INT,
	[BRB_OP_I64] = BRB_OPF_OPERAND_INT,
	[BRB_OP_PTR] = BRB_OPF_OPERAND_INT,
	[BRB_OP_ADDR] = BRB_OPF_OPERAND_VAR_NAME,
	[BRB_OP_DBADDR] = BRB_OPF_OPERAND_DB_NAME,
	[BRB_OP_LD] = BRB_OPF_OPERAND_TYPE,
	[BRB_OP_STR] = 0,
	[BRB_OP_SYS] = BRB_OPF_OPERAND_SYSCALL_NAME,
	[BRB_OP_BUILTIN] = BRB_OPF_OPERAND_BUILTIN
};
static_assert(sizeof(BRB_opFlags) / sizeof(BRB_opFlags[0]) == BRB_N_OPS, "not all BRB operations have their flags set defined in `BRB_opFlags`"); 

typedef enum {
	BRB_TYPE_I8,
	BRB_TYPE_I16,	
	BRB_TYPE_I32,
	BRB_TYPE_PTR,
	BRB_TYPE_I64,	
	BRB_TYPE_VOID,
	BRB_N_TYPE_KINDS
} BRB_TypeKind;

extern const sbuf BRB_typeNames[];

#define BRB_I8_TYPE(n)  ((BRB_Type){.kind = BRB_TYPE_I8,  .n_items = n})
#define BRB_I16_TYPE(n) ((BRB_Type){.kind = BRB_TYPE_I16, .n_items = n})
#define BRB_I32_TYPE(n) ((BRB_Type){.kind = BRB_TYPE_I32, .n_items = n})
#define BRB_PTR_TYPE(n) ((BRB_Type){.kind = BRB_TYPE_PTR, .n_items = n})
#define BRB_I64_TYPE(n) ((BRB_Type){.kind = BRB_TYPE_I64, .n_items = n})
#define BRB_VOID_TYPE   ((BRB_Type){.kind = BRB_TYPE_VOID             })
typedef struct {
	BRB_TypeKind kind:8;
	bool is_any;
	bool is_any_int;
	uint32_t n_items;
} BRB_Type;
static_assert(sizeof(BRB_Type) <= sizeof(uint64_t), "just for compactness");
declArray(BRB_Type);

typedef struct {
	BRB_OpType type:8;
	union {
		uint64_t operand_u;	
		int64_t operand_s;
		BRB_Type operand_type;
	};
} BRB_Op;
static_assert(sizeof(BRB_Op) <= 16, "`sizeof(BRB_Op) > 16`, that's no good, gotta save up on that precious memory");
declArray(BRB_Op);

typedef struct {
	const char* name;
	BRB_OpArray body;
	BRB_Type ret_type;
	BRB_TypeArray args;
} BRB_Proc;
declArray(BRB_Proc);

// System Calls: crossplatform way to use system-dependent functionality
typedef enum {
	BRB_SYS_EXIT,
	BRB_SYS_WRITE,
	BRB_SYS_READ,
	BRB_N_SYSCALLS
} BRB_Syscall;

extern const sbuf BRB_syscallNames[];

extern const size_t BRB_syscallNArgs[];

// Built-ins: crossplatform way to get system-dependent constants
// all built-ins are of pointer size
typedef enum {
	BRB_BUILTIN_NULL,
	BRB_BUILTIN_STDIN,
	BRB_BUILTIN_STDOUT,
	BRB_BUILTIN_STDERR,
	BRB_N_BUILTINS
} BRB_Builtin;

extern const uintptr_t BRB_builtinValues[];
extern const sbuf BRB_builtinNames[];

typedef enum {
	BRB_DP_NONE,
	BRB_DP_BYTES, // span of bytes stored in the bytecode along with a size
	BRB_DP_I16, // 16-bit integer
	BRB_DP_I32, // 32-bit integer
	BRB_DP_PTR, // pointer-sized integer
	BRB_DP_I64, // 64-bit integer
	BRB_DP_TEXT, // span of bytes stored in the bytecode as a null-terminated string; takes less space than `bytes` data piece, but cannot store a null byte
	BRB_DP_DBADDR, // pointer to the contents of a data block
	BRB_DP_ZERO, // span of null bytes of arbitrary size
	BRB_DP_BUILTIN, // pointer-sized integer, storing a built-in constant
	BRB_N_DP_TYPES
} BRB_DataPieceType;
// TODO: add pre-evaluation data pieces that would pre-evaluate a block of code and embed its output as a data piece

extern const sbuf BRB_dataPieceNames[];

typedef struct {
	BRB_DataPieceType type;
	union {
		sbuf data; // for PIECE_BYTES or PIECE_TEXT
		uint64_t content_u;
		int64_t content_s;
		BRB_Type content_type;
	};
} BRB_DataPiece;
declArray(BRB_DataPiece);

typedef struct {
	const char* name;
	union {
		BRB_DataPieceArray pieces;
		sbuf data;
	};
	bool is_mutable;
} BRB_DataBlock;
declArray(BRB_DataBlock);

typedef struct {
	BRB_ProcArray seg_exec;
	BRB_DataBlockArray seg_data;
	size_t exec_entry_point;
} BRB_Module;

#define BRB_HEADER_SIZE 8
#define BRB_V1_HEADER fromcstr("BRBv1\0\0\0")

typedef enum {
	BRB_ERR_OK,
	BRB_ERR_INVALID_HEADER,
	BRB_ERR_NO_HEADER,
	BRB_ERR_NO_DATA_SEG,
	BRB_ERR_NO_MEMORY,
	BRB_ERR_NO_EXEC_SEG,
	BRB_ERR_NO_OPCODE,
	BRB_ERR_INVALID_OPCODE,
	BRB_ERR_NO_OPERAND,
	BRB_ERR_INVALID_NAME,
	BRB_ERR_NAMES_NOT_RESOLVED,
	BRB_ERR_INVALID_BUILTIN,
	BRB_ERR_INVALID_SYSCALL,
	BRB_ERR_STACK_UNDERFLOW,
	BRB_ERR_OPERAND_TOO_LARGE,
	BRB_ERR_OPERAND_OUT_OF_RANGE,
	BRB_ERR_NO_PROC_RET_TYPE,
	BRB_ERR_NO_PROC_NAME,
	BRB_ERR_NO_PROC_ARG,
	BRB_ERR_NO_PROC_BODY_SIZE,
	BRB_ERR_NO_DP_TYPE,
	BRB_ERR_INVALID_DP_TYPE,
	BRB_ERR_NO_DP_CONTENT,
	BRB_ERR_NO_DB_NAME,
	BRB_ERR_NO_DB_BODY_SIZE,
	BRB_ERR_NO_ENTRY,
	BRB_ERR_INVALID_ENTRY,
	BRB_ERR_INVALID_ENTRY_PROTOTYPE,
	BRB_ERR_TYPE_EXPECTED,
	BRB_ERR_OP_NAME_EXPECTED,
	BRB_ERR_INT_OPERAND_EXPECTED,
	BRB_ERR_INT_OR_NAME_OPERAND_EXPECTED,
	BRB_ERR_BUILTIN_OPERAND_EXPECTED,
	BRB_ERR_DP_NAME_EXPECTED,
	BRB_ERR_TEXT_OPERAND_EXPECTED,
	BRB_ERR_INVALID_TEXT_OPERAND,
	BRB_ERR_INVALID_DECL,
	BRB_ERR_ARGS_EXPECTED,
	BRB_ERR_PROTOTYPE_MISMATCH,
	BRB_ERR_SYSCALL_NAME_EXPECTED,
	BRB_ERR_INVALID_ARRAY_SIZE_SPEC,
	BRB_ERR_TYPE_MISMATCH,
	BRB_ERR_DEL_ARGS,
	BRB_N_ERROR_TYPES
} BRB_ErrorType;

typedef struct {
	BRB_ErrorType type;
	uint8_t opcode;
	BRP_TokenLoc* loc;
	union {
		const char* name;
		char header[BRB_HEADER_SIZE];
		BRB_Builtin builtin_id;
		BRB_Syscall syscall_id;
		struct {
			uint32_t expected_stack_length;
			uint32_t actual_stack_length;
		};
		struct {
			BRB_Type expected_type;
			BRB_Type actual_type;
			uint32_t arg_id;
		};
		uint64_t operand;
	};
} BRB_Error;

typedef struct BRB_stacknode_t* BRB_StackNode;
struct BRB_stacknode_t {
	BRB_StackNode prev;
	const char* name;
	BRB_Type type;
	uint8_t flags;
};
declArray(BRB_StackNode);
declArray(BRB_StackNodeArray);

typedef struct {
	BRB_Module module;
	BRB_StackNodeArrayArray procs;
	BRB_Error error;
	Arena arena;
} BRB_ModuleBuilder;

typedef enum {
	BRB_EXC_CONTINUE,
	BRB_EXC_EXIT,
	BRB_EXC_END,
	BRB_EXC_INTERRUPT,
	BRB_EXC_UNKNOWN_OP,
	BRB_EXC_STACK_OVERFLOW,
	N_BRB_EXCS
} BRB_ExecStatusType;

typedef struct {
	BRB_ExecStatusType type;
	uint8_t exit_code; // for BRB_EXC_EXIT
} BRB_ExecStatus;

declArray(sbuf);
#define BRB_DEFAULT_STACK_SIZE (512 * 1024) /* 512 KBs, just like in JVM */
typedef struct {
	sbufArray seg_data;
	BRB_ProcArray seg_exec;
	const BRB_Op* cur_proc;
	sbuf stack;
	char* stack_head;
	uint32_t exec_index;
	BRB_ExecStatus exec_status;
	sbuf* exec_argv;
	uint32_t exec_argc;
	uint32_t entry_point;
} BRB_ExecEnv;

typedef const char** field;
declArray(field);
defArray(field);
// implemented in `src/brb_core.c`
void       BRB_printErrorMsg(FILE* dst, BRB_Error err, const char* prefix);
char*      BRB_getErrorMsg(BRB_Error err, const char* prefix);

fieldArray BRB_getNameFields(BRB_Module* module);
FILE*      BRB_findModule(const char* module_name, const char* search_paths[]);

BRB_Error  BRB_initModuleBuilder(BRB_ModuleBuilder* builder);
BRB_Error  BRB_analyzeModule(const BRB_Module* module, BRB_ModuleBuilder* dst);
BRB_Error  BRB_extractModule(BRB_ModuleBuilder builder, BRB_Module* dst);
BRB_Error  BRB_setEntryPoint(BRB_ModuleBuilder* builder, size_t proc_id);

BRB_Error  BRB_preallocExecSegment(BRB_ModuleBuilder* builder, uint32_t n_procs_hint);
BRB_Error  BRB_addProc(BRB_ModuleBuilder* builder, uint32_t* proc_id_p, const char* name, size_t n_args, BRB_Type* args, BRB_Type ret_type, uint32_t n_ops_hint);
BRB_Error  BRB_addOp(BRB_ModuleBuilder* builder, uint32_t proc_id, BRB_Op op);
size_t     BRB_getProcIdByName(BRB_Module* module, const char* name); // returns SIZE_MAX on error

BRB_Error  BRB_preallocDataSegment(BRB_ModuleBuilder* builder, uint32_t n_dbs_hint);
BRB_Error  BRB_addDataBlock(BRB_ModuleBuilder* builder, uint32_t* db_id_p, const char* name, bool is_mutable, uint32_t n_pieces_hint);
BRB_Error  BRB_addDataPiece(BRB_ModuleBuilder* builder, uint32_t db_id, BRB_DataPiece piece);
size_t     BRB_getDataBlockIdByName(BRB_Module* module, const char* name); // returns SIZE_MAX on error

BRB_Error  BRB_labelStackItem(BRB_ModuleBuilder* builder, uint32_t proc_id, uint32_t op_id, uint32_t item_id, const char* name);
bool       BRB_getStackItemType(BRB_ModuleBuilder* builder, BRB_Type* dst, uint32_t proc_id, uint32_t op_id, uint32_t item_id);
size_t     BRB_getStackItemRTOffset(BRB_ModuleBuilder* builder, uint32_t proc_id, uint32_t op_id, size_t item_id); // if `item_id` is not SIZE_MAX, returns SIZE_MAX on error
size_t     BRB_getStackRTSize(BRB_ModuleBuilder* builder, uint32_t proc_id, uint32_t op_id);
size_t     BRB_getStackItemRTSize(BRB_ModuleBuilder* builder, uint32_t proc_id, uint32_t op_id, uint32_t item_id); // returns SIZE_MAX on error
ssize_t    BRB_getStackRTSizeDiff(BRB_ModuleBuilder* builder, uint32_t proc_id, uint32_t op_id);
size_t     BRB_getStackItemIdByName(BRB_ModuleBuilder* builder, uint32_t proc_id, uint32_t op_id, const char* name); // returns SIZE_MAX on error
size_t     BRB_getTypeRTSize(BRB_Type type);

// implemented in `src/brb_write.c`
long       BRB_writeModule(BRB_Module src, FILE* dst);

// implemented in `src/brb_load.c`
BRB_Error  BRB_loadModule(FILE* src, BRB_Module* dst);

// implemented in `src/brb_exec.c`
BRB_Error  BRB_initExecEnv(BRB_ExecEnv* env, BRB_Module module, size_t stack_size);
void       BRB_execModule(BRB_ExecEnv* env, char* args[], const volatile bool* interruptor);
void       BRB_delExecEnv(BRB_ExecEnv* env);

// implemented in `src/brb_asm.c`
BRB_Error  BRB_assembleModule(FILE* input, const char* input_name, BRB_Module* dst);

// implemented in `src/brb_dis.c`
long       BRB_printType(BRB_Type type, FILE* dst);
long       BRB_disassembleModule(const BRB_Module* module, FILE* dst);

// implemented in `src/brb_compile.c`
long       BRB_compileModule_darwin_arm64(const BRB_Module* module, FILE* dst);

#endif // _BRB_
