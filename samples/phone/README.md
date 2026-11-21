# samples/phone

Not built in this repo; read as reference. Used by docs/10 and 11. Some files mention paths of the app repos they came from (in comments); the files are as on disk.

| File | From | Kind | Shows |
|---|---|---|---|
| push.sh | Tengram, `scripts/push.sh` (5a65cdf) | verbatim | Push a test binary to `/root/<app>`, trust it with `pathtrust`, run it |
| push-engine.sh | HyperBerry, `scripts/push-engine.sh` (df45afc) | verbatim | The same for an engine binary, with a fresh name per build |
| install-phone.sh | Phoenix, `scripts/install-phone.sh` (4e06969) | verbatim | Install a `.bar` over root SSH with the PPS installer job |
| install-root-pps.md | Tengram, `docs/install-root-pps.md` (5a65cdf) | verbatim | The root PPS install route, step by step |
| engine_client.py | Tengram, `tests/engine_client.py` (5a65cdf) | verbatim | Protocol client for a headless engine through an SSH tunnel |
| measure-phone.py | bbmusic, `scripts/measure-phone.py` (1d80813) | verbatim | CPU and memory snapshots over SSH (`pidin`); app-specific driver kept as a pattern |
| sim-driver.py | HyperBerry, `scripts/sim.py` (df45afc) | verbatim | Drives the app UI on the phone from the Mac; app-specific driver kept as a pattern |
