# std::test

Built-in test framework with assertion functions.

## Import

```
use std::test::{ assertEqual, assertTrue, assertFalse, assertNear, log };
```

## Functions

### Assertions

| Function | Signature | Description |
|----------|-----------|-------------|
| `assertEqual` | `(T, T) -> void` | Assert two values are equal |
| `assertNotEqual` | `(T, T) -> void` | Assert two values are not equal |
| `assertTrue` | `(bool) -> void` | Assert condition is true |
| `assertFalse` | `(bool) -> void` | Assert condition is false |
| `assertGreater` | `(T, T) -> void` | Assert first > second |
| `assertLess` | `(T, T) -> void` | Assert first < second |
| `assertGreaterEq` | `(T, T) -> void` | Assert first >= second |
| `assertLessEq` | `(T, T) -> void` | Assert first <= second |
| `assertStringContains` | `(string, string) -> void` | Assert string contains substring |
| `assertNear` | `(float64, float64, float64) -> void` | Assert floats within tolerance |

### Utilities

| Function | Signature | Description |
|----------|-----------|-------------|
| `fail` | `(string) -> void` | Fail with message |
| `skip` | `(string) -> void` | Skip test with reason |
| `log` | `(string) -> void` | Log message during test |

## Example

```
use std::test::{ assertEqual, assertTrue, assertNear, log };
use std::math::{ sqrt, PI };

// Basic assertions
assertEqual(2 + 2, 4);
assertTrue(10 > 5);

// Float comparison with tolerance
assertNear(sqrt(2.0), 1.41421356, 0.0001);
assertNear(PI, 3.14159265, 0.0001);

// Logging
log("Test passed successfully");
```

## Running Tests

The `#[test]` attribute turns a function into a test case:

```lucis
use std::test::{ assertEqual, assertTrue };

#[test]
fn testMath() int32 {
    assertEqual(2 + 2, 4);
    ret 0;
}

#[test("custom display name", skip("WIP"))]
fn testPending() void {
}
```

```bash
./build/lucis test examples/my_test.lc
```

Tests that call assertion functions will report pass/fail status automatically.
Failing assertions, crashes and timeouts are isolated per test (each test runs
in a child process), so the runner always continues. Exit code: `0` when all
tests pass (skips don't fail), `1` when any test fails.

See the [`#[test]` attribute](../language/attributes.md) reference and
[CLI usage](../getting-started/cli-usage.md) for filters, `--list`,
`--timeout`, `-j` and `-v`.

## See Also

- [CLI Usage](../getting-started/cli-usage.md) — Running tests
