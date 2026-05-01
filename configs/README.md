# GHB sweep configs

Run `./scripts/generate.py` from the project root directory to generate several
different config files that sweep the parameters of a design.

Configs also contain compile time defines. so, the base file:
`../skylake_config_l2ghb.json`.
Could have it's `INDEX_ENTRIES` and `GHB_ENTRIES` swept.

And all configs would be placed in `configs/sweep`

And all binaries can be generated using
```
./scripts/build.sh ./configs/sweep/*
```
