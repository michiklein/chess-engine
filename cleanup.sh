#!/bin/bash

# Cleanup script to remove local build/distribution artifacts.
# Run locally: chmod +x cleanup.sh && ./cleanup.sh

set -euo pipefail

rm -rf build/
rm -rf dist/
find . -name '__pycache__' -type d -not -path './.git/*' -exec rm -rf {} +

echo "Cleanup complete."
