# Bitstructs

A `bitstruct` is a type where each field occupies a specific range of bits within an underlying integer. The compiler automatically generates the shifts and masks for reading/writing each field.

```lucis
bitstruct Control : uint16 {
    enable  : 1;     // bit 0
    mode    : 2;     // bits 1-2
    status  : 3;     // bits 3-5
    counter : 10;    // bits 6-15
}
```

## Declaration

### Explicit backing type

```lucis
bitstruct Name : uint8  { ... }
bitstruct Name : uint16 { ... }
bitstruct Name : uint32 { ... }
bitstruct Name : uint64 { ... }
bitstruct Name : uint128 { ... }
```

The sum of all field widths must not exceed the backing type's width.

### `auto` backing type

The compiler picks the smallest unsigned type that fits all fields:

```lucis
bitstruct Control : auto {
    enable  : 1;
    counter : 10;
}
```

| Total bits | Chosen type |
|---|---|
| ≤ 8 | `uint8` |
| ≤ 16 | `uint16` |
| ≤ 32 | `uint32` |
| ≤ 64 | `uint64` |
| ≤ 128 | `uint128` |

### Fields

```lucis
bitstruct Ex : uint8 {
    flag   : 1;      // named 1-bit field
    value  : 3;      // named 3-bit field
    _      : 4;      // padding (unused bits)
}
```

Each field must have width > 0. Padding uses `_ : N`.

### Generic

```lucis
bitstruct Reg<T: uint32> : T {
    field1 : 8;
    field2 : 8;
    rest   : 16;
}
```

## Field Access

### Read

```lucis
Control reg;
int32 e = reg.enable;     // load + shift + mask → uint8
int32 c = reg.counter;    // load + shift + mask → uint16
```

### Write

```lucis
reg.enable = 1;           // RMW: load, clear bit, set bit, store
reg.counter = 42;
```

### Compound assignment

```lucis
reg.counter += 10;
reg.counter &= 0xFF;
reg.mode |= 2;
```

## Bitstruct Methods

| Method | Description |
|---|---|
| `reg.raw()` | Returns the raw backing integer value |
| `reg.clearAll()` | Sets all bits to 0 |
| `reg.setAll()` | Sets all bits to 1 |

```lucis
Control reg;
reg.counter = 42;
reg.enable = 1;
auto raw = reg.raw();     // raw = (42 << 1) | 1 = 85

reg.clearAll();           // reg.raw() == 0
reg.setAll();             // reg.raw() == 0xFFFF (all bits 1)
```

## MMIO with Volatile

For hardware register access at fixed addresses:

```lucis
bitstruct UART : uint32 {
    tx_ready : 1;
    rx_ready : 1;
    _        : 30;
}

fn main() int32 {
    volatile *UART reg = @ptr(volatile UART, 0x10001000);

    while !reg->rx_ready { /* spin */ }
    reg->tx_ready = 1;

    ret 0;
}
```

Each field access emits an individual `volatile load` / `volatile store`, ensuring the hardware sees each operation separately.

## Bitstruct as a Struct Field

```lucis
struct Device {
    uint32 id;
    Control reg;
}

fn main() int32 {
    Device dev;
    dev.reg.counter = 42;
    dev.reg.enable = 1;
}
```

## Complete Example

```lucis
use stdio::printf;

bitstruct Status : uint16 {
    ready   : 1;
    error   : 1;
    code    : 6;
    count   : 8;
}

fn main() int32 {
    Status s;

    s.ready = 1;
    s.code  = 42;
    s.count = 255;

    printf("code={d}\n", s.code);
    printf("count={d}\n", s.count);
    printf("raw={d}\n", s.raw());

    s.clearAll();
    printf("after clearAll raw={d}\n", s.raw());

    s.setAll();
    printf("after setAll raw={d}\n", s.raw());

    s.count = 100;
    s.count += 50;
    printf("count after +=50 ={d}\n", s.count);

    ret 0;
}
```

## Restrictions

- The backing type must be **unsigned** (`uint8`-`uint128`, `usize`, or `auto`)
- The sum of field widths must not exceed the backing type's width
- Each field must have width > 0
- `bitstruct` cannot be initialized with `{ }` syntax — it is zero-initialized by default
- There are no per-field `.set()`/`.clear()`/`.toggle()` methods (only `=` and compound assignment operators)

## See Also

- [Operators](operators.md) — compound assignment operators
- [`@ptr`](../language/README.md) — convert integer address to typed pointer
