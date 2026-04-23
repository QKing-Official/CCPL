# CCPL (C Compiled Programming language)

CCPL is a language designed to be as easy as bash and to have the power of C.
CCPL is a transpiled language. The language is a parser to C and it compiles the C code after that to a binary.
Currently this works on linux only, but I'm planning to add cross-compilation.

This language has it's own package manage and library manager called Barite.
The compiler can also call Barite automatically with the -a flag.

We have also included CCPL GUI. It is a gui for running anc compiling the programs. Works on linux using gtk3.


## REPL

I have made a REPL for the transpiled language, It works by slowly building the program and executing it line by line.
Use it with `ccpl-bin --repl`
That will open the REPL session that clears on exit.

### build the ./ccpl-bin aka the compiler itself
gcc ccpl/main.c ccpl/lexer.c ccpl/parser.c -o ccpl-bin

### compile a program:
./ccpl-bin program.ccpl

-a for auto install dependecies from barite and -o name to set output file.
also supports -r / --repl for interactive mode.
use the -k or --keep-c flags to keep the C files generated from the .ccpl program.

Start interactive REPL:
./ccpl-bin --repl

After it you can run the program using ./out or ./programname

### Build Barite, the package manager
gcc barite/barite.c -o barite-cli

add the -a flag to auto install dependecies when compiling your program (cloud first, local fallback).

### Install barite packages

Normal installation, defaults to cloud
./barite-cli install math
./barite-cli install io

Cloud installation, defaults to cloud
./barite-cli install cloud math
./barite-cli install cloud io

Local package install (for development)
./barite-cli install local math
./barite-cli install local io

### Gui deps
sudo apt install libgtk-3-dev

### Compile gui
cd CCPLGUI
gcc ccpl_gui.c icon.h -o ccpl_gui \
`pkg-config --cflags --libs gtk+-3.0`

## Making packages

Check out the custom package located in local-packages folder.
Also please refer to the guide below

Custom package runtime logic
- Put implementation code in: `local-packages/<pkg>/src/runtime.c`
- Install it with barite so it is copied to: `std/<pkg>/src/runtime.c`
- Namespaced CCPL calls map to C symbols with this convention:
	- `mypkg.doThing(a, b)` -> `ccpl_mypkg_doThing(a, b)`
- The compiler auto-injects every installed package runtime file from the `packages` block.
- This lets users ship custom libs without changing compiler source.
