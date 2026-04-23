build the ./ccpl-bin aka the compiler itself
gcc ccpl/main.c ccpl/lexer.c ccpl/parser.c -o ccpl-bin

run a program:
./ccpl-bin program.ccpl

Start interactive REPL:
./ccpl-bin --repl

Build Barite, the package manager
gcc barite/barite.c -o barite-cli

Install barite packages
./barite-cli install local math
./barite-cli install local io

-a for auto install and -o name to set output file
also supports -r / --repl for interactive mode

for the gui
sudo apt install libgtk-3-dev

Compile gui
gcc ccpl_gui.c icon.h -o ccpl_gui \
`pkg-config --cflags --libs gtk+-3.0`