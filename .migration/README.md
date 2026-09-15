# `.migration/` — evidence for the Android Embedder API migration

Everything an agent produces that is not source code lives here, **committed to
the branch it describes**. This directory is the audit trail. It is the thing
the previous migration attempt claimed to have and did not: its ledger cited
`adversarial_review_phase_*.md` files that exist in no branch.

Specification: [`MIGRATION_LEDGER.md`](../docs/platforms/android/embedder-api-migration/MIGRATION_LEDGER.md)
§A.5 (the evidence rule), §A.6 (artifact format and stack staleness), §D.6
(this layout), and §E (decision records).

## Layout

```
.migration/
  README.md                     # this file
  HUMAN_REVIEW_REQUIRED.md      # gates no agent can satisfy (§D.9)
  plans/
    T-0.3.context.md            # Planner phase 0 — the context pack (§F.2)
    T-0.3.plan.md               # the plan
    T-0.3.plan-review-1.md      # adversarial rounds, one file each
    T-0.3.plan-review-2.md
  reviews/
    T-0.3.code-review-1.md
  decisions/
    DR-0007.md                  # ADR-style, template in §E.1
  verification/
    T-0.3.json                  # the SHA-stamped artifact
    T-0.3/                      # raw logs backing it
      native.log
      robolectric.log
      composition_matrix_results.json
      device_preconditions.txt
      devicelab/*.json
```

## The verification artifact

Written by `dev/tools/bin/migration_verify.dart`. Never by hand.

```jsonc
{
  "task_id": "T-0.3",
  "branch": "android-embedder-v8/t-0.3-threading-characterization",
  "commit_sha": "<git rev-parse HEAD>",     // the tree this actually ran against
  "parent_sha": "<git rev-parse HEAD~1>",   // for the staleness audit
  "timestamp": "2026-09-15T02:19:05Z",
  "host": { "os": "...", "device": "<adb get-serialno>", "android": "16" },
  "gates": {
    "native_unittests":   { "ran": true,  "passed": 41,  "failed": 0, "log": "native.log" },
    "robolectric":        { "ran": true,  "passed": 120, "failed": 0, "log": "robolectric.log" },
    "composition_matrix": { "ran": true,  "results": "composition_matrix_results.json" },
    "devicelab":          { "ran": false, "reason": "not a stage boundary" },
    "ratchet":            { "ran": true,  "before": 37, "after": 37 }
  }
}
```

Three properties make this falsifiable in a way prose is not:

1. **It is in git history**, on the branch it describes. It cannot be
   backdated without an obvious amend.
2. **It embeds the SHA it ran against.** A mismatch with the branch tip is
   mechanically detectable — `--audit-stack` finds it.
3. **`"ran": false` is a legitimate outcome.** There is no incentive to invent a
   pass, because declining to run is a recorded, acceptable result. An honest
   unticked box blocks a stage exit exactly as a failure does.

## Rules worth repeating here

- **The device serial is captured programmatically**, from `adb get-serialno`,
  never typed. §A.7.
- **Goldens are compared against the T-0.12 baseline and never regenerated.** A
  local engine build may not update a golden under any circumstance. §A.8.
- **Rebasing or amending a branch invalidates every descendant's artifact.**
  Re-run them or mark them stale; do not carry them forward silently. §A.6.
- **Nothing in here is generated from memory.** If you are about to paste a SHA
  into a markdown table by hand, stop. §A.2.

## Checking the stack

```bash
dart dev/tools/bin/migration_verify.dart --audit-stack
```

Reports, per branch: artifact present, `commit_sha` matching the tip,
`parent_sha` matching the actual parent, and any descendant invalidated by a
rebase. Run it at every stage boundary and before the final branch.
