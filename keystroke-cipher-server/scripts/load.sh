#!/bin/bash
# load.sh - insert the kernel module and set up device files

set -e

echo "[*] Building kernel module..."
make -C ../kernel

echo "[*] Loading module..."
sudo insmod ../kernel/keycipher.ko

echo "[*] Creating device files..."
MAJOR=$(grep keycipher /proc/devices | awk '{print $1}')
sudo mknod /dev/keycipher_out c "$MAJOR" 0
sudo mknod /dev/keycipher_in  c "$MAJOR" 1

echo "[*] Setting permissions..."
sudo chmod 666 /dev/keycipher_in
sudo chmod 666 /dev/keycipher_out

echo "[*] Generating TLS certificates if missing..."
if [ ! -f ../userspace/cert.pem ] || [ ! -f ../userspace/key.pem ]; then
    openssl req -x509 -newkey rsa:2048 -keyout ../userspace/key.pem -out ../userspace/cert.pem \
        -days 365 -nodes -subj "/CN=keycipher" 2>/dev/null
    echo "[✓] Certificates generated."
else
    echo "[✓] Certificates already exist."
fi

echo "[*] Starting userspace daemon..."
../userspace/keycipher_daemon &

echo "[✓] KeyCipher loaded. Check dmesg for kernel logs."
echo "[✓] Stats: cat /proc/keycipher/stats"
