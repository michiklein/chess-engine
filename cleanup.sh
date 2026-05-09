#!/bin/bash

# Cleanup script to remove build artifacts and archived files from the repo.
# Run locally: chmod +x cleanup.sh && ./cleanup.sh

set -euo pipefail

# Remove build artifacts
rm -rf build/
rm -rf build-release/

# Remove old artifacts (safe - ignore errors)
git rm -f --ignore-unmatch tournament_results.txt || true
git rm -f --ignore-unmatch match.py || true

echo "Cleanup complete. If files were tracked, remember to commit the removals:"
echo "  git add -A && git commit -m 'Cleanup: remove binaries and old artifacts'"
