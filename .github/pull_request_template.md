## Summary

- What changed?
- Why was this needed?
- Which user-visible behavior is affected?

## Scope

- [ ] Planner core (`include/aimp`, `src/ImpositionPlan.cpp`)
- [ ] CLI (`src/CliMain.cpp`)
- [ ] PDF/report output (`src/PdfComposer.cpp`)
- [ ] Presets (`src/Preset.cpp`)
- [ ] Artifact bundles (`src/ArtifactBundle.cpp`)
- [ ] Acrobat plug-in integration (`src/ImposePlugin.cpp`)
- [ ] Build/CI (`CMakeLists.txt`, `.github/workflows/*`)
- [ ] Docs (`README.md`, `docs/*`)

## Conflict Resolution Check

- [ ] No merge conflict markers remain (`<<<<<<<`, `=======`, `>>>>>>>`)
- [ ] If conflicts occurred, resolved locally using `docs/CONFLICT_PLAYBOOK.md`

## Testing

List the exact commands and outcomes:

```bash
cmake -S . -B build -DAIMP_BUILD_PLUGIN=OFF -DAIMP_BUILD_TESTS=ON -DAIMP_BUILD_CLI=ON -DAIMP_ENABLE_STRICT_WARNINGS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Add extra runtime checks if relevant (CLI outputs, artifact bundle generation, etc.).

## Risks / Follow-ups

- Any known limitations?
- Any deferred work required for full product-MVP?

