#!/usr/bin/env bash
set -euo pipefail

# Detect unresolved git conflict markers in tracked source/docs files.
# We intentionally exclude docs that demonstrate the markers as plain text.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

PATTERN='<<<<<<<|=======|>>>>>>>'
EXCLUDE_REGEX='^(docs/CONFLICT_PLAYBOOK\.md|docs/CODE_AUDIT\.md)$'

HITS="$(rg -n "$PATTERN" . \
  --glob '!*build*/**' \
  --glob '!.git/**' \
  --glob '!docs/CONFLICT_PLAYBOOK.md' \
  --glob '!docs/CODE_AUDIT.md' \
  --glob '!scripts/check_conflict_markers.sh' || true)"

if [[ -n "${HITS}" ]]; then
  echo "ERROR: unresolved conflict markers found:"
  echo "${HITS}"
  exit 1
fi

echo "OK: no unresolved conflict markers detected."
