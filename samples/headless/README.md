# samples/headless

Not built in this repo; read as reference. Used by docs/09-headless-service.md.

| File | From | Kind | Shows |
|---|---|---|---|
| MemoryPatch.h, MemoryPatch.cpp | HyperBerry, `app/src/MemoryPatch.*` (df45afc) | verbatim | The UI's side of the memory patch: starts the root installer helper with a minimal environment, `FD_CLOEXEC` on every descriptor, result file |
| install-patcher.c | HyperBerry, `app/helper/hb-install-patcher.c` (df45afc) | verbatim | The root helper: spawns its arguments detached so the system does not end them when the app exits |
| DataSettingsPage.qml | hyperberry `df45afc` app/assets | verbatim | Settings page saved as values change |
