#!/bin/bash
# Script de déploiement AstrAlim Driver sur RPi5/StellarMate
# Usage: ./deploy.sh <user@host>

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <user@host>"
    echo "Example: $0 stellarmate@stellarmate.local"
    exit 1
fi

TARGET=$1
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REMOTE_DIR="/tmp/astralim-build"

echo "=== Deploying AstrAlim Driver to $TARGET ==="

# Copy sources to RPi
echo "Copying sources..."
ssh -p 5624 $TARGET "sudo rm -rf $REMOTE_DIR && mkdir -p $REMOTE_DIR"
scp -P 5624 -r "$SCRIPT_DIR"/* $TARGET:$REMOTE_DIR/

# Build on RPi
echo "Building on target..."
ssh -p 5624 $TARGET << 'EOF'
cd /tmp/astralim-build
mkdir -p build && cd build
cmake ..
make -j$(nproc)
sudo make install
echo "=== Build complete ==="
EOF

echo "=== Restarting INDI server ==="
ssh -p 5624 $TARGET "sudo systemctl restart indiserver 2>/dev/null || sudo systemctl restart indi-web 2>/dev/null || echo 'Note: Restart INDI manually if needed'"

echo "=== Deployment successful ==="

