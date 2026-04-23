# shell package

This package provides shell command helpers through the `shell.*` namespace.

Actual implementation logic is in `src/runtime.c` and is copied to `std/shell/src/runtime.c` on install.

## Functions

- `shell.capture(command)`
- `shell.exec(command)`

## Example

```ccpl
packages = { shell, io }
out = shell.capture("pwd")
io.print(out)
shell.exec("echo done")
```
