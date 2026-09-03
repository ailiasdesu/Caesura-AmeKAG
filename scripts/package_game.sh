#!/usr/bin/env bash
# package_game.sh — thin wrapper (t179): the implementation now lives in the
# platform-independent Node CLI scripts/package_game.mjs (no Git Bash needed
# at the product path; Node is already implicit in web packaging). This shim
# stays for dev-script convenience only. See package_game.mjs for the CLI,
# options and full behavior contract.
exec node "$(dirname "$0")/package_game.mjs" "$@"
