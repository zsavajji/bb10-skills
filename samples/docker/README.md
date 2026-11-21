# docker

Build images for the engines. Copied from the app repos; not built in this repo.

| File | From | Kind | Shows |
|---|---|---|---|
| Dockerfile.openssl-curl | phoenix, `docker/Dockerfile` (4e06969) | verbatim | bb10-cxx20 plus static OpenSSL and curl |
| Dockerfile.wolfssl-curl-opus | bbmusic, `docker/Dockerfile` (1d80813) | verbatim | bb10-cxx20 plus wolfSSL, curl and Opus |
| Dockerfile.go | hyperberry, `docker/Dockerfile` (df45afc) | verbatim | go-qnx plus bb10-cxx20 for cgo |
| Dockerfile.tdlib | tengram, `docker/Dockerfile` (5a65cdf) | verbatim | TDLib build image |
| patches/openssl-0001-qnx-armcap.patch | phoenix, `docker/patches/` (4e06969) | verbatim | OpenSSL patch for QNX ARM |
