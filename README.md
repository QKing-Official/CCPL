# CCPL — C Compiled Programming Language

CCPL is a high-level scripting language that compiles down to C. It's designed to feel as simple as Bash while giving you the raw power of C under the hood. Write clean, readable code — CCPL transpiles it to C and compiles it to a native binary automatically.

> **Platform:** Linux (cross-compilation planned)

---

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Compiler Usage](#compiler-usage)
- [REPL](#repl)
- [Barite Package Manager](#barite-package-manager)
- [Packages](#packages)
- [GUI](#gui)
- [Making Your Own Packages](#making-your-own-packages)
- [Building from Source](#building-from-source)

---

## Installation

### One-line installer (recommended)

No need to clone anything. Just run this in your terminal:

```bash
bash <(curl -s https://raw.githubusercontent.com/QKing-Official/CCPL/main/installer.sh)
```

The installer uses a dialog-based UI to let you choose components:

- **ccpl** — the compiler
- **barite** — the package manager
- **offline** — local package cache (installs packages for use without internet)

Binaries are installed to `/opt/ccpl/` and symlinked into `/usr/local/bin/`, so `ccpl` and `barite-cli` are available everywhere on your system after install.

---

## Quick Start

Create a file called `hello.ccpl`:

```
packages = {
    io
}

print "Hello, world!"
```

Compile and run:

```bash
ccpl hello.ccpl
./out
```

---

## Compiler Usage

```
ccpl [options] <file.ccpl>
```

| Flag | Description |
|------|-------------|
| `-o <name>` | Set output binary name (default: `out`) |
| `-a`, `--auto` | Auto-install missing packages via Barite |
| `-r`, `--repl` | Start interactive REPL mode |
| `-k`, `--keep-c` | Keep the generated `.c` file |
| `--quiet` | Suppress success output |

**Examples:**

```bash
ccpl program.ccpl                  # compile to ./out
ccpl program.ccpl -o myapp         # compile to ./myapp
ccpl program.ccpl -a               # auto-install missing packages
ccpl program.ccpl -o myapp -k      # keep the generated C file
```

---

## REPL

CCPL includes an interactive REPL that builds up your program line by line and executes it incrementally.

```bash
ccpl --repl
```

**REPL commands:**

| Command | Description |
|---------|-------------|
| `:help` | Show available commands |
| `:clear` | Reset REPL state |
| `:quit` / `exit` | Exit the REPL |

The REPL distinguishes between persistent statements (variable assignments, function definitions) and transient ones (`print`, shell captures) — only transient lines are re-evaluated on each run rather than accumulated.

---

## Barite Package Manager

Barite is the package manager for CCPL. It fetches and installs standard library packages so the compiler can find them.

### Cloud vs Local packages

**Cloud packages** are downloaded from the [BariteStd repository](https://github.com/QKing-Official/BariteStd) over the internet. This is the default and gives you the latest published versions of all official packages.

**Local packages** are installed from the `local-packages/` folder bundled with CCPL. They work completely offline and are useful when you have no internet access, are developing your own packages, or selected the **offline** option during installation. The local package cache lives at `/opt/ccpl/local-packages/` after a global install.

### Which command to use

| Situation | Command |
|-----------|---------|
| Globally installed via the one-line installer | `barite-cli install math` |
| Cloned the repo and running locally | `./barite-cli install math` |
| Globally installed, no internet | `barite-cli install local math` |
| Cloned the repo, no internet | `./barite-cli install local math` |

### Installing Packages

```bash
# Cloud install (default) — downloads from the internet
barite-cli install math
barite-cli install io shell

# Explicitly cloud
barite-cli install cloud math

# Local install — uses the bundled offline package cache
barite-cli install local math
barite-cli install local io shell
```

### Managing Packages

```bash
barite-cli list                    # list installed packages
barite-cli remove math             # remove a package
barite-cli info installed math     # show info for an installed package
barite-cli info local math         # show info for a local/bundled package
```

### Auto-install via Compiler

Pass `-a` to the compiler to have it call Barite automatically when a required package is missing. It will install the required packages for you. It tries cloud first, then falls back to local packages:

```bash
ccpl program.ccpl -a
```

---

## Packages

Packages are declared at the top of your `.ccpl` file in a `packages` block:

```
packages = {
    io
    math
    shell
}
```

The current packages that exist are listed below. Some libraries may not be included in the local version. Check the BariteStd repo for that: https://github.com/QKing-Official/BariteStd. It is the repository where the cloud install fetches it's packages from.


| Package | Description |
|---------|-------------|
| `io` | `print`, string variables, string concatenation, `len()` |
| `math` | Arithmetic, variables, `for`/`while` loops, arrays |
| `shell` | Run shell commands, capture output with `$()` |
| `str` | String utilities: `trim`, `upper`, `lower`, `contains`, `index_of`, etc. |
| `rand` | Random numbers: `rand.int()`, `rand.float()`, `rand.seed()` |
| `dt` | Date/time: `dt.now_iso()`, `dt.format_unix()` |
| `crypto` | Cryptographic utilities |

Package function calls use dot notation and map directly to C symbols:

```
math.add(a, b)    →    ccpl_math_add(a, b)
io.print(x)       →    ccpl_io_print_int(x)  (type-dispatched)
shell.capture(cmd) →   ccpl_shell(cmd)
```

---

## Making Your Own Packages

Custom packages follow a simple convention that lets you ship libraries without modifying the compiler.

### Directory Structure

```
local-packages/
└── mypkg/
    ├── package.barite       # package metadata
    └── src/
        └── runtime.c        # implementation
```

### package.barite format

```
name: mypkg
version: 1.0
description: My custom package
```

### runtime.c conventions

All functions must be namespaced as `ccpl_<pkgname>_<funcname>`:

```c
// mypkg.doThing(a, b)  →  ccpl_mypkg_doThing(a, b)
double ccpl_mypkg_doThing(double a, double b) {
    return a + b;
}
```

The compiler automatically injects every installed package's `runtime.c` into the generated C file when that package appears in the `packages` block.

### Install and use your package

```bash
barite-cli install local mypkg
```

```
packages = {
    mypkg
}

x = mypkg.doThing(3, 4)
print x
```

---

## Building from Source

If you'd rather build manually from a clone of the repo instead of using the one-line installer:

```bash
git clone https://github.com/QKing-Official/CCPL.git
cd CCPL
```

### Build the compiler

```bash
gcc ccpl/main.c ccpl/lexer.c ccpl/parser.c -o ccpl-bin
```

### Build Barite

```bash
gcc barite/barite.c -o barite-cli
```

### Run without installing globally

When running from the cloned repo directory, prefix commands with `./`:

```bash
./barite-cli install local math io shell
./ccpl-bin program.ccpl
```

### Install globally

To install system-wide so you can use `ccpl` and `barite-cli` from anywhere:

```bash
sudo mkdir -p /opt/ccpl/std
sudo chown -R $USER:$USER /opt/ccpl
sudo mv ccpl-bin /opt/ccpl/ccpl
sudo ln -sf /opt/ccpl/ccpl /usr/local/bin/ccpl
sudo mv barite-cli /usr/local/bin/barite-cli
```

After a global install, drop the `./` prefix — use `ccpl` and `barite-cli` directly:

```bash
barite-cli install math io shell
ccpl program.ccpl
```

---

## License

See [LICENSE](LICENSE) for details.