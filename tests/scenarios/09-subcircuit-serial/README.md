# 09-subcircuit-serial

This scenario is a vendored copy of `hardware-tests/test3-attiny85-encoder-i2c-slave`.

It intentionally keeps local copies of:

- `test.sim1`
- source projects under `sources/`
- the currently known-good `firmware/*.hex`

That makes the regression test stable even if `hardware-tests/` evolves independently.

To rebuild the local firmware snapshots from the copied sources, run:

```bash
./rebuild_hex.sh
```
