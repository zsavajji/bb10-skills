# engine-link

How the UI talks to the engine. Copied from the app repos; not built in this repo.

| File | From | Kind | Shows |
|---|---|---|---|
| tcp/EngineLink.h, tcp/EngineLink.cpp | bbmusic, `app/src/` (1d80813) | verbatim | Loopback TCP to a headless engine: port and token read from engine.json, invoke START with back-off |
| stdio/EngineLink.h, stdio/EngineLink.cpp | phoenix, `app/src/` (4e06969) | verbatim | The engine as a QProcess child over stdio, with FD_CLOEXEC |
