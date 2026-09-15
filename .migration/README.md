# Verification Artifacts and Hand-off Directory

This directory contains the audit trail for the Android Embedder API Migration.

## Verification Artifact Schema (`.migration/verification/*.json`)

Every verified task commits `.migration/verification/<task-id>.json` plus raw logs to its own branch.
These artifacts ensure provenance over prevention.

Three properties make this falsifiable:
1. **It is in git history**, timestamped, on the branch it describes.
2. **It embeds the SHA it ran against.** A mismatch between `commit_sha` and the branch tip is mechanically detectable, preventing fake evidence across rebases.
3. **`"ran": false` is a first-class, expected value.** There is no incentive to fake a run, because declining to run is a legitimate recorded outcome.

## Rules
- **No Golden Changes**: A locally-built engine may never update a golden file, not with `--update-goldens` or by hand.
- **Serial-capture rule**: The serial goes in the artifact `host.device` via `adb get-serialno` natively.
- **Branch staleness**: A verification is valid only if its `parent_sha` matches the actual parent commit. Rebasing or amending invalidates descendant artifacts.
