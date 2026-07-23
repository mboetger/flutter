---
name: flutter-kotlin-lint
description: 'Lint and format Kotlin files in the flutter/flutter repository using the project-aligned version of ktlint.'
---

Use this skill when you need to run `ktlint` checks on changed Kotlin files, or when the user explicitly asks to "lint Kotlin files", "run ktlint", or "format Kotlin files".

## Workflow

### 1. Run the Lint Check
Execute the helper script located in the skill directory to perform the lint check on changed Kotlin files:

```bash
dart .agents/skills/flutter-kotlin-lint/scripts/ktlint_check.dart
```

This script will automatically:
1. Identify changed Kotlin files in the current branch compared to `upstream/master`.
2. Extract the correct `ktlint` version from `.ci.yaml`.
3. Download/export `ktlint` via CIPD if not already cached.
4. Run `ktlint` on the changed files using the project's editorconfig and baseline.

### 2. Handle Results
*   If the script exits with `0`, the lint check passed. Report this to the user.
*   If the script exits with a non-zero code, it will print the lint violations.
    *   Show these violations to the user.
    *   Offer to automatically format the files to fix the violations.

### 3. Automatically Format Files
If the user approves formatting, or if you need to fix formatting issues, run the script with the `--format` or `-F` flag:

```bash
dart .agents/skills/flutter-kotlin-lint/scripts/ktlint_check.dart --format
```

Verify that the files were modified and that a subsequent run of the check script without flags passes.

