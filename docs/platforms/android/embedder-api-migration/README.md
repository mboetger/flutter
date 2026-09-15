# Android Embedder → Embedder API Migration (v8)

Moving `shell/platform/android` off internal engine APIs and onto the public
Embedder API (`embedder.h`), **incrementally**, with every change either
behaviour-preserving or behind a runtime flag.

## Start here

| Document | What it is |
|---|---|
| [`MIGRATION_PLAN.md`](MIGRATION_PLAN.md) | Strategy. The five stages, the twelve invariants, the five blockers, the API gap list, the testing model, and the autonomous decision policy (§10). |
| [`MIGRATION_LEDGER.md`](MIGRATION_LEDGER.md) | Execution. Rules of engagement (§A), the task list (Stages 0–4), the agent pipeline (§D), decision records (§E), just-in-time task expansion (§F), and verified ground truth (§G). |
| [`v7-post-mortem.md`](v7-post-mortem.md) | The previous attempt, analysed. Required reading — it is a completed experiment that already found out what breaks. |

**If you are an agent picking up a task:** read ledger §A, your role contract in
§D.4, §F, §G, and *only* your own task block. §D.3 explains why that restriction
matters.

**If you are a human reviewing the eventual pull request:** start with plan §0
(why v7 failed) and §1.1 (how this one is delivered), then ledger §A.5 (the
evidence rule) — that is what the whole process hangs on.

## How this migration is run

- **A stack of branches on a fork, no pull requests.** One task, one branch. A
  single PR is opened manually from the last branch when the stack is complete.
  Ledger §A.2.
- **Five agents per branch**: Planner → Plan Reviewer → Implementer → Code
  Reviewer → Validator, with adversarial review loops. Ledger §D.
- **Evidence, not assertion.** A ledger box is ticked only when a committed,
  SHA-stamped verification artifact exists whose recorded commit matches the
  branch tip. `"ran": false` is a first-class, expected outcome. Ledger §A.5,
  §A.6.
- **All four platform-view composition modes stay working, continuously.**
  Virtual Display, TLHC, Hybrid Composition, and HC++, plus both external
  texture paths — validated at every stage boundary, not at the end. Plan I-11,
  I-12.

> [!WARNING]
> The previous attempt marked 41 of 42 ledger items complete, certified
> "285/285 tests passed" on branches where the vsync JNI natives were
> unregistered, and cited review documents that do not exist in any branch.
> Nearly every rule in ledger §A exists because of a specific thing that went
> wrong there, and each one names it.

## Related reading

- [Android Platform Views](../Android-Platform-Views.md)
- [Hybrid Composition](../../Hybrid-Composition.md)
- [Texture Layer Hybrid Composition](../Texture-Layer-Hybrid-Composition.md)
- [Virtual Display](../Virtual-Display.md)
