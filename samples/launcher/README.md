# samples/launcher

These samples are not built in this repo: they are reference code copied from the app repos.

| File | From | Kind | Shows |
|---|---|---|---|
| tengram.pro | tengram app/tengram.pro @ dbe35bf | verbatim | The UI as a shared object for the Cascades launcher: `TEMPLATE = lib`, `plugin`, `-Bsymbolic` |
| bar-descriptor.xml | tengram app/bar-descriptor.xml @ dbe35bf | verbatim | The `.so` as the entry asset, the UI's entry point as `Qnx/Cascades`, the engine still `Qnx/Elf` |
| main.cpp | tengram app/src/main.cpp @ dbe35bf | excerpt | Exported `main`, the start-up timing, the translations found under the working directory |

Doc: [../../docs/14-cascades-launcher.md](../../docs/14-cascades-launcher.md).
