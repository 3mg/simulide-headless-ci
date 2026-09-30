# Legacy branch: SimulIDE 1.1.0-SR2 fork

This branch is an archival snapshot of the first version of this fork,
based on the SimulIDE 1.1.0-SR2 release rather than the current
`Arcachofo/SimulIDE-dev` `master` branch used by this repo's `master`.

It is vendored as a plain source snapshot (no upstream git history —
the original 1.1.0-SR2 checkout wasn't a git clone, just extracted
sources), and is kept for reference only. All active development,
including this exact USI clock-stretch fix re-derived against the
current upstream `twimodule.cpp`, has moved to `master`. See that
branch's `CHANGES.md` and `USI_FIX_LOG.md` (also present here,
unchanged) for the full story of how the fix was found.

Do not build CI on this branch — it predates the `-nogui-ci`/`-test-ci`
flag naming, uses `--headless` instead, and its regression suite tests
against 1.1.0-SR2's now-superseded component-loading model.
