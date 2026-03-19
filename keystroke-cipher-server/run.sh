#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Kill any existing daemon and free the port
sudo pkill -f keycipher_daemon 2>/dev/null || true
sleep 1

# Reload kernel module cleanly
cd "$SCRIPT_DIR/kernel"
make -s
sudo rmmod keycipher_mod 2>/dev/null || true
sudo insmod keycipher_mod.ko

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

# Generate TLS certs if missing
if [ ! -f "$SCRIPT_DIR/userspace/cert.pem" ] || [ ! -f "$SCRIPT_DIR/userspace/key.pem" ]; then
    echo "Generating TLS certificates..."
    openssl req -x509 -newkey rsa:2048 -keyout "$SCRIPT_DIR/userspace/key.pem" \
        -out "$SCRIPT_DIR/userspace/cert.pem" -days 365 -nodes -subj "/CN=keycipher" 2>/dev/null
fi

# Start daemon
echo "Starting daemon..."
"$SCRIPT_DIR/userspace/keycipher_daemon" &

echo "[✓] KeyCipher daemon running."
echo "[*] To start the frontend: cd web && node server.js"
