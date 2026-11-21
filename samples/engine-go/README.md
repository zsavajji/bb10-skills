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
