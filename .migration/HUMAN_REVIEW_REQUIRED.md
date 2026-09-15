# Human review required

Gates the autonomous agent pipeline **structurally cannot satisfy**, recorded
here instead of being quietly ticked or quietly dropped.

Governed by
[`MIGRATION_LEDGER.md`](../docs/platforms/android/embedder-api-migration/MIGRATION_LEDGER.md)
§D.9. Every entry below has already had its *mechanical* half proven by test —
that is mandatory, not optional. What remains is the judgement a person has to
supply.

> [!IMPORTANT]
> **Whoever opens the pull request for this stack must read this file first.**
> Every entry is an unticked box in the ledger. None of them is a pass.

## How to add an entry

Append, never reorder. Use this shape:

```markdown
### <task-id> — <gate>
- **Role required:** iOS owner | macOS owner | Windows owner | release owner | …
- **Question they must answer:** <one sentence, answerable>
- **Where to look:** <files, tests, API surface>
- **Mechanical evidence already gathered:** <test names + result + artifact path>
- **What the pipeline assumed in the meantime:** <the presumption, and the DR that records it>
- **Blast radius if the assumption is wrong:** <concrete>
```

## Entries

*(None yet.)*
