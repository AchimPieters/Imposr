# PR Conflict Playbook (GitHub UI loop fix)

If GitHub keeps looping when you click **Mark as resolved**, stop using the web editor for that PR and resolve conflicts locally via command line.

## Symptoms

- You edit a conflicted file in the web UI.
- The **Mark as resolved** button does not complete successfully.
- You keep returning to the same conflict state.

## Reliable fix (command line)

Assume:

- your feature branch = `your-branch`
- base branch = `main`

```bash
git fetch origin
git checkout your-branch
git pull --ff-only
git merge origin/main
```

At this point, Git will stop on conflicts.

> **Permanent recommendation:** use this command-line flow for this repository instead of the GitHub web conflict editor.

Shortcut script:

```bash
./scripts/resolve_pr_conflicts_local.sh <feature-branch> [base-branch]
```

### 1) Inspect conflict status

```bash
git status
```

### 2) Resolve each conflicted file

Open each file listed by `git status`, remove all conflict markers:

- `<<<<<<<`
- `=======`
- `>>>>>>>`

Keep only the final intended content.

### 3) Verify no markers remain

```bash
rg -n "<<<<<<<|=======|>>>>>>>" .github CMakeLists.txt README.md docs src tests include
```

This command should return **no lines**.

### 4) Stage + complete merge commit

```bash
git add .github/workflows/planner-ci.yml CMakeLists.txt README.md docs/CODE_AUDIT.md src/CliMain.cpp src/ImposePlugin.cpp tests/ImpositionPlanTests.cpp
git commit
```

### 5) Push branch and refresh PR

```bash
git push origin your-branch
```

GitHub will re-run checks with the merged result and the conflict banner should disappear.

## Make it “stick” (so future PRs are easier)

Enable git rerere once on your machine:

```bash
git config --global rerere.enabled true
```

Then Git will remember repeated conflict resolutions and auto-apply them next time.

## Notes for this repository

- Prefer the command-line merge flow for large Markdown/CMake/CI conflicts.
- Keep compiler-warning policy from `CMakeLists.txt` (`AIMP_ENABLE_STRICT_WARNINGS`) and avoid reintroducing hardcoded GNU flags into CI.
- For tests, keep cross-platform temp paths (no hardcoded `/tmp/...`).

## Troubleshooting

### `No such file or directory` for `scripts/resolve_pr_conflicts_local.sh` or `scripts/check_conflict_markers.sh`

This means your local branch does not yet contain those script commits.

1. Verify script presence:

```bash
ls scripts
```

2. If files are missing, update your branch from remote:

```bash
git fetch origin
git checkout your-branch
git pull --ff-only origin your-branch
```

3. If still missing, resolve conflicts manually without scripts:

```bash
git fetch origin
git merge origin/main
# resolve conflicts in editor
rg -n "<<<<<<<|=======|>>>>>>>" .github CMakeLists.txt README.md docs src tests include
git add <resolved-files>
git commit
git push origin your-branch
```

4. If your PR branch simply does not include these helper commits yet, cherry-pick them (or rebase onto the latest PR branch state) before retrying.
