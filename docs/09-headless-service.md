# An engine that outlives the UI, and its memory cap

Read this when: your app needs an engine that keeps running after the user closes the UI (messages, a connection, music), you hit `std::bad_alloc` or an engine that exits before `main`, or you want to know what the memory-cap addon does before you ask the user to install it.

Code referenced here is in [../samples/](../samples/). Why the engine is a separate process: [02-architecture.md](02-architecture.md) sections 1-2. The descriptor XML itself: [08-packaging.md](08-packaging.md) section 2. Installing the addon over SSH: [10-phone-access-and-install.md](10-phone-access-and-install.md). Evidence labels: **Q10** / **Classic** with a date means read or seen on that phone; "builds" means compiled only.

## 1. The descriptor side

The service is a second `Qnx/Elf` asset with its own `entryPointId`, an `<entryPoint>` of type `Qnx/Elf`, and an `application.headless` invoke target. From [samples/descriptor/bar-descriptor-headless.xml](../samples/descriptor/bar-descriptor-headless.xml) (full text and the diff from a plain app: [08-packaging.md](08-packaging.md)):

| Piece | Value | Why |
|---|---|---|
| second asset | `entryPointId="BBMusicService"`, `type="Qnx/Elf"`, no `entry="true"` | the engine binary |
| `<entryPoint id>` | `<entryPointType>Qnx/Elf</entryPointType>` | marks it a native process, not Cascades |
| `_sys_run_headless` | system permission | may run with no UI |
| `_sys_headless_nostop` | system permission | "no stop when the UI is swiped away" (bbmusic comment) |
| `_sys_headless_xlarge` | system permission | a memory class: "else the heap is capped at 3 MB" (section 3) |
| invoke target | `application.headless`, `entry-point-id` = the service, filter action `<id>.START`, mime `*`, `<property var="uris" value="data://local"/>` | what the UI invokes |
| `run_when_backgrounded` | app permission (bbmusic, hyperberry, tengram) | no comment in the descriptors |

The phone grants these `_sys_` permissions to an unsigned app (HyperBerry notes, 0.3, Q10, 2026-10-29).

### How the UI starts the service (exact path)

All three headless apps do the same in `EngineLink` (HyperBerry's and Tengram's `EngineLink.cpp`, [samples/engine-link/tcp/EngineLink.cpp](../samples/engine-link/tcp/EngineLink.cpp)):

1. Read `data/engine.json` (`QDir::homePath() + "/engine.json"`): `port` and `token`.
2. If there is a port, connect a `QTcpSocket` to `127.0.0.1:<port>` and send `hello` with the token. If the file is missing, or the connection fails, the engine is not running.
3. Start it with an invoke:

```cpp
InvokeRequest request;
request.setTarget("com.zsavajji.hyperberry.service");
request.setAction("com.zsavajji.hyperberry.START");
InvokeTargetReply *reply = m_invoker->invoke(request);
```

   Target = the invoke target id; action = the filter's action. Then retry connecting.
4. Rules the code keeps, each from an incident:
   - A START that reaches an engine still exiting is lost (about 0.5 s), so another goes soon; a spare one is harmless. Do not send two within 1 s (hyperberry, tengram) / 1.5 s (bbmusic, `ReinvokeMs`).
   - **A background app cannot invoke** ("not allowed to make invoke requests"). The reply is `InvokeReplyError` 2, `BadRequest`, whose "could not be parsed" misleads. A retry every 500 ms kept the CPU busy for 18.5 hours (Q10, 2026-11-03, 55,560 refused STARTs). Each refusal doubles the wait (500 ms up to 60 s), and coming back to fullscreen retries at once (HyperBerry notes, HyperBerry's `EngineLink.cpp`).
   - `QElapsedTimer` has no constructor: call `invalidate()` at construction, or the START limit reads "just sent" forever.
   - The navigator refuses a headless service's invoke of an app ("target of different type"), so the engine cannot bring the UI to the front: the app invokes itself (Tengram's `EngineLink.cpp`, `toFront`).

### What the navigator passes the engine

- It starts the engine with arguments of its own: the package's name and `invoke://localhost` ([samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp) comment; tengram's start line: `start: service ... ignored: [invoke://localhost]`, Tengram notes).
- **Do not treat an unknown argument as an error.** bbmusic 0.1.0.3 took them for a usage error and exited in 80 ms (not caught from a shell, where it was started by hand; Q10, 2026-11-14, BBMusic notes). tengram 0.1.0.2 took a stray argument for the data folder and exited unseen in 60 ms ([samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp)). Log and ignore them. [samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp) still returns usage for any other `--` option: it lets `invoke://localhost` through because it does not start with `--`.
- The data folder comes from `HOME` (the sandbox's `data/`, the UI's `QDir::homePath()`). A headless app's stderr goes nowhere: open the log file first thing ([samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp): `logs/tengramd.log`, the previous one kept once it passes 1 MB; bbmusic `bbmusicd.log`; hyperberry `logs/hyperberryd.log` and `crash.log`).
- The navigator also sends the invoke's action, mime type, uri and data to the process through BPS (`NAVIGATOR_INVOKE_TARGET`; hyperberry's `engine/navigator_qnx.go` hands each action to the engine).
- **Connect to the navigator within 6 s or the process is slain.** The BPS thread starts before everything else: hyperberry connected 366 ms after launch (`engine/navigator_qnx.go`, cgo, `-lbps`), bbmusic's `Navigator().start()` runs before data is opened ([samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp): "it slays a headless process that has not reached it within 6 s"). Both show only in the navigator's log (`slog2info | grep navigator`).
- `NAVIGATOR_EXIT` (sent before a reinstall, with 3 s to go) must become a clean shutdown: the engine left 1.2 s after the exit message and removed `engine.json`; SQLite left no `-wal` file (hyperberry, Q10, 2026-10-29).

## 2. The memory cap: the full story

A headless process runs under `RLIMIT_DATA`, set by the launcher from its capability class ([rlimit-patch](https://github.com/zsavajji/authman-rlimit)'s README):

| Class | `RLIMIT_DATA` |
|---|---|
| none (`headless_heapsize_limit_mb` in `/etc/system/config/launcher.cfg`) | 3 MiB |
| `_sys_headless_medium` | 6 MiB |
| `_sys_headless_large` | 10 MiB |
| `_sys_headless_xlarge` | 30 MiB |

- Soft equals hard, so the app cannot raise it, and no capability grants the `rlimit` ability ([rlimit-patch](https://github.com/zsavajji/authman-rlimit)'s research notes). `RLIMIT_AS` and `RLIMIT_STACK` are unlimited; `RLIMIT_FREEMEM` is 970 (97 % of the phone).
- With no class: the Go runtime exited 47 ms after launch, before `main`, and the UI's invoke got error 6 (`Target`) (HyperBerry notes 0.3; HyperBerry's [`bar-descriptor.xml`](../samples/descriptor/bar-descriptor-hyperberry.xml); Q10, 2026-10-29). The class's size was first not printed anywhere. It is in `/etc/authman/sys.acl` only (`_sys_headless_xlarge` is in no binary).
- What died against 30 MiB: tengram's first sync after a login (467 chats, then TDLib reloading the installed sticker sets) died with `std::bad_alloc`, again and again, until one run got through (0.1.0.3, 2026-11-17, Classic; Tengram notes). hyperberry: four out-of-memory crashes in `crash.log` came from single 9 MB allocations (HyperBerry notes).
- **The "3 MB and 7-25% CPU" budget** in the older cascades skill is "a 2013 BlackBerry blog ... (unverified)" ([skills/bb10-cascades/SKILL.md](../skills/bb10-cascades/SKILL.md)). Measured instead: an idle unlinked Go engine held 6.4 MB of data for 30 min with no kill (Q10, 2026-10-29); bbmusic's engine used 5.6 % of a core playing for 157 s (Q10, 2026-11-14) and 7.7 % over 9 min on the Classic (2026-11-15, BBMusic notes step 2.12). Battery over a night: 3 % an hour for hyperberry with the boot timer (Q10, 2026-11-06/07, HyperBerry notes). Method only: [11-testing-and-debugging.md](11-testing-and-debugging.md).

### Why the obvious fixes fail ([rlimit-patch](https://github.com/zsavajji/authman-rlimit)'s README, [rlimit-patch](https://github.com/zsavajji/authman-rlimit)'s research notes)

| Idea | Why not |
|---|---|
| Edit `sys.acl` | it is on the signed read-only OS image. A procnto link (`ln -sP`) over it wins by longest-prefix match, but authman reads the file once at boot, about 4 minutes before root exists |
| Restart authman with the linked file | authman comes up, but the navigator, the launchers and the rest opened `/dev/authman` once and never reconnect: every app launch fails (navigator: `pps_fd (-1)`, "pthread_mutex_lock failed - Resource deadlock avoided") until a reboot. Tried on the Classic, 2026-11-17, with approval. **This broke every app launch** |
| Add a bigger class at run time with authman's `regprefix` and `load` (what the carrier manager does) | authman refuses any `RLM` action in a capability loaded under a prefix: every `.acl` gave `EINVAL`; `arlm_create` takes the refusal path as soon as a prefix is set |
| Overlay the file before authman starts (a system-data package, as getroot does for `/base/bin`) | installing one needs root, which comes too late in the boot |

## 3. The patch: `rlimit-patch`

authman keeps each class's limit only as the text of its `RLM` action, `rlm RLIMIT_DATA 31457280`, in a 0x200-byte buffer in memory; the launcher gets that text at each launch and parses it. Root can write another process's memory through `/proc/<pid>/as`. So `mempatch` replaces `rlm RLIMIT_DATA 31457280` with `rlm RLIMIT_DATA 134217728` in the running authman: **128 MiB**. Nothing restarts; every launch from then on gets the new limit ([rlimit-patch](https://github.com/zsavajji/authman-rlimit)'s research notes).

Verified on the Classic (SQC100-1, OS 10.3.3.3216), 2026-11-17: an engine launched after the patch logged `RLIMIT_DATA` soft and hard 134217728 and got through the work that killed it at 30 MiB ([rlimit-patch](https://github.com/zsavajji/authman-rlimit)'s README).

### The addon

A root-installer addon, `build/headless-mem.zip`, made by [rlimit-patch](https://github.com/zsavajji/authman-rlimit)'s `build.sh` (needs Docker; compiles `src/mempatch.c` with the NDK's `qcc`):

```sh
/var/rootdata/installer /path/to/headless-mem.zip
```

It copies the addon to `/var/addons/headless-mem`, trusts `mempatch`, and runs `init/start.sh` now and at every boot once root is up. `start.sh` waits up to 120 s for `/dev/authman`, finds authman with `pidin`, and patches it, or logs `already`. The log is `/var/log/installer.log`, lines starting `[HMEM]`. The size is `MB=128` at the top of [rlimit-patch](https://github.com/zsavajji/authman-rlimit)'s `init/start.sh`.

### Caveats

- **A reboot undoes the patch** until the addon runs again, about 4 minutes into the boot. An engine started earlier keeps 30 MiB until it restarts. (The reboot behaviour is from the investigation; a reboot test of the addon is listed as still to do in Tengram notes.)
- **An app keeps the limit of its first launch in a boot until every one of its processes has ended**: the navigator reads the app's capabilities once and keeps them in its object for the app. Seen on the phone: Tengram's UI opened before the patch, its engine restarted after the patch still got 30 MiB; with both processes ended, the next launch got 128 MiB. An app that installs the addon itself, or was opened before the addon's boot run, must close completely once afterwards.
- **An app must ask for the class** on its headless entry point: `<permission system="true">_sys_headless_xlarge</permission>`.
- **`mempatch` safety**: it refuses to write unless the text occurs exactly once, reads its write back, and matches whole strings only (the match must end where the NUL is). It cannot see buffer sizes: do not make a replacement longer than the buffer (authman's is 0x200 bytes for a 25-character text).

```
mempatch <pid> '<find>'                 lists every match
mempatch <pid> '<find>' -w '<replace>'  writes <replace> and its NUL over the one match, then reads it back
mempatch <pid> -d <hexaddr> <len>       a hex dump, read-only
mempatch <pid> -n <number>              where an aligned 32-bit value sits, read-only
```

- **Undo without a reboot**: `mempatch <authman pid> 'rlm RLIMIT_DATA 134217728' -w 'rlm RLIMIT_DATA 31457280'`.
- **Remove**: delete `/var/addons/headless-mem` and reboot.
- Never restart authman, load a class at run time or touch `sys.acl` to "try" something: the first one is what broke every launch. These are changes outside the scratch folder; ask first ([10-phone-access-and-install.md](10-phone-access-and-install.md)).

### How the apps ship it

hyperberry and tengram bundle `headless-mem.zip` in the `.bar` ([08-packaging.md](08-packaging.md) sections 4 and 9) plus a small C helper, `hb-install-patcher` / `tg-install-patcher`. The code is `app/src/MemoryPatch.cpp` (the two are the same file with names changed) and `app/helper/*-install-patcher.c`.

**`MemoryPatch`** ([samples/headless/MemoryPatch.cpp](../samples/headless/MemoryPatch.cpp)):

1. `m_installed` = `/var/addons/headless-mem/init/start.sh` exists.
2. It needs a root helper: `/base/bin/__root` (AutoRoot v4, setuid and setgid root) or `/base/bin/u_root`; else it shows "Root is not available: ... needs AutoRoot v4 with root enabled (`/var/rootdata/use.root`)".
3. `install()` runs, as root: `/var/rootdata/installer "$HB_ZIP"` (the zip is `app/native/headless-mem.zip`) and then waits up to 20 s until a new `[HMEM]` line is in `/var/log/installer.log` (the installer launches `start.sh` in the background and returns before it has patched authman), and prints the `[HMEM]` lines.
4. `runRoot()` starts the root helper with a **minimal environment** (`PATH`, `HOME=/root`, the zip path and the command in `HB_ZIP` / `HB_CMD`, the helper's path in `HB_PATCHER`): the app's relative `LD_LIBRARY_PATH` and sandbox `HOME`/`TMPDIR` must not leak into a root shell. It first sets `FD_CLOEXEC` on every descriptor from 3 up, because Cascades leaves its navigator, GPU, screen, input and PPS connections open across `exec` and the system ends every process holding them when the app exits. It writes a script to the root shell's stdin: copy the helper to `/var/tmp/hb-install-patcher.$$`, `chmod 755`, trust it with `/proc/boot/pathtrust "!$H"`, then run the commands through it and wait for the `.done` output file.
5. `processFinished()` calls it a success when the output contains `Installation completed successfully.`, writes the whole output to `/accounts/1000/shared/documents/hyperberry-memory-patch.txt` (the app has `access_shared`; tengram's file is `tengram-memory-patch.txt`), and shows "Installed. HyperBerry has to close once to use the new memory limit: open it again afterwards."
6. `closeAll()` runs `sleep 1; slay -f hyperberryd; slay -f hyperberry` through the same route: once no process of the app is left, the navigator forgets the capabilities it read at the first launch.

**The helper** ([samples/headless/install-patcher.c](../samples/headless/install-patcher.c)): "On BB10 every process carries the application id of the app that started it, children included, and the system ends all of them when that app exits." It `posix_spawnp`s its arguments with `POSIX_SPAWN_NEWAPP | POSIX_SPAWN_SETSID | POSIX_SPAWN_NOZOMBIE` (set with `posix_spawnattr_setxflags`: "the xflags setter takes 32 bits: the short one behind setflags would drop NEWAPP") and returns 0 at once: the install runs as a new application in its own session and survives the UI closing. It always exits 0, so only the output tells success from failure. Built with `qcc -Vgcc_ntoarmv7le -O2` (HyperBerry's [`build-app.sh`](../samples/ui/scripts/build-app-hyperberry.sh)).

**Where the user finds it.** hyperberry: Settings, Data and storage, "Install the memory patch" (HyperBerry's [`DataSettingsPage.qml`](../samples/headless/DataSettingsPage.qml)). The 2026-11-20 status entry says the engine reports its real cap (`memMB`), the page shows it and installs the addon, and the user decided: Settings only, "no offer at launch, nothing shows it is needed today" (HyperBerry notes; the entry names Tengram in one sentence, apparently meaning the same feature). Tengram's first version asked in a dialog (Tengram notes: **verified on the phone**, 0.1.0.7, 2026-11-17 13:40, clean boot without the addon: the dialog installed the addon, "Close Tengram" ended both processes, the reopened engine logged `rlimit DATA: soft 134217728`; the no-root dialog was never reached).

### The engine logs its limits

At start every engine logs `RLIMIT_DATA` (soft, hard): tengram's `limits()` writes `rlimit DATA: soft ..., hard ...` and the first failed allocation as `out of memory: an allocation failed` (with the limits then, from a `std::set_new_handler`); hyperberry's `readMemMB` logs `rlimit DATA: soft N, hard M` and `engine memory at start: ...`. That line tells whether the phone has the patch. Both raise the soft limit to the hard one when it is lower ([samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp), [samples/engine-go/memlimit.go](../samples/engine-go/memlimit.go)).

### Design consequence

Keep the engine's peak low anyway: "Without the patch the cap is 30 MiB ... The engine should still keep its peak low" (Tengram notes; its first two suspects were patch 0006's 8 MB SQLite cache and the sticker sets TDLib loads at the first sync). hyperberry sets Go's soft memory limit to four fifths of the real cap (24 MiB for 30, 102 for 128; 24 MiB when unknown: [samples/engine-go/memlimit.go](../samples/engine-go/memlimit.go)) so the collector works harder before the OS kills it. Heavy work that needs heap can live in the UI process: hyperberry's call audio is in the UI because "the engine runs headless under a memory cap ... so the microphone, the speaker and their buffers belong to the UI process, which has no such cap" (HyperBerry notes). A crash dump's top frames name the last allocator, not the hog: grep a dump for every frame before blaming it (HyperBerry notes).

## 4. Start at boot

- An **unsigned app cannot run until root is up**, some time after boot (about 4 minutes in), and the boot event also **fires on every install**; so the descriptors say "Not at boot" for the service entry itself (comments in the hyperberry, tengram and bbmusic descriptors; user, 2026-10-29).
- What the sources say works: a recurring timer registered by the UI on the service's headless target, filtered on `bb.action.system.TIMER_FIRED` ([descriptor](../samples/descriptor/bar-descriptor-hyperberry.xml): mime `*`, `uris` `data://local`). From HyperBerry's [`Settings.cpp`](../samples/cookbook/Settings-hyperberry.cpp) (tengram's is the same with its own ids; its excerpt: [samples/cookbook/Settings-excerpts.cpp](../samples/cookbook/Settings-excerpts.cpp)):

```cpp
bb::system::InvokeRecurrenceRule rule(bb::system::InvokeRecurrenceRuleFrequency::Minutely,
                                      bb::system::InvokeDateTime(QDate::currentDate(), QTime(0, 0)));
rule.setInterval(6);
reply = invoker->registerTimer(bb::system::InvokeTimerRequest(kBootTimer, rule, "com.zsavajji.hyperberry.service"));
// off: invoker->deregisterTimer(kBootTimer);
```

  Every 6 minutes is the shortest the service takes. Fires before root is up fail; the first after starts the engine; one that finds it running does nothing, so a killed engine comes back too. A timer survives reboots (`navigator_invoke.h`: "Timer registration is persistent and will survive device reboots"). A setting "Start at boot", off by default (HyperBerry notes 8.2); the UI registers again at each start while it is on.
- **Verified on the Q10, 2026-11-06/07, hyperberry 0.0.1.172, unplugged:** booted 00:05:57; with the app closed the engine started 00:12:02 on the timer's second slot (the 00:06 fire came 3 s after boot, before root), authenticated 00:12:12; a message reached the Hub at 00:16:15 with no UI running. The repeat registration at each UI start has been refused since the first (`boot timer: failed, 2 12292`), harmless: the one timer survived about ten reinstalls and the reboot. Tengram carries the same code ([samples/cookbook/Settings-excerpts.cpp](../samples/cookbook/Settings-excerpts.cpp)); no source here records a phone test of it.
- A headless service can be started at boot by an `application.headless` invoke target filtered on `bb.action.system.STARTED`. hyperberry's phase 0 tried that filter, found it "also fires right after an install, so every new build started its engine without a tap", removed it, and later used the timer. **The timer is what the newest sources use; do not use `STARTED`.**
- bbmusic: the engine "is not started at boot"; the UI starts it, and it stops itself when idle (below).

## 5. Lifecycle facts

| Fact | Source |
|---|---|
| An install closes the app and its engine; the user reopens the app | Tengram notes |
| Opening the UI starts the engine: 2.5 s from launching the UI to the engine connected | HyperBerry notes 0.3 (Q10, 2026-10-29) |
| The engine ran 30 min alive with no kill (started 21:04, alive 21:36) | same |
| **The engine outlives the UI.** bbmusic played 157 s through the UI being swiped away (Q10, 2026-11-14): "A headless entry point may open the speaker, and it outlives the UI" | BBMusic notes (gate app, first round) |
| The engine stops itself when idle: bbmusic exits after 10 minutes with nothing playing (stopped or paused) and no client (`idle_exit_ms = 10 * 60 * 1000`, log "idle for 600 s: exiting"). The gate app had no idle exit | bbmusic's [`engine.h`](../samples/engine-cxx20/engine.h), [`engine.cpp`](../samples/engine-cxx20/engine.cpp); BBMusic notes |
| **Never run the service binary bare from a root shell.** In tengram, with no `--data` the binary IS the service (data folder from `HOME`, Hub account registered): a stray Hub account. A test run always passes `--data` | Tengram notes; [samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp) |
| Root cannot execute an installed app's binaries ("Operation not permitted"): test an installed build through the navigator. A binary copied to the phone runs only after `pathtrust` | HyperBerry notes, `phase0.md` |
| The UI minimising, screen-off and `fullscreen()` fire repeatedly; handlers must be idempotent. A background app cannot invoke | HyperBerry notes |
| BB10 may kill a background app without a clean exit (untested): save state as it changes | same |
| Engine stops on `SIGINT`/`SIGTERM` cleanly (engine.json removed): `sigwait` thread, signals blocked before any other thread | [samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp); Tengram notes |

## 6. System integration a headless engine does

Short pointers; each is a topic of its own.

- **Hub account and notifications** (hyperberry [`engine/hub_qnx.go`](../samples/engine-go/hub_qnx.go), tengram [`engine/hub_qnx.cpp`](../samples/engine-cxx20/hub_qnx.cpp)): `_sys_access_pim_unified` (system permission, never prompted; name from BBTelega's bar), the UDS C library works from an engine with no Qt event loop in synchronous mode, icons are `public="true"` assets read from `/apps/<pkg>/public/`, a tap invokes the UI target with `bb.action.VIEW` and a `pim:` uri, item ids take only letters, digits and `_`. The posting service takes posts only from an app, not from a root shell. The lock screen and the instant preview draw the account's one icon (HyperBerry notes, "A UDS Hub account" and gate 0.5). [08-packaging.md](08-packaging.md) shows the descriptor side.
- **Now-playing and media keys** (bbmusic, BBMusic notes 0.5): `bb::multimedia::NowPlayingConnection` is a client of the PPS service `/usr/sbin/nowplaying` at `/pps/services/multimedia/mediaplayer/control`, and a process with no Qt can do the same. BB10 shows no player on the lock screen; the controls are in the volume overlay. The overlay draws the state it is told and sends the opposite: set the media state on every change. With the engine alone holding it, the buttons arrive as `msg::track` with the command in `dat` (`pause`, `play`, `prev`, `next`, `sendData`, `holdData`) and `msg::revoke` when another player takes over; **passed from a root shell with no Qt (Q10, 2026-11-14), not yet seen from the headless engine in a user session**; "the volume overlay's buttons with the engine holding now-playing" is on the list of things the user has not tried (BBMusic notes). `NowPlayingConnection` in the UI dies with the UI. The overlay's process (`/usr/sbin/nowplaying`, group `1000_shared`) cannot read the app's folder, hence `access_shared` for the cover. More: [12-video-and-audio.md](12-video-and-audio.md).
- **Network status, navigator**: tengram's [`engine/bb10.cpp`](../samples/engine-cxx20/bb10.cpp) (`--service` only: navigator and network events); hyperberry's `engine/navigator_qnx.go` and [`net_qnx.go`](../samples/engine-go/net_qnx.go) (BPS `netstatus`).

## 7. When NOT to go headless: phoenix

Phoenix's engine is a plain `QProcess` child of the UI speaking NDJSON on its stdin/stdout: no entry point, no invoke target, no `_sys_*` permission, no token, no loopback port ([samples/descriptor/bar-descriptor-child.xml](../samples/descriptor/bar-descriptor-child.xml): "The engine: not an entry point, a plain child of the UI (app/native/phoenixd)"; [samples/engine-link/stdio/EngineLink.cpp](../samples/engine-link/stdio/EngineLink.cpp); [02-architecture.md](02-architecture.md)). No memory class is involved, so no cap beyond the UI process's own, and the patch is not needed.

What it costs: the engine dies with the UI. Nothing runs, notifies or plays after the user closes the app, and the system ends every process carrying the app's id when the app exits (helper's comment above). hyperberry's own fallback plan says the same: with the engine as a child of the UI and `run_when_backgrounded`, "Messages and notifications keep coming while the app is minimised, but not after it's closed" (HyperBerry notes, "If headless doesn't hold"; its second option, a tiny headless shim that spawns the engine detached, was never tried). Choose a child process when the app only needs the engine while it is open.

## Where sources disagree

- Arguments: HyperBerry notes 0.4 says "A headless launch gets no arguments"; BBMusic notes ("argv ... invoke://localhost"), [samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp) and Tengram notes (2026-11) show it does. Newer wins; Tengram notes says "no arguments we choose".
- Boot filter: `STARTED` (hyperberry phase 0) against `TIMER_FIRED` ([descriptors](../samples/descriptor/bar-descriptor-hyperberry.xml), [`Settings.cpp`](../samples/cookbook/Settings-hyperberry.cpp), phase 8). `TIMER_FIRED` wins.
- Tengram notes runs the engine as `$E data`; [samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp) takes `--data <dir>` (a bare argument is logged and ignored). `main.cpp` wins.
- Where the memory patch is offered: a first-use dialog (Tengram notes, 2026-11-17) against Settings only (HyperBerry notes, 2026-11-20). The newer entry wins.
- The overlay's controls with the app closed: no controls when held by the UI's `NowPlayingConnection` (gate round 2), controls reach a plain process (0.5 settled). Both are true; the second is why now-playing moved into the engine.

## Samples

- [samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp)
- [samples/headless/MemoryPatch.cpp](../samples/headless/MemoryPatch.cpp)
- [samples/headless/install-patcher.c](../samples/headless/install-patcher.c)
- [samples/engine-link/tcp/EngineLink.cpp](../samples/engine-link/tcp/EngineLink.cpp)
- [samples/engine-go/memlimit.go](../samples/engine-go/memlimit.go)
- [samples/engine-go/navigator_qnx.go](../samples/engine-go/navigator_qnx.go)
- [samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp)
- [samples/descriptor/bar-descriptor-full.xml](../samples/descriptor/bar-descriptor-full.xml)
- [samples/descriptor/bar-descriptor-headless.xml](../samples/descriptor/bar-descriptor-headless.xml)
- [samples/descriptor/bar-descriptor-child.xml](../samples/descriptor/bar-descriptor-child.xml)
