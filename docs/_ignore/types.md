# Lucis Types

## Signed Integers
- int1, int8, int16, int32, int64, int128, intinf, isize | default: int32

## Unsigned Integers
- uint1, uint8, uint16, uint32, uint64, uint128, usize   | default: uint32

## Floating Point
- float32, float64, float80, float128                     | supports scientific notation (e.g. 1.0e10)
- double (alias of float64)

## Character
- char (8-bit, supports escape sequences: \n \r \t \\ \' \" \0 \a \b \f \v \xNN)

## Boolean
- bool (1-bit, true/false)

## String
- string (internally: struct { *[]uint8 data, usize len })
- cstring (alias of *char — C-compatible null-terminated string)

## Void
- void | return type for functions that return nothing

## Pointer
- *T (pointer to T) | &x (address-of), *p (dereference read), *p = val (dereference write)
- null (pointer literal) | null pointer value, assignable to any *T

## Arrays
- []T (dynamic array/slice), [][]T (2D), [][][]T (N-D) | stack-allocated, supports indexing and mutation
- [N]T (fixed-size array) | compile-time known size, e.g. [10]int32, [3][3]float64

## Function Pointer
- fn(T1, T2, ...) -> RetType | function type specification, e.g. fn(int32, int32) -> int32

## Struct
- struct Name { type field; } | user-defined composite types, field access (p.x), field mutation (p.x = val)

## Union
- union Name { type field; } | all fields share same memory, size = max field size

## Enum
- enum Name { Variant1, Variant2 } | internally i32, auto-incremented (0, 1, 2...), access via Name::Variant

## Type Alias
- type Alias = ExistingType; | creates a named alias for any type

## Generic Collections (native keywords)
- vec<T>    | dynamic growable array, supports push, pop, indexing, and iteration
- map<K, V> | open-addressing hash map, supports insert, get, remove, and iteration
- set<T>    | open-addressing hash set, supports insert, contains, remove, and iteration

## Concurrency
- Task<T>   | async task handle (opaque), represents a pending computation
- Mutex     | synchronization primitive

## Built-in Structs
- Error { message: string, file: string, line: int32, column: int32 }
