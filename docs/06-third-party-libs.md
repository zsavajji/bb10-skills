# Third-party libraries: cross-building and bundling

Read this when: your app needs OpenSSL, libcurl, wolfSSL, libopus, libwebp, SQLite/SQLCipher, TDLib or any other library on BB10, in the engine or in the UI.

Compilers and images: [03-toolchains.md](03-toolchains.md). Linking engines (link order, the `-lz` trap and its fix): [04-engine-cxx20.md](04-engine-cxx20.md). Go libraries: [05-engine-go.md](05-engine-go.md).

## 1. The general recipe

1. **Pin every download by sha256.** In a Dockerfile: `ADD --checksum=sha256:<hash> <url> /tmp/x.tar.gz`. In a script: a `fetch()` that downloads to `<dest>.tmp`, compares the hash, and only then moves it into place ([samples/engine-cxx20/scripts/fetch-third-party.sh](../samples/engine-cxx20/scripts/fetch-third-party.sh)):

```sh
sha() { if command -v sha256sum >/dev/null; then sha256sum "$1"; else shasum -a 256 "$1"; fi | cut -d' ' -f1; }

fetch() {  # dir url sha file
	dest=$1/$4
	mkdir -p "$1"
	[ -f "$dest" ] && [ "$(sha "$dest")" = "$3" ] && return 0
	curl -fsSL "$2" -o "$dest.tmp"
	if [ "$(sha "$dest.tmp")" != "$3" ]; then
		rm -f "$dest.tmp"
		echo "fetch-third-party: $4 does not match its pinned sha256" >&2
		exit 1
	fi
	mv "$dest.tmp" "$dest"
}
```

   macOS's `sha256sum -c` takes no `--status` and fails on stdin: the repo's fetch scripts compare the hash by hand ([samples/third-party/fetch-libwebp.sh](../samples/third-party/fetch-libwebp.sh)). A multi-file tree (libwebp) writes a `.sha256` stamp **last**, so a half-extracted tree is fetched again.
2. **Build static**, install into **`/opt/<app>`** inside the app's build image (`/opt/phoenix`, `/opt/bbmusic`), and link by full path. Engine libraries are fetched at image build time; sources the app compiles itself are fetched into `engine/third_party/` or `lib/`.
3. **Fetched sources are git-ignored, not committed** ([samples/engine-cxx20/gitignore](../samples/engine-cxx20/gitignore), the other apps' `.gitignore`, HyperBerry's and Tengram's). Reason recorded: files over the repo's 500-line rule, licenses, size ([samples/third-party/fetch-qrcodegen.sh](../samples/third-party/fetch-qrcodegen.sh)).
4. **Chain Docker build steps with `&&`**: under `;` an early failure went unnoticed (HyperBerry's pitfalls notes 0.8). Several Dockerfiles also end a block with a grep or `nm` that proves the result (the `libcurl` blocks grep `CMakeCache.txt`; OpenSSL greps `nm` for the NEON symbol).
5. **Patches**: keep them in `patches/<lib>/*.patch`, apply them to the **extracted** tarball (not to your tree), and make a patch that no longer applies **stop the build** (Tengram's `apply_patches`: applied, already applied (reverse applies), or the series is broken; Go: `patch --fuzz=0`, [05-engine-go.md](05-engine-go.md)).
6. **CMake libraries**: `-DCMAKE_TOOLCHAIN_FILE=$TC` with `TC=/opt/bb10-cxx20/share/bb10-cxx20.cmake`.
7. **Autotools libraries**: `./configure --host=arm-unknown-nto-qnx6.5.0eabi CC=bb10-clang` and, for C++ code, `CXX=bb10-clang++ ac_cv_prog_cxx_cxx11=` (empty). Why: Autoconf's C++11 probe assigns a `u8""` literal to `const char *`, which fails under C++20's `char8_t`; it then appends `-std=gnu++11` to `CXX`. An empty value means "none needed" ([bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s README).
8. **NEON is fixed at configure time**: every BB10 CPU has NEON and QNX has no runtime CPU detection, so build with `-mfpu=neon` and turn runtime detection off where the library has the switch.
9. **Never link the SDK's old libssl/libcrypto** (OpenSSL 1.0.x, in the default search path) when you ship your own: pass full paths to your `.a` files ([samples/third-party/build-deps.sh](../samples/third-party/build-deps.sh): "Explicit paths to our .a files: the SDK has its own, older libssl/libcrypto in the default search path").

## 2. OpenSSL 3.5.7 with ARM assembly (phoenix, tengram)

Configure target **`linux-armv4`** (turns the assembly on: QNX has no target of its own with assembly; the Linux one works because the output is plain ELF for the same ABI). Flags: `no-shared no-tests no-dso no-engine no-async -DNO_SYSLOG -O2 -ffunction-sections -fdata-sections`.

- `-DNO_SYSLOG`: the SDK has no `<syslog.h>`.
- `-O2`: the target's default is `-O3`; ours comes after it and wins.
- `-ffunction-sections -fdata-sections`: for `--gc-sections` at the engine link.
- Compiler `CC=bb10-clang`, `AR` and `RANLIB` from the armnto binutils (`/opt/bb10-toolchain/bin/arm-unknown-nto-qnx6.5.0eabi-ar` and `-ranlib`).
- Build **`build_generated` first**, then only `libcrypto.a libssl.a`: the generated headers (`include/openssl/safestack.h`...) are not prerequisites of the library targets, so otherwise the SDK's old OpenSSL headers get picked up; and `make build_libs` also builds `apps/`, which needs `syslog.h`.
- perl: the **full** distribution (FindBin etc.) for Configure and perlasm; `patch` for the QNX patch.

### The REQUIRED `armcap` patch

Without it the library dies on start. OpenSSL finds the CPU's extensions in `OPENSSL_cpuid_setup` (`crypto/armcap.c`) by running one instruction of each and catching SIGILL. **On BB10 those probes leave the thread without an FPU: its next VFP instruction dies with SIGFPE (`FPE_NOFPU`).** The patch adds a `__QNX__` branch before the probes that sets the mask and returns (the same as OpenSSL does for 32-bit iOS). [samples/docker/patches/openssl-0001-qnx-armcap.patch](../samples/docker/patches/openssl-0001-qnx-armcap.patch) and Tengram's own copy of that patch are identical:

```diff
# BB10: no SIGILL probing of CPU features (it leaves the thread without an FPU: SIGFPE at the next VFP instruction); NEON is on every BB10 phone.
--- a/crypto/armcap.c
+++ b/crypto/armcap.c
@@ -267,6 +267,18 @@
         return;
     }
 
+#if defined(__QNX__)
+    /*
+     * BlackBerry 10: after the SIGILL probes the thread is left without an
+     * FPU (its next VFP instruction dies with SIGFPE, FPE_NOFPU). Every BB10
+     * phone's CPU has NEON and none has the ARMv8 extensions, so the mask
+     * is known, as for 32-bit iOS below.
+     */
+    if (1) {
+        OPENSSL_armcap_P = ARMV7_NEON;
+        return;
+    }
+#endif
 #if defined(__APPLE__)
 #if !defined(__aarch64__)
     /*
```

With the mask at `ARMV7_NEON` the NEON paths run: bit-sliced AES (`bsaes-armv7.S`), SHA-1/256/512 `_neon` entries, `armv4-mont.S` bignum, `ghash-armv4.S` GCM, ChaCha20/Poly1305, SHA-3, P-256. The ARMv8 files (`aesv8-armx.S`, `ghashv8-armx.S`) are compiled in and never taken. `armcap.c` reads the `OPENSSL_armcap` environment variable before the patch's branch, so `OPENSSL_armcap=0` runs the same binary on the ARM code without NEON; for the pure C code, Configure with `linux-generic32` (Tengram's OpenSSL ARM notes).

Measured on the phone, `cryptobench`, C build against the assembly build (2026-11-16; the tool is not in the repo; Tengram's doc says "on the phone", the model is not named):

| | C | Assembly |
|---|---|---|
| AES-256-CTR | 18 MB/s | 59 MB/s |
| SHA-256 | 41 MB/s | 113 MB/s |
| SHA-512 | 28 MB/s | 63 MB/s |
| 2048-bit modexp | 13 per second | 58 per second |

The build is right when this prints symbols:

```sh
nm build/prefix/lib/libcrypto.a | grep -E 'sha256_block_data_order_neon|bn_mul8x_mont_neon|ossl_bsaes_ctr32'
```

([samples/docker/Dockerfile.openssl-curl](../samples/docker/Dockerfile.openssl-curl) ends its block with `$BINUTILS-nm $PHX/lib/libcrypto.a | grep -q sha256_block_data_order_neon` so a build without the assembly fails.)

### The Dockerfile block, verbatim ([samples/docker/Dockerfile.openssl-curl](../samples/docker/Dockerfile.openssl-curl))

```dockerfile
FROM bb10-cxx20:latest
ENV PHX=/opt/phoenix TC=/opt/bb10-cxx20/share/bb10-cxx20.cmake BINUTILS=/opt/bb10-toolchain/bin/arm-unknown-nto-qnx6.5.0eabi

RUN apt-get update && apt-get install -y --no-install-recommends perl patch make \
	&& rm -rf /var/lib/apt/lists/*

ADD --checksum=sha256:a8c0d28a529ca480f9f36cf5792e2cd21984552a3c8e4aa11a24aa31aeac98e8 \
	https://www.openssl.org/source/openssl-3.5.7.tar.gz /tmp/openssl.tar.gz
COPY patches/openssl-0001-qnx-armcap.patch /tmp/openssl-0001-qnx-armcap.patch
RUN cd /tmp && tar xzf openssl.tar.gz && cd openssl-3.5.7 \
	&& patch -p1 < /tmp/openssl-0001-qnx-armcap.patch \
	&& CC=bb10-clang AR=$BINUTILS-ar RANLIB=$BINUTILS-ranlib \
		./Configure linux-armv4 no-shared no-tests no-dso no-engine no-async -DNO_SYSLOG -O2 \
			-ffunction-sections -fdata-sections --prefix=$PHX --openssldir=/etc/ssl >/dev/null \
	&& make -s -j"$(nproc)" build_generated >/dev/null \
	&& make -s -j"$(nproc)" libcrypto.a libssl.a >/dev/null \
	&& mkdir -p $PHX/lib $PHX/include && cp libcrypto.a libssl.a $PHX/lib/ \
	&& cp -RL include/openssl $PHX/include/ \
	&& $BINUTILS-nm $PHX/lib/libcrypto.a | grep -q sha256_block_data_order_neon \
	&& cd / && rm -rf /tmp/openssl*
```

(The file has comments between the lines; they say the same as the text above.)

## 3. TLS choices and the CA bundle

| App | TLS library | Reason recorded |
|---|---|---|
| bbmusic | **wolfSSL 5.9.4**, static, client-only: TLS 1.3 and 1.2, a few hundred KB, a supported libcurl backend, GPL (compatible with the project's GPLv3). OpenSSL 3 "would also work and is the fallback if wolfSSL misbehaves on QNX 6.6, at several times the size" | BBMusic's plan §3 "TLS and HTTP" |
| phoenix | OpenSSL 3.5.7 with ARM assembly | [samples/docker/Dockerfile.openssl-curl](../samples/docker/Dockerfile.openssl-curl) |
| tengram | OpenSSL 3.5.7 with ARM assembly (TDLib needs it) | [samples/third-party/build-deps.sh](../samples/third-party/build-deps.sh) |
| hyperberry | Go's own TLS (1.3, current roots) | [05-engine-go.md](05-engine-go.md) |

bbmusic gate 0.3 proved TLS 1.3 by wolfSSL on the phone (Q10, 2026-11-14: `http 200` to `music.youtube.com`, 1.88 s, TLS 1.3 forced with `CURLOPT_SSLVERSION`).

- **The phone's CA store is too old** (2013 per bbmusic's plan; "a CA store from 2018" per HyperBerry's pitfalls). Ship **Mozilla's `cacert.pem`** as a bundle fetched at build time (`curl -fsSL https://curl.se/ca/cacert.pem -o app/assets/cacert.pem`, in [samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh); not committed: `app/assets/cacert.pem` is in `.gitignore`). It lands at `app/native/assets/cacert.pem` and the engine hands it to curl: `http_set_ca_file("app/native/assets/cacert.pem")` under `#ifdef __QNX__`, which sets `CURLOPT_CAINFO`.
- curl is built with **`-DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none`**: no CA path is compiled in.
- BBMusic's plan §3 says the bundle is "compiled in and handed to curl as a blob"; the code ([samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp), BBMusic's build-app script) ships it as an asset file read at run time. The code is newer and wins.
- **Never link the SDK's old libssl** with curl: the curl build passes full paths (`-DOPENSSL_SSL_LIBRARY=$PHX/lib/libssl.a`, ...) and the Dockerfile greps `CMakeCache.txt` to prove it (shown in the next block).

## 4. libcurl 8.23.0, minimal build (verbatim, [samples/docker/Dockerfile.openssl-curl](../samples/docker/Dockerfile.openssl-curl))

HTTP and HTTPS only; OpenSSL backend (the one above); the SDK's zlib for gzip.

```dockerfile
ADD --checksum=sha256:e3de77cfa6367083c776a26647fc9f633b538c06c3baa73d931a252ac8f72b49 \
	https://github.com/curl/curl/releases/download/curl-8_23_0/curl-8.23.0.tar.gz /tmp/curl.tar.gz
RUN cd /tmp && tar xzf curl.tar.gz \
	&& off="DICT FILE FTP GOPHER IMAP LDAP LDAPS MQTT POP3 RTSP SMTP TELNET TFTP IPFS WEBSOCKETS \
		NTLM ALTSVC HSTS NETRC AWS KERBEROS_AUTH NEGOTIATE_AUTH DIGEST_AUTH FORM_API MIME DOH" \
	&& flags="" && for p in $off; do flags="$flags -DCURL_DISABLE_$p=ON"; done \
	&& cmake -S curl-8.23.0 -B curl-build -DCMAKE_TOOLCHAIN_FILE=$TC -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$PHX -DOPENSSL_ROOT_DIR=$PHX -DOPENSSL_USE_STATIC_LIBS=ON \
		-DOPENSSL_INCLUDE_DIR=$PHX/include -DOPENSSL_SSL_LIBRARY=$PHX/lib/libssl.a \
		-DOPENSSL_CRYPTO_LIBRARY=$PHX/lib/libcrypto.a \
		-DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON -DBUILD_CURL_EXE=OFF -DBUILD_LIBCURL_DOCS=OFF \
		-DBUILD_MISC_DOCS=OFF -DBUILD_TESTING=OFF -DCURL_USE_OPENSSL=ON -DCURL_USE_WOLFSSL=OFF \
		-DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF -DUSE_NGHTTP2=OFF -DUSE_LIBIDN2=OFF -DCURL_BROTLI=OFF \
		-DCURL_ZSTD=OFF -DCURL_ZLIB=ON -DENABLE_UNIX_SOCKETS=OFF -DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none \
		$flags >/tmp/curl-configure.log \
	&& grep -E "OpenSSL|OPENSSL" /tmp/curl-configure.log \
	&& grep -q "^OPENSSL_SSL_LIBRARY:.*=$PHX/lib/libssl.a" curl-build/CMakeCache.txt \
	&& grep -q "^OPENSSL_CRYPTO_LIBRARY:.*=$PHX/lib/libcrypto.a" curl-build/CMakeCache.txt \
	&& cmake --build curl-build -j"$(nproc)" >/dev/null && cmake --install curl-build >/dev/null \
	&& grep -E "^Libs|^Requires|ssl_backends|SSL" $PHX/lib/pkgconfig/libcurl.pc \
	&& cd / && rm -rf /tmp/curl*
```

- curl's CMake build has **no `HTTP_ONLY`** any more: each protocol is switched off by its own `CURL_DISABLE_*` (BBMusic's phase 0 notes gate 0.3). The list above is that list.
- Engine link order and `CURL_STATICLIB`: [04-engine-cxx20.md](04-engine-cxx20.md) §4.

### wolfSSL block (verbatim, [samples/docker/Dockerfile.wolfssl-curl-opus](../samples/docker/Dockerfile.wolfssl-curl-opus))

```dockerfile
ADD --checksum=sha256:7256bfc89b183a75183806c7debfa203443873b0b4a562e1b80d68e01b45ac57 \
	https://github.com/wolfSSL/wolfssl/archive/refs/tags/v5.9.4-stable.tar.gz /tmp/wolfssl.tar.gz
RUN cd /tmp && tar xzf wolfssl.tar.gz \
	&& cmake -S wolfssl-5.9.4-stable -B wolfssl-build -DCMAKE_TOOLCHAIN_FILE=$TC -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$BBM -DBUILD_SHARED_LIBS=OFF -DWOLFSSL_CURL=yes -DWOLFSSL_TLS13=yes \
		-DWOLFSSL_EXAMPLES=no -DWOLFSSL_CRYPT_TESTS=no >/dev/null \
	&& cmake --build wolfssl-build -j"$(nproc)" >/dev/null && cmake --install wolfssl-build >/dev/null \
	&& rm -rf /tmp/wolfssl*
```

bbmusic's curl block is the same as above with `-DWOLFSSL_INCLUDE_DIR=$BBM/include -DWOLFSSL_LIBRARY=$BBM/lib/libwolfssl.a`, `-DCURL_USE_WOLFSSL=ON -DCURL_USE_OPENSSL=OFF`. Gate-0.3 note: wolfSSL's `--enable-curl` set is most of the 3.0 MB probe and can be trimmed later.

## 5. libopus 1.6.1

Pinned `sha256:6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1`, `https://downloads.xiph.org/releases/opus/opus-1.6.1.tar.gz`. **`--disable-rtcd` and `CFLAGS="-O2 -mfpu=neon"`**: QNX has no runtime CPU detection; "without the flag the ARM build doesn't compile" (HyperBerry's phase 0 notes 0.8, `pitfalls.md`). From [samples/docker/Dockerfile.wolfssl-curl-opus](../samples/docker/Dockerfile.wolfssl-curl-opus):

```dockerfile
RUN cd /tmp && tar xzf opus.tar.gz && cd opus-1.6.1 \
	&& ./configure --host=arm-unknown-nto-qnx6.5.0eabi CC=bb10-clang CFLAGS="-O2 -mfpu=neon" --disable-rtcd \
		--enable-static --disable-shared --disable-doc --disable-extra-programs --prefix=$BBM >/dev/null \
	&& make -j"$(nproc)" >/dev/null && make install >/dev/null \
	&& cd / && rm -rf /tmp/opus*
```

- **hyperberry builds it twice** ([samples/docker/Dockerfile.go](../samples/docker/Dockerfile.go)): once for BB10 into the SDK sysroot (`--prefix=$T/armle-v7/usr --includedir=$T/usr/include`, `T=/opt/bb10-sdk/target/qnx6`, so `bb10-clang` finds it), then `make distclean`, then again on the host (`CC=cc`, prefix `/usr/local`) for the PC tests. Both install headers as `<opus/opus.h>`.
- **tengram** adds `--enable-fixed-point` (`OPUS_CONFIGURE_FLAGS` in [samples/third-party/build-deps.sh](../samples/third-party/build-deps.sh)) and installs into `build/prefix`.
- The speed: encode at 0.117 of real time on the Q10 (HyperBerry gate 0.8); decode alone 78x real time flat out (bbmusic gate 0.4).

## 6. libwebp 1.4.0 (decoder only)

`https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.4.0.tar.gz`, `sha256:61f873ec69e3be1b99535634340d5bde750b2e4447caa1db9f61be3fd49ab1e5`, unpacked whole; the build picks the files of its `makefile.unix` `DEC_OBJS`, `DSP_DEC_OBJS`, `UTILS_DEC_OBJS` plus the demuxer. No encoder, no sharpyuv; threads off (no `WEBP_USE_THREAD`).

- **In an engine via CMake** ([samples/engine-cxx20/CMakeLists.txt](../samples/engine-cxx20/CMakeLists.txt)): a `webp` static library from those files, `-w`, with **`-mfpu=neon`** on BB10. "BB10: every CPU has NEON, and QNX has no runtime detection; without `HAVE_CONFIG_H`, `dsp/cpu.h` turns `WEBP_USE_NEON` on when the compiler defines `__ARM_NEON__` (what `-mfpu=neon` does) and `cpu.c`'s `armCPUInfo` then answers 'yes' to `kNEON` on any OS but Linux's RTCD." The SSE/MIPS/MSA files compile to empty stubs on another CPU.
- **Inside the qcc UI via the `.pro` file lists** (HyperBerry's [`.pro`](../samples/ui/hyperberry.pro), [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro)): `WEBP_DEC`, `WEBP_DSP`, `WEBP_UTILS` plus `src/demux/demux.c` and `anim_decode.c`, with `INCLUDEPATH += $$WEBP_DIR $$WEBP_DIR/src`. **qcc passes `-mcpu=cortex-a9` and no `-mfpu=neon`, so `__ARM_NEON__` is unset and the NEON, SSE and MIPS files compile to empty stubs: the plain C code decodes.** Left as `# ponytail: add -mfpu=neon to QMAKE_CFLAGS if a sticker takes long to draw; untried.` The fetch scripts: [samples/third-party/fetch-libwebp.sh](../samples/third-party/fetch-libwebp.sh).

## 7. Single-file and header-only pieces

| Library | How | Source |
|---|---|---|
| **yyjson 0.13.0** (JSON) | two files fetched from `raw.githubusercontent.com/ibireme/yyjson/0.13.0/src`, pinned by sha256 each, into `engine/third_party/yyjson/`; compiled with `-w` inside the `util` module | [samples/engine-cxx20/scripts/fetch-third-party.sh](../samples/engine-cxx20/scripts/fetch-third-party.sh), [samples/engine-cxx20/CMakeLists.txt](../samples/engine-cxx20/CMakeLists.txt) |
| **qrcodegen** (Nayuki, C, MIT) | commit of release v1.8.0, sha256-pinned, into `lib/qrcodegen/`; compiled by the UI's `.pro` (`tengram`: `-std=gnu99`) | [samples/third-party/fetch-qrcodegen.sh](../samples/third-party/fetch-qrcodegen.sh) |
| **OpenMAX IL headers** (Khronos 1.1.2 as AOSP carries them, nine headers) | fetched pinned; **only headers**: "the phone's `libOmxCore.so` is `dlopen()`ed, so nothing links" (`/base/lib/libOmxCore.so`) | [samples/video/fetch-omxil.sh](../samples/video/fetch-omxil.sh), `.pro` comment |
| **rlottie** (Telegram Desktop's fork, `cbfdcdd...`) | built by **clang** into **`libtgs.so`**, which exports **rlottie's C API only** (`tests/tgs.map`) with libc++ static inside, shipped in the bar's `native/lib`; the qcc UI calls it as plain C. Options: no image-loader plugin, no render thread, no cache; NEON on (`-mfpu=neon`); pixman's NEON asm assembled apart with the armnto GNU `as` (clang's assembler rejects its macros); `LOT_BUILD` makes the C API visible in a static build; Samsung's rlottie was left out because it dropped parts of stickers. A program started over ssh cannot map a library outside the system's trusted file system (mmap EPERM, even after `pathtrust`), while a bar's `native/lib` loads | [samples/third-party/build-tgs.sh](../samples/third-party/build-tgs.sh) |
| **Noto Color Emoji** (Google; font reduced by [`build-emoji-font.py`](../samples/third-party/build-emoji-font.py)) | fetched pinned to a release commit, derived font kept out of the repo | [samples/third-party/fetch-emoji.sh](../samples/third-party/fetch-emoji.sh) (UI topic: [07-ui-build.md](07-ui-build.md)) |
| **Mozilla `cacert.pem`** | section 3 | |

## 8. A static libcrypto inside the UI process (tengram)

Tengram's UI links a clang-built static OpenSSL (`libcrypto.a`, from the same `build/prefix`) for SQLCipher. Traps recorded in [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro):

- **`QMAKE_LFLAGS += -Wl,--exclude-libs,ALL`**: "Nothing of the static libcrypto may show in the executable's dynamic symbols: a system library that uses the phone's own OpenSSL 1.0 (QtNetwork, say) would bind to ours, 3.5, and crash."
- A clang-built `libcrypto.a` needs the **`__aeabi_mem*` helpers** ([`ui/sqlcipher/aeabi_mem.c`](../samples/third-party/aeabi_mem.c), compiled into the UI) and **`-lsocket`**: `LIBS += $$PWD/../build/prefix/lib/libcrypto.a -lsocket`.
- `QMAKE_LFLAGS += -Wl,--gc-sections`; `QMAKE_CFLAGS_WARN_ON = -w` silences the C libraries' warnings, the C++ ones stay on.

### SQLCipher via Qt's SQLite driver, renamed ([samples/third-party/fetch-qtsqlcipher.sh](../samples/third-party/fetch-qtsqlcipher.sh))

The phone's QtSql already registers a `QSQLiteDriver`, so a second driver of the same name would clash. The script fetches Qt **4.8.6**'s `qsql_sqlite.cpp` and `.h` from `raw.githubusercontent.com/qt/qt/v4.8.6/src/sql/drivers/sqlite/` (sha256-pinned), and `sed`-renames `QSQLite` to `QSQLCipher`, pointing the include at [`tdsqlite_names.h`](../samples/third-party/tdsqlite_names.h) instead of `<sqlite3.h>`; output `lib/qtsqlcipher/qsql_sqlcipher.cpp/.h`, not committed. The UI compiles that with **TDLib's own SQLCipher amalgamation** (`third_party/td/sqlite/sqlite/sqlite3.c`) and TDLib's own flags (`OMIT_MEMLOCK SQLITE_DEFAULT_MEMSTATUS=0 ... SQLITE_HAS_CODEC ... USE_PREAD=1`), **without `SQLITE_OMIT_DECLTYPE`** because Qt's driver asks for declared column types. It needs `gnu99` (see [03-toolchains.md](03-toolchains.md) §3.2). `ui/sqlcipher/` holds the spike and its test ([`scripts/build-qtsqlcipher-test.sh`](../samples/third-party/build-qtsqlcipher-test.sh)).

## 9. TDLib / TDLight, the heavy case ([samples/third-party/build-deps.sh](../samples/third-party/build-deps.sh))

Cross-builds **TDLight** (TDLib plus a few options), **OpenSSL** and **libopus** as static libraries into `build/prefix`; about **12 minutes**, "idempotent, stamped".

Structure:

1. **Host side (the Mac)**: `fetch_td` clones the repo at a **pinned commit** (`TD_REPO` default `https://github.com/tdlight-team/tdlight`, `TD_COMMIT` default `ccf8956e63d29074009867859ecad251c75f36e1`; override both to build stock TDLib); `apply_patches` applies `patches/td/*.patch` with `git apply` (already applied = reverse applies; otherwise the build fails loudly: "TDLight moved? Rebase the patch."); `fetch_openssl`, `fetch_opus` download and **check sha256**.
2. **Container side**: the same script re-run inside `tengram-build` as `build-deps.sh --in-container <function>` (`build_openssl_in_container`, `build_opus_in_container`, `build_td_in_container`, `verify_in_container`, `link_test_in_container`).
3. **TDLib in two passes**: pass 1 on the **host compiler** (`CC=clang CXX=clang++`, host libssl/zlib from the image's `-dev` packages) builds the target `prepare_cross_compiling`, which writes the generated sources (`td/generate/auto`) into the source tree; pass 2 is the **BB10 cross build** with `-DCMAKE_TOOLCHAIN_FILE=$BB10_CXX20/share/bb10-cxx20.cmake`, `-DTD_ENABLE_LTO="$TD_LTO"`, our OpenSSL by path (`-DOPENSSL_FOUND=1 -DOPENSSL_INCLUDE_DIR ... -DOPENSSL_CRYPTO_LIBRARY ... -DOPENSSL_SSL_LIBRARY`), the SDK's zlib, `-DTD_SKIP_TEST=ON -DTD_SKIP_BENCHMARK=ON -DTD_INSTALL_SHARED_LIBRARIES=OFF`; target `tdclient` only (`cmake --install` is not used because it wants the JSON interface). `td_api.h/.hpp` are copied from the host pass's output.
4. **Patches** (Tengram's patches), each file starting with a `#` comment line saying why:

| Patch | Why |
|---|---|
| `td/0001-qnx-platform` | recognise QNX (`platform.h` falls through to Cygwin otherwise); `poll()` plus the socketpair eventfd |
| `td/0002-qnx-signals` | QNX 6.6 has no `sigaltstack`, `SA_ONSTACK`, `SA_RESTART`, `MINSIGSTKSZ` |
| `td/0003-qnx-stat-times` | `struct stat` has whole-second times only |
| `td/0004-qnx-walk-path` | no `fdopendir` and no `d_type`: reopen directories by path |
| `td/0005-poll-no-busy-loop` | `poll()` is level-triggered; without it a connected socket makes the loop spin (64 % of a core on the Q10) |
| `td/0006-qnx-sqlite-io` | SQLite page cache 8 MB per connection (default 2 MB), no SQLCipher memory wiping, pread/pwrite |
| `openssl/0001-qnx-armcap` | section 2 |
| `tg_owt/0001-qnx-port`, `tgcalls/0001-qnx-no-transport-cc` | calls (below) |

5. **Stamp files keyed on patch hashes**: `stamped <name> <stamp> <fn>` skips a step if `build/prefix/.stamp-<name>` equals the stamp, which names everything that shapes the output: version, flags, the **sha256 of all patches**, `TD_LTO`, `TD_BUILD_TYPE` and the **toolchain image id** (`docker image inspect -f '{{.Id}}'`), so a rebuilt base image rebuilds everything. Other CMake flags are not in the stamp: `FORCE_REBUILD=1` after changing those.
6. **Switches**: `FORCE_REBUILD=1`; `JOBS=2` if the `td_api` translation units (over 1 GB of RAM each) get OOM-killed; `TD_LTO=ON` (default) needs a bb10-cxx20 with the linker plugin, else `TD_LTO=OFF`; `TD_BUILD_TYPE=Release` (-O2; `MinSizeRel` -Os is "about a tenth smaller and slower"); `BASE_IMAGE` (default `bb10-cxx20:emutls-lto`).
7. **Verify and link test**: every member of every `.a` must be ARM (`readelf -h`; with LTO the archives hold bitcode, which readelf does not read, so the link test proves the target), then a `main()` using `td::ClientManager` is linked against explicit `.a` paths (not run: "no phone here").

**[samples/third-party/build-calls.sh](../samples/third-party/build-calls.sh)** in a few lines: fetches **tg_owt** and **tgcalls** at the pair Telegram Desktop builds (pinned commits, `git clone` + `checkout`, submodules for tg_owt), applies `patches/tg_owt` and `patches/tgcalls` with the same applied/reverse/broken test, builds with `calls/CMakeLists.txt` (Telegram Desktop's source list without desktop capture, group calls (need ffmpeg) and other platforms; [`calls/QnxInterface.cpp`](../samples/third-party/calls/QnxInterface.cpp) is ours; software video codecs left out, so libvpx, openh264, ffmpeg and libjpeg are not needed; their include paths point at an empty folder) in `-O2`, copies `libtgcalls.a` and `libtg_owt.a` to `build/prefix/lib/`, writes the defines and includes a program needs to `build/prefix/calls.flags`, and links and strips `tests/calls_check`, the on-phone check. It needs `build-deps.sh` first.

## 10. Go dependencies

Patching them without forking: [05-engine-go.md](05-engine-go.md) §5.

## Samples

- [samples/docker/Dockerfile.openssl-curl](../samples/docker/Dockerfile.openssl-curl)
- [samples/docker/patches/openssl-0001-qnx-armcap.patch](../samples/docker/patches/openssl-0001-qnx-armcap.patch)
- [samples/engine-cxx20/scripts/fetch-third-party.sh](../samples/engine-cxx20/scripts/fetch-third-party.sh)
- [samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh)
- [samples/engine-cxx20/CMakeLists.txt](../samples/engine-cxx20/CMakeLists.txt)
- [samples/engine-cxx20/main-child.cpp](../samples/engine-cxx20/main-child.cpp)
- [samples/docker/Dockerfile.wolfssl-curl-opus](../samples/docker/Dockerfile.wolfssl-curl-opus)
- [samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp)
- [samples/third-party/build-deps.sh](../samples/third-party/build-deps.sh)
- [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro)
- [samples/docker/Dockerfile.go](../samples/docker/Dockerfile.go)
- [samples/third-party/fetch-libwebp.sh](../samples/third-party/fetch-libwebp.sh)
- [samples/third-party/build-tgs.sh](../samples/third-party/build-tgs.sh)
- [samples/third-party/build-calls.sh](../samples/third-party/build-calls.sh)
- [samples/third-party/fetch-qtsqlcipher.sh](../samples/third-party/fetch-qtsqlcipher.sh)
- [samples/video/fetch-omxil.sh](../samples/video/fetch-omxil.sh)
- [samples/third-party/fetch-qrcodegen.sh](../samples/third-party/fetch-qrcodegen.sh)
- [samples/third-party/fetch-emoji.sh](../samples/third-party/fetch-emoji.sh)
- [samples/third-party/patches/td/](../samples/third-party/patches/td/)
