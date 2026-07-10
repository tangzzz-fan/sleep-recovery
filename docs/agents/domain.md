# Domain Docs

How the engineering skills should consume this repo's domain documentation when exploring the codebase.

## Before exploring, read these

- `CONTEXT.md` at the repo root, if it exists
- `docs/adr/`, if it exists

If these files do not exist yet, proceed silently. They can be created later by the skill flow when terminology or decisions become stable.

## File structure

Single-context repo:

```text
/
├── CONTEXT.md
├── docs/adr/
└── src/
```

## Use the glossary's vocabulary

When naming domain concepts in specs, tickets, tests, or module discussions, prefer the terms defined in `CONTEXT.md` once that file exists.

## Flag ADR conflicts

If a proposed change conflicts with an existing ADR, surface the conflict explicitly instead of silently overriding it.
