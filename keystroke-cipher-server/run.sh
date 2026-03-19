#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Run as: (1) Sender  (2) Receiver"
read -p "Choice: " choice

# Build and load kernel module
cd "$SCRIPT_DIR/kernel"
make -s
sudo insmod keycipher_mod.ko 2>/dev/null || true

# Get major number
MAJOR=$(cat /proc/devices | grep keycipher | awk '{print $1}')
if [ -z "$MAJOR" ]; then
    echo "ERROR: keycipher not found in /proc/devices"
    exit 1
fi
echo "Major number: $MAJOR"

# Create device nodes
sudo rm -f /dev/keycipher_out /dev/keycipher_in
sudo mknod /dev/keycipher_out c "$MAJOR" 0
sudo mknod /dev/keycipher_in  c "$MAJOR" 1
sudo chmod 666 /dev/keycipher_out /dev/keycipher_in

# Build userspace
cd "$SCRIPT_DIR/userspace"
make -s

cd "$SCRIPT_DIR/userspace/network"

if [ "$choice" = "1" ]; then
    echo "Starting sender daemon..."
    sudo "$SCRIPT_DIR/userspace/keycipher_daemon"
elif [ "$choice" = "2" ]; then
    rm -f /tmp/keycipher_enc_queue
    echo "Starting receiver daemon (Terminal 1)..."
    echo "Open a second terminal and run: cd $SCRIPT_DIR/userspace/network && sudo $SCRIPT_DIR/userspace/inbox_terminal"
    sudo "$SCRIPT_DIR/userspace/keycipher_daemon"
else
    echo "Invalid choice"
    exit 1
fi
