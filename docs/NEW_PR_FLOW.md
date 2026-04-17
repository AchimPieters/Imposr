# New PR Flow (recommended)

Use this flow to create a clean new PR and avoid repeated conflict loops.

## 1) Start from up-to-date `main`

```bash
git fetch origin
git checkout main
git pull --ff-only origin main
```

## 2) Create a fresh feature branch

```bash
git checkout -b <new-branch-name>
```

## 3) Apply your changes

- implement code/docs updates
- run local checks

Recommended baseline:

```bash
cmake -S . -B build -DAIMP_BUILD_PLUGIN=OFF -DAIMP_BUILD_TESTS=ON -DAIMP_BUILD_CLI=ON -DAIMP_ENABLE_STRICT_WARNINGS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./scripts/check_conflict_markers.sh
```

## 4) Commit and push

```bash
git add .
git commit -m "Your change summary"
git push -u origin <new-branch-name>
```

## 5) Open PR against `main`

- use `.github/pull_request_template.md`
- if conflicts appear, resolve locally using `docs/CONFLICT_PLAYBOOK.md` (not web editor)

## 6) Keep PR current

```bash
git fetch origin
git checkout <new-branch-name>
git merge origin/main
./scripts/check_conflict_markers.sh
git push origin <new-branch-name>
```

