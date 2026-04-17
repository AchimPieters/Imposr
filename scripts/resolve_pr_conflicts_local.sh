#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <feature-branch> [base-branch]"
  echo "Example: $0 achimpieters/create-c++-plug-in-plan-for-acrobat-g6fstf main"
  exit 1
fi

FEATURE_BRANCH="$1"
BASE_BRANCH="${2:-main}"

echo "==> Fetching latest refs"
git fetch origin

echo "==> Switching to feature branch: ${FEATURE_BRANCH}"
git checkout "${FEATURE_BRANCH}"

echo "==> Fast-forward pull on feature branch"
git pull --ff-only origin "${FEATURE_BRANCH}"

echo "==> Merging base branch: origin/${BASE_BRANCH}"
set +e
git merge "origin/${BASE_BRANCH}"
MERGE_EXIT=$?
set -e

if [[ $MERGE_EXIT -ne 0 ]]; then
  echo
  echo "Merge has conflicts. Resolve files shown by:"
  echo "  git status"
  echo
  echo "After editing conflicts, run:"
  echo "  ./scripts/check_conflict_markers.sh"
  echo "  git add <resolved-files>"
  echo "  git commit"
  echo "  git push origin ${FEATURE_BRANCH}"
  exit 0
fi

echo "==> Merge completed without conflicts"
./scripts/check_conflict_markers.sh
git push origin "${FEATURE_BRANCH}"
echo "Done. Refresh the PR page."

