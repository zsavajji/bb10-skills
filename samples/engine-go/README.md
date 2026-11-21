# engine-go

A Go engine for BB10 (go-qnx, cgo through bb10-clang). Copied from hyperberry (df45afc); not built in this repo.

| File | From | Kind | Shows |
|---|---|---|---|
| build-engine.sh, test-engine.sh | `scripts/` | verbatim | Docker build, symbol check, tests |
| patch-deps-work.sh | `scripts/hypermeow-work.sh` | verbatim | Working on patched dependencies |
| go.mod | `engine/go.mod` | verbatim | Module and replace lines |
| roots.go | `engine/roots.go` | verbatim | TLS roots on the phone |
| navigator_qnx.go, navigator_other.go | `engine/` | verbatim | Navigator invoke from Go, with the host stub |
| memlimit.go | `engine/memlimit.go` | verbatim | Memory limit handling |
| engine.go | `engine/` | verbatim | Engine core: start, dispatch |
| hub_other.go | `engine/` | verbatim | Hub stub for off-device tests |
| hub_qnx.go | `engine/` | verbatim | BlackBerry Hub through cgo |
| log.go | `engine/` | verbatim | Log file helper |
| main.go | `engine/` | verbatim | Entry point: loopback TCP server, DNS through libsocket |
| net_other.go | `engine/` | verbatim | Network-type stub for tests |
| net_qnx.go | `engine/` | verbatim | Cellular detection through BPS (cgo) |
| notifier.go | `engine/` | excerpt | A Hub item built from a chat row (`note`, `hubItem`) and handed to the one-thread Notifier that calls the Hub (UDS); account registration and prefs left out |
| ringtone_other.go | `engine/` | verbatim | Ringtone stub for tests |
| ringtone_qnx.go | `engine/` | verbatim | Ringtone through mm-renderer (cgo) |
| server.go | `engine/` | verbatim | Loopback TCP protocol server |
| sim_chats.go | `engine/` | verbatim | Simulated chats for the simulator |
| sim.go | `engine/` | excerpt | Simulated account: link state, `sim.link` writing the history (`seed`) only when it links, request dispatch, one simulated behaviour (an incoming message). The `data/sim` switch itself is in main.go and engine.go |
| translate.go | `engine/` | excerpt | Google's HTTP/1.1-only transport (empty `TLSNextProto`, made fresh) and the request function that uses it |
| patches/framesocket.patch | `engine/patches/` | verbatim | Patch for the frame socket |
| patches/hypermeow-fullsync.patch | `engine/patches/` | verbatim | Patch for hypermeow: full sync |
| patches/hypermeow-status.patch | `engine/patches/` | verbatim | Patch for hypermeow: status |
| patches/meowcaller-group.patch | `engine/patches/` | verbatim | Patch for meowcaller: group calls |
| patches/meowcaller-prst.patch | `engine/patches/` | verbatim | Patch for meowcaller: presence |
| patches/meowcaller.patch | `engine/patches/` | verbatim | Patch for meowcaller |
| db.go | `engine/` | verbatim | SQLite driver import |
| media.go | `engine/` | verbatim | Voice-note media info kept per message |
| store.go | `engine/` | excerpt | Store setup over the patched library, the one writer (ops channel, batched transactions) and one event-to-row handler (`Pin`) |
| session.go | `engine/` | excerpt | The hypermeow client (creation, connect) and the link state sent to the UI; login flows and most event handling left out |
| send.go | `engine/` | excerpt | Sending a text with a 60 s timeout, and the failed row and error reported back to the UI; other message kinds left out |
