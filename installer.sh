#!/usr/bin/env bash

set -e

CCPL_URL="https://github.com/QKing-Official/CCPL/releases/download/0.1/ccpl"
BARITE_URL="https://github.com/QKing-Official/CCPL/releases/download/0.1/barite-cli"

INSTALL_DIR="/opt/ccpl"
BIN_DIR="/usr/local/bin"

if ! command -v dialog &> /dev/null; then
    echo "Installing dialog..."
    sudo apt update
    sudo apt install -y dialog
fi

TEMPFILE=$(mktemp)

dialog --title "CCPL Installer" \
--checklist "Select what to install:" 15 50 5 \
"ccpl" "CCPL Compiler" ON \
"barite" "Barite Package Manager" ON 2> "$TEMPFILE"

clear

choices=$(cat "$TEMPFILE")
rm "$TEMPFILE"

INSTALL_CCPL=false
INSTALL_BARITE=false

[[ $choices == *"ccpl"* ]] && INSTALL_CCPL=true
[[ $choices == *"barite"* ]] && INSTALL_BARITE=true

dialog --title "Confirm Installation" \
--yesno "Install to:\n$INSTALL_DIR\n\nCommands in:\n$BIN_DIR\n\nContinue?" 10 50

clear

echo "Creating directories..."
sudo mkdir -p "$INSTALL_DIR"

if $INSTALL_CCPL; then
    echo "Downloading CCPL..."
    sudo curl -L "$CCPL_URL" -o "$INSTALL_DIR/ccpl"
    sudo chmod +x "$INSTALL_DIR/ccpl"
    sudo ln -sf "$INSTALL_DIR/ccpl" "$BIN_DIR/ccpl"
fi

if $INSTALL_BARITE; then
    echo "Downloading Barite..."
    sudo curl -L "$BARITE_URL" -o "$INSTALL_DIR/barite-cli"
    sudo chmod +x "$INSTALL_DIR/barite-cli"
    sudo ln -sf "$INSTALL_DIR/barite-cli" "$BIN_DIR/barite-cli"
fi

dialog --title "Done" \
--msgbox "CCPL installation complete!\n\nRun:\nccpl\nbarite-cli" 10 40

# Proper cleanup
clear
tput reset