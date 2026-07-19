# Bare-metal e Atributos

## MMIO — Memory-Mapped I/O (`@ptr`)

Converte um endereço inteiro para um ponteiro tipado:

```lucis
*volatile Uart uart = @ptr(volatile Uart, 0x0900_0000);
uart.dr = 'H';
```

`@ptr(type, address)` — `type` é um type spec, `address` é uma expressão inteira.

## Qualificador `volatile`

Usado em tipos para marcar acesso volátil. Loads/stores através do
ponteiro emitem instruções LLVM `load volatile` / `store volatile`:

```lucis
*volatile int32 ptr = @ptr(volatile int32, 0x3F8);
```

Pode ser combinado com qualquer tipo: `volatile Uart`, `*volatile int32`,
`volatile [10]uint8`.

## Layout de struct — `#[repr(packed)]`

Remove padding entre campos:

```lucis
#[repr(packed)]
struct Uart {
  uint32  dr;
  uint32  rsr;
  uint16  ibrd;
  uint16  fbrc;
  uint16  lcr;
  uint16  cr;
}
```

## Controle de fluxo — `loop {}`

Loop infinito (compila para `br label %loop.body`).

## Atributos de função

### `#[naked]`

Remove prólogo/epílogo da função. O corpo deve conter apenas
inline asm:

```lucis
#[naked]
fn handler() void {
  asm volatile("iretq");
}
```

### `#[no_stack_probe]`

Desativa o stack probe (útil em kernels sem stack guard).

### `#[interrupt]` e `#[interrupt(vector = N)]`

Marca uma função como interrupt service routine. O LLVM gera
save/restore de todos os registradores e usa `iretq` no retorno
(calling convention `X86_INTR`):

```lucis
#[interrupt(vector = 15)]
fn irq_handler() void { }
```

### `#[vector_table]`

Gera automaticamente uma tabela de vetores de interrupção (IVT)
na seção `.isr_vector`, populada a partir de todos os
`#[interrupt(vector = N)]` do módulo:

```lucis
// A IVT é gerada automaticamente na seção .isr_vector
#[entry]
fn _start() void {
  loop {}
}

#[interrupt(vector = 15)]
fn irq_handler() void { }
```

A vector table é um array `[256 x ptr]` com as entry points
dos handlers. Vetores sem handler são null.

### `#[entry]`

Marca a função como entry point do programa. Equivalente a
`#[export]` + `--linker-entry nome`. O nome da função é preservado
(sem mangling) e passado ao linker via `-e`:

```lucis
#[entry]
fn _start() void {
  lucis::sys::outb(0x3F8u16, 'H' as uint8);
  loop {}
}
```

Prioridade do entry point:
1. `--linker-entry` (CLI)
2. `#[entry]` (atributo)
3. `lucis.yaml → linker.entry`

### `#[must_use]]

Emite warning se o valor de retorno da função for descartado:

```lucis
#[must_use]
fn get_value() int32 { return 42; }

fn main() int32 {
  get_value();  // warning: unused return value
  return 0;
}
```

## Compilação para bare-metal

### Com `#[entry]` e `#[interrupt]`

```sh
lucis build main.lc --no-std
qemu-system-x86_64 -device loader,file=main.out -serial stdio
```

### Com `#[entry]`, sem criar ELF

```sh
lucis build main.lc --no-std -q
qemu-system-x86_64 -device loader,file=main.out -serial stdio
```

### Apenas objeto (para linker manual)

```sh
lucis build main.lc --no-std --emit-obj -q
ld -o main.elf main.o -e _start -Ttext=0x100000 -nostdlib -static
qemu-system-x86_64 -device loader,file=main.elf -serial stdio
```

## Intrínsecos `lucis::sys`

Disponíveis sem `use`:

- `lucis::sys::outb(port: uint16, val: uint8)` — escreve byte na porta I/O
- `lucis::sys::outw(port: uint16, val: uint16)` — escreve word na porta I/O
- `lucis::sys::outl(port: uint16, val: uint32)` — escreve dword na porta I/O
- `lucis::sys::inb(port: uint16) -> uint8` — lê byte da porta I/O
- `lucis::sys::inw(port: uint16) -> uint16` — lê word da porta I/O
- `lucis::sys::inl(port: uint16) -> uint32` — lê dword da porta I/O
- `lucis::sys::cli()` — desativa interrupções
- `lucis::sys::sti()` — ativa interrupções
- `lucis::sys::hlt()` — para CPU até próxima interrupção
