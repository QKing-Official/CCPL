# io package

This package provides output and string-length helpers through the `io.*` namespace.

Actual implementation logic is in `src/runtime.c` and is copied to `std/io/src/runtime.c` on install.

## Functions

- `io.print(value)`
- `io.len(text)`

## Example

```ccpl
packages = { io }
msg = "hello"
io.print(msg)
io.print(io.len(msg))
```
