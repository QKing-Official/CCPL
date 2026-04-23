# math package

This package provides math helpers through the `math.*` namespace.

Actual implementation logic is in `src/runtime.c` and is copied to `std/math/src/runtime.c` on install.

For custom packages, expose functions with symbols named like `ccpl_<package>_<member>`.

## Functions

- `math.add(a, b)`
- `math.sub(a, b)`
- `math.mul(a, b)`
- `math.div(a, b)`
- `math.mod(a, b)`
- `math.pow(a, b)`

## Example

```ccpl
packages = { math, io }
value = math.add(10, 20)
io.print(value)
```
