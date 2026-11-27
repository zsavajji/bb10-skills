# The target and the rules

Read this when: you start a new BlackBerry 10 app, or you are about to add an OS-version check, a "no root" fallback, a signing step, or a "works" claim.

Every fact below is fixed. Each later doc assumes them. Code referenced here is in [../samples/](../samples/).

## 1. One OS, forever

- Every phone that runs these apps runs BlackBerry 10 **10.3.3**. The test phones report **10.3.3.3216** (read on the phone by the Tengram work: Tengram notes; the Q10 reading is in [skills/bb10-devices/SKILL.md](../skills/bb10-devices/SKILL.md)).
- BB10 gets no further updates.
- So: **no OS version checks, no fallbacks, no "in case a future OS changes this" code.** What works on the test phone works everywhere (HyperBerry notes, Tengram notes).
- Test for features, never for the version: the OS is QNX 6.6-based but `uname` prints QNX 8.0.0 and `_NTO_VERSION` is 800 ([skills/bb10-devices/SKILL.md](../skills/bb10-devices/SKILL.md)).
- Rule zero from HyperBerry: never remove a workaround the phone proved, on the theory that it is redundant. Say it looks redundant and let a phone test decide (HyperBerry notes).

## 2. Every phone is rooted

- No user of these apps has an unrooted phone. Root-only paths, PPS objects, system files and root addons are **product design**, not lab hacks. Do not build "without root" fallbacks (Tengram notes, HyperBerry notes).
- Example already shipped as product: the memory-cap addon `rlimit-patch`, installed by the app itself (Tengram notes: "Every user is rooted ... so the addon is part of installing Tengram").
- **Installing on the user's phone still needs their go-ahead, every time** (HyperBerry notes; Tengram notes: "approval doesn't carry over"). How the phone is reached and how installs work: [10-phone-access-and-install.md](10-phone-access-and-install.md).

## 3. Signing is dead

- BlackBerry's signing and debug-token servers shut down in **January 2022**. Packages are **unsigned**: `blackberry-nativepackager -package` without `-sign` ([skills/bb10-build-deploy/SKILL.md](../skills/bb10-build-deploy/SKILL.md)).
- Never spend time on keystores, author certificates or debug tokens (same file). Packaging steps: [08-packaging.md](08-packaging.md).
- An unsigned app starts only once root is up, some time after boot (HyperBerry notes; Tengram notes: root comes about 4 minutes after boot).

## 4. The platform stack

| Layer | Fact | Source |
|---|---|---|
| OS | QNX 6.6-based; `uname` says QNX 8.0.0 | [skills/bb10-devices/SKILL.md](../skills/bb10-devices/SKILL.md) |
| CPU | 32-bit ARMv7 (`armle-v7`), **softfp** | HyperBerry notes |
| libc | from 2014 (gaps: [04-engine-cxx20.md](04-engine-cxx20.md)) | [skills/bb10-devices/SKILL.md](../skills/bb10-devices/SKILL.md) |
| NEON | every BB10 CPU has NEON; **QNX has no runtime CPU detection**, so NEON is fixed at configure time (`--disable-rtcd`, `-mfpu=neon`) | [samples/docker/Dockerfile.go](../samples/docker/Dockerfile.go), [samples/docker/Dockerfile.wolfssl-curl-opus](../samples/docker/Dockerfile.wolfssl-curl-opus) |
| UI toolkit | **Qt 4.8** (4.8.6 on the phone) + **Cascades `bb.cascades 1.4`**, not Qt 5 | HyperBerry notes (`libQtNetwork.so.4.8.6`); [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro) (Qt 4.8.6 SQLite driver) |
| UI compiler | NDK `qcc`: GCC 4.6.3 (default) or 4.8.3 with `-std=gnu++11`; there is no 4.8.4 ([03-toolchains.md](03-toolchains.md)) | [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro) |
| Engine compiler | clang 19 + libc++ 19.1.7 (C++20), or go-qnx 1.27.4 (Go) | [bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s README, HyperBerry notes |
| Phone SQLite | 3.8.7.1 measured at `/usr/lib/libsqlite3.so.1` (HyperBerry); BBMusic's plan says 3.7.14.1 "(the user, 2026-11-14)". Both have WAL. Write SQL both accept | HyperBerry notes 0.4; BBMusic notes |
| Phone TLS (system) | OpenSSL 1.0.2g, TLS 1.2 capable; CA store from 2018, no TLS 1.3, no redirects followed by Qt. Apps still ship their own TLS and CA bundle ([06-third-party-libs.md](06-third-party-libs.md)) | HyperBerry notes (Qt 4.8's TLS entry) |
| `ARG_MAX` | 61440: long command lines to child processes do not fit | [skills/bb10-build-deploy/SKILL.md](../skills/bb10-build-deploy/SKILL.md) |
| Headless heap | 3 MB without a memory class; `_sys_headless_xlarge` = 30 MiB, raised to 128 MiB by `rlimit-patch` | HyperBerry notes 0.3; [rlimit-patch](https://github.com/zsavajji/authman-rlimit)'s README; [09-headless-service.md](09-headless-service.md) |

Older text that says "Qt 4.8's TLS is limited to 1.0" is wrong; the note was corrected on 2026-11-20 (HyperBerry notes). The newer note wins.

## 5. The phone's shell is poor

The phone's shell lacks these tools: `head`, `tr`, `uniq`, `printf`, `strings`, `id`, `diff`, `rmdir`, `sqlite3`, and `grep -A/-B` (`grep -a -o` works). Sources: HyperBerry notes, Tengram notes, [samples/phone/install-phone.sh](../samples/phone/install-phone.sh) ("the phone has no printf").

- Filter on the Mac: `ssh bb10rootx 'cat f' > local`, or `scp` the file off first.
- Write lines into files with `echo >>`, not `printf`.
- Available: `grep`, `awk`, `sed`, `find`, `sort`, `slog2info`, `pidin`, `dig`, `ping`, python 3.2 (HyperBerry notes).
- Details and the full ssh recipe: [10-phone-access-and-install.md](10-phone-access-and-install.md).

## 6. The test phones

- `ssh bb10rootx` is a root shell over USB networking (169.254.0.1). The alias is **the USB cable, not a phone**: it reaches whichever phone is plugged in, the Q10 or the Classic (HyperBerry notes, 2026-11-20: a Classic was reported as "the Q10" until the user said so).
- **Before naming the phone, run:**

```sh
ssh bb10rootx 'grep modelname /pps/services/deviceproperties'
```

  It prints `modelname::Classic` on the Classic (same file). The test phone has mostly been the Classic since 2026-11-15 (Q20, SQC100-1, OS 10.3.3.3216): BBMusic notes, Tengram notes.
- **Never hardcode the device in code.** Read it (BPS `deviceinfo`, as Tengram's engine does: Tengram notes).
- Device classes in one table (px per du decides layouts; full detail in [../skills/bb10-devices/SKILL.md](../skills/bb10-devices/SKILL.md)):

| Class | Phones | Pixels | px per du |
|---|---|---|---|
| Keyboard 720x720 | Q10, Q5, P'9983 | 720x720 | 9 |
| Keyboard 720x720 Classic | Classic (Q20) | 720x720 | 8 |
| Keyboard 1440x1440 | Passport | 1440x1440 | 12 |
| All Touch 768x1280 | Z10, P'9982 | 768x1280 | 10 |
| All Touch 720x1280 | Z30, Leap, Z3 | 720x1280 | 8 |

  Source: [skills/bb10-devices/SKILL.md](../skills/bb10-devices/SKILL.md). Keyboard phones are portrait only. OLED phones (Q10, Z30, P'9983) should default to the dark theme.
- The author has only a Q10 and a Classic. For any model you have no phone for (for them the Passport's touch keyboard, the on-screen keyboard of the Z10/Z30 and the like), only others' logs can check the code: say "builds; not tried on a <model>" (same file, "Testing reality").

## 7. Working rules with the user that recur

| Rule | Where it comes from |
|---|---|
| Say **"builds"**, not "works", until the phone confirms. Report plainly: verified on the phone, only compiles, failed. The user checks claims against the phone | [skills/bb10-build-deploy/SKILL.md](../skills/bb10-build-deploy/SKILL.md) ("Handing a build to the user"); Tengram notes; HyperBerry notes ("Say exactly what was verified and how") |
| Keep the evidence labels: **verified on the phone (Q10/Classic, date)**, **builds, not tried on a phone**, **untried** | used throughout BBMusic notes, HyperBerry notes (`[Q10]`, `[SDK]`, `[untested]`) |
| The phone is **read-only for agents** except our own scratch folder (`/root/<app>`) | HyperBerry notes; Tengram notes |
| **Ask before installing** a `.bar` or an addon, and before any change outside the scratch folder (OS state, `/var`, authman, PPS). One such step "broke every app launch until a reboot" | HyperBerry notes; Tengram notes |
| Ask before rebooting the phone: another agent may be using it | HyperBerry notes |
| **Bump the build id on every build** handed to the phone; the installer keeps the old app when Package-Version is unchanged. Check `META-INF/MANIFEST.MF` | HyperBerry notes; [skills/bb10-build-deploy/SKILL.md](../skills/bb10-build-deploy/SKILL.md) |
| Secrets (API keys, tokens) are never in a tracked file, notes or memory. Pass them on the command line or from a git-ignored key file | Tengram notes |
| **Screenshots are never committed**; measure from them and write the numbers | HyperBerry notes |
| **No file over 500 lines**, code and docs alike (HyperBerry's rule; sub-agents split files) | HyperBerry notes |
| Comments say why, with the measured number when there is one; deliberate ceilings get a `ponytail:` comment naming the upgrade path | HyperBerry notes |
| A platform limit quoted from old notes is checked first (the "TLS 1.0 only" claim was wrong) | HyperBerry notes (2026-11-20) |
| Surveys of other repos are done by the main model by hand; sub-agents carry out a decided plan; review every sub-agent diff, "for the calls, not only the report" | Tengram notes; HyperBerry notes |
| "Please just stop" / "stop and report" means stop at once | both files above |
| One commit per step, with what was verified and what only compiles | Tengram notes |

### Translations

New apps ship the user's usual language set, and the source strings are written in English ([samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh): "the sources are in English, which has no file"). The set is the user's standing instruction (2026-11-21, while building Phoenix): English, Italian, French, German, Polish, Russian, Hindi, Vietnamese, Indonesian. What the build files show:

| App | `.ts` languages | Source |
|---|---|---|
| phoenix | it fr de pl ru hi vi id | [samples/ui/app.pro](../samples/ui/app.pro) |
| bbmusic | en it fr de pl ru uk hi vi | BBMusic's [`.pro`](../samples/ui/bbmusic.pro) |
| hyperberry | it fr de es ru uk pl hi id vi | HyperBerry's [`.pro`](../samples/ui/hyperberry.pro) |
| tengram | it fr de es ru uk pl hi id vi | [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro) |

The common core is it, fr, de, pl, ru, hi, vi, id; use `en, it, fr, de, pl, ru, hi, vi, id` for a new app. An existing app keeps its own list (HyperBerry and Tengram also have es and uk, BBMusic uk). How `lupdate`/`lrelease` are run: [07-ui-build.md](07-ui-build.md).

## 8. Checklist before you write the first line of a new app

Each item restates a rule above; nothing here is new.

1. No `if (osVersion ...)`, no "unrooted" branch, no signing step in any script (sections 1 to 3).
2. Name the phone only after `grep modelname /pps/services/deviceproperties`; read the device in code, never hardcode it (section 6).
3. UI in Cascades on Qt 4.8 with `qcc`; everything that needs C++14 or newer in a separate process or behind a C API ([02-architecture.md](02-architecture.md), [03-toolchains.md](03-toolchains.md)).
4. Engine language chosen from the table in [02-architecture.md](02-architecture.md) section 6; NEON flags set at configure time (section 4 above).
5. Gates first: prove headless start, TLS, the speaker or the database on the phone before app code ([02-architecture.md](02-architecture.md) section 8).
6. Scripts print the package version; the build id is bumped for every build handed over; the user installs, or says "install it" ([08-packaging.md](08-packaging.md), [10-phone-access-and-install.md](10-phone-access-and-install.md)).
7. Reports use the evidence labels: verified on the phone (model, date), builds, not tried on a phone, untried ([11-testing-and-debugging.md](11-testing-and-debugging.md)).
8. Skills for the details: [../skills/bb10-cascades/SKILL.md](../skills/bb10-cascades/SKILL.md), [../skills/bb10-ux/SKILL.md](../skills/bb10-ux/SKILL.md), [../skills/bb10-devices/SKILL.md](../skills/bb10-devices/SKILL.md), [../skills/bb10-pitfalls/SKILL.md](../skills/bb10-pitfalls/SKILL.md), [../skills/bb10-cxx20/SKILL.md](../skills/bb10-cxx20/SKILL.md), [../skills/bb10-build-deploy/SKILL.md](../skills/bb10-build-deploy/SKILL.md).

## Samples

- [samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh)
- [samples/phone/install-phone.sh](../samples/phone/install-phone.sh)
- [samples/docker/Dockerfile.go](../samples/docker/Dockerfile.go)
- [samples/docker/Dockerfile.wolfssl-curl-opus](../samples/docker/Dockerfile.wolfssl-curl-opus)
