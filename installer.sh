#!/usr/bin/env bash

set -e

CCPL_URL="https://github.com/QKing-Official/CCPL/releases/download/0.1/ccpl"
BARITE_URL="https://github.com/QKing-Official/CCPL/releases/download/0.1/barite-cli"
REPO_ZIP="https://github.com/QKing-Official/CCPL/archive/refs/heads/main.zip"

INSTALL_DIR="/opt/ccpl"
BIN_DIR="/usr/local/bin"

if ! command -v dialog &> /dev/null; then
    echo "Installing dialog..."
    sudo apt update
    sudo apt install -y dialog
fi

if ! command -v unzip &> /dev/null; then
    echo "Installing unzip..."
    sudo apt install -y unzip
fi

TEMPFILE=$(mktemp)

dialog --title "CCPL Installer" \
--checklist "Select components to install:" 15 60 6 \
"ccpl" "CCPL Compiler" ON \
"barite" "Barite Package Manager" ON \
"offline" "Local Packages (offline install)" OFF 2> "$TEMPFILE"

clear

choices=$(cat "$TEMPFILE")
rm "$TEMPFILE"

INSTALL_CCPL=false
INSTALL_BARITE=false
INSTALL_OFFLINE=false

[[ $choices == *"ccpl"* ]] && INSTALL_CCPL=true
[[ $choices == *"barite"* ]] && INSTALL_BARITE=true
[[ $choices == *"offline/local packages"* ]] && INSTALL_OFFLINE=true

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

if $INSTALL_OFFLINE; then
    echo "Downloading local packages..."
    TMP_ZIP=$(mktemp)

    curl -L "$REPO_ZIP" -o "$TMP_ZIP"

    TMP_DIR=$(mktemp -d)
    unzip -q "$TMP_ZIP" -d "$TMP_DIR"

    sudo mkdir -p "$INSTALL_DIR/local-packages"
    sudo mkdir -p /opt/ccpl/std && sudo chown -R $USER:$USER /opt/ccpl
    sudo cp -r "$TMP_DIR"/CCPL-main/local-packages/* "$INSTALL_DIR/local-packages/"

    rm -rf "$TMP_ZIP" "$TMP_DIR"

    echo "Local packages installed to $INSTALL_DIR/local-packages"
fi

dialog --title "Done" \
--msgbox "CCPL installation complete!\n\nRun:\nccpl\nbarite-cli" 10 40

clear
tput reset