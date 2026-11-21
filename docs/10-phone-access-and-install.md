# The phone over root SSH: access, test binaries, installs

Read this when: you are about to touch the phone (read a log, run a test binary, install a `.bar` or an addon), or an install seems to do nothing.

Paths in parentheses are relative to the `bb10/` folder. Facts about the phones and their shell: [01-target-and-rules.md](01-target-and-rules.md) sections 5-6. What to do with a log once you have it: [11-testing-and-debugging.md](11-testing-and-debugging.md). Addon and cap background: [09-headless-service.md](09-headless-service.md). Building the `.bar`: [08-packaging.md](08-packaging.md).

## 1. The rules, first

| Rule | Source |
|---|---|
| The phone is **read-only for agents** by default. | `hyperberry/PLAN.md` section 13 answer 9 (via `hyperberry/docs/memory/q10-root-ssh.md`); `tengram/docs/HANDOFF.md` section 3 |
| **Allowed without asking:** copy and run test binaries in the app's own scratch folder (`/root/<app>`, e.g. `/root/tengram`, `/root/bbmusic`, `/root/hyperberry`), and read anything (logs, `pidin`, files). | `tengram/docs/HANDOFF.md` section 3 |
| **Ask in the moment, every time**, before installing a `.bar` or an addon, and before any change outside the scratch folder (OS state, `/var`, authman, PPS). "The user approved each such step this session one at a time; approval doesn't carry over." | `tengram/docs/HANDOFF.md` section 3 |
| **Never restart a system process.** Restarting authman "broke every app launch until a reboot". Check how a system process's clients behave before touching it. | `tengram/docs/HANDOFF.md` section 3; `rlimit-patch/research/findings.md` (Route 1) |
| **Ask before a reboot**: another agent may be using the phone. | `hyperberry/docs/memory/q10-root-ssh.md` |
| Do not turn Development Mode on over root SSH (it needs the device password, which only the user has). | `hyperberry/docs/memory/q10-install-bar.md` |
| Installing: the user installs their own builds (Sachesi or DDPB). "Ask before installing by any route"; on 2026-10-31 an agent's install attempt ran long and the user said "stop it, I will install". Later, hyperberry builds went over the root route "when the user says 'install it'", and the phoenix and bbmusic scripts record that "the user allowed writes over ssh for tests on 2026-11-14". | `hyperberry/docs/memory/q10-install-bar.md`; `phoenix/scripts/install-phone.sh`; `bbmusic/docs/plan/ui.md` ("Build, install, look": "installing test builds this way. No reboots, no system settings") |
| Install only when it will not interrupt what the user is doing: an install closes the app and its engine ([11-testing-and-debugging.md](11-testing-and-debugging.md)). | `tengram/docs/HANDOFF.md` section 4 |
| Never log out a session the user needs, and keep tests on a real account in a safe place. | `tengram/docs/HANDOFF.md` section 3 |

**Note for agents:** the operator's own global instructions may restrict SSH hosts to read-only unless they say otherwise. The allowances above come from these projects' notes for the dev phones. If your instructions are stricter, follow them and ask.

## 2. `ssh bb10rootx`

- A root `ksh` over USB networking (169.254.0.1). The host alias is in the operator's SSH config (`hyperberry/docs/memory/q10-root-ssh.md`; `tengram/docs/HANDOFF.md` section 3: "USB only").
- **The alias is the cable, not a phone.** It reaches whichever phone is plugged in. On 2026-11-20 it was the Classic, reported as "the Q10" until the user said so. Check first:

```sh
ssh bb10rootx 'grep modelname /pps/services/deviceproperties'
```

It prints `modelname::Classic` on the Classic. Name the phone from that, and from whether `data/session.db` or `data/sim` exists for hyperberry (`hyperberry/docs/memory/q10-root-ssh.md`).

- **Missing on the phone:** `head`, `tr`, `uniq`, `strings`, `printf`, `id`, `diff`, `rmdir`, `sqlite3`, `grep -A/-B` (`-a -o` work). Available: `grep`, `awk`, `sed`, `find`, `sort`, `slog2info`, `pidin`, `dig`, `ping`, python 3.2 (but root may not run `python -c`). `pidin` printed nothing for process lists on 2026-11-01 (`q10-root-ssh.md`), yet the addon's `start.sh` and `bbmusic/scripts/measure-phone.py` use it: test it before relying on it.
- **Workarounds:** filter on the Mac (`ssh bb10rootx 'cat f' > local`, or `scp` it off first); write lines with `echo >>`, not `printf` ("the phone has no printf": `phoenix/scripts/install-phone.sh`); `cat` a PPS object rather than grep it (`bbmusic/docs/plan/ui.md`, step 2.12: "read it with `cat`, the phone's grep cannot"). The phone's `grep` is also unreliable with `\|` alternation: copy the log off and grep on the Mac (BBMusic session notes, 2026-11-21).
- **Clean reboot** (ask first): `/proc/boot/shutdown -S reboot -r <reason>` (`q10-root-ssh.md`).
- **Useful read-only checks** (answered questions on 2026-11-01, `q10-root-ssh.md`):

```sh
grep Package-Version /apps/com.zsavajji.hyperberry*/META-INF/MANIFEST.MF   # the installed version
cat /pps/services/notify/settings/cat.com.zsavajji.hyperberry              # notification settings (LED, previews)
cat "/pps/services/led/status?wait,delta"                                   # the LED as it blinks, timestamp it on the Mac
slog2info | grep "IF: "                                                      # what the system launched on a tap (invocation framework)
```

  System libraries can be copied off the phone and read with the SDK's `ntoarmv7-objdump` (in `yamsergey/bb10-ndk:0.6`): that is how `libbps`'s LED messages were found. The same worked for authman and the launcher (disassembled on the Mac, `rlimit-patch/research/findings.md`).
- The test phones are the Q10 and the Classic only; see [01-target-and-rules.md](01-target-and-rules.md) section 6.

## 3. Running a test binary: `pathtrust`

BB10 runs a new file (scripts included) only once `/proc/boot/pathtrust !<file>` trusts that one file. A file it refused stays refused, even as a new file that reuses its inode. Always write the `!`: without it the whole filesystem is trusted (`hyperberry/docs/plan/phase0.md` 0.2). So each build gets a fresh name, `<basename>-<sha8>-<n>`; the file is copied through `/dev/shmem`; a refused copy is emptied and parked in `denied/` so its inode is never handed out again. Root cannot execute an installed app's binaries ("Operation not permitted"), so an installed build is tested through the navigator, not by hand.

`tengram/scripts/push.sh`, the generic version, verbatim:

```sh
#!/bin/sh
# Copies a program or a shell script to the phone over ssh, makes it runnable, prints its path there.
# Usage: scripts/push.sh <file> [check-argument]     (PHONE=bb10rootx, DIR=/root/tengram)
#
# BB10 runs a new file (scripts included) only once `/proc/boot/pathtrust !<file>` trusts that one
# file, and a file it refused stays refused, even as a new file that reuses its inode. So each
# version gets a fresh name, <basename>-<sha8>-<n>, it is trusted before its first run, and a
# refused copy is emptied and parked in denied/ (kept, so its inode is never handed out again).
# The check is `<file> <check-argument>` exiting with anything but 126 (not permitted); the default
# argument is -h. For a script that must not run as a check, pass `-n` handling yourself or use
# a harmless argument. From ../hyperberry/scripts/push-engine.sh.
set -eu
PHONE=${PHONE:-bb10rootx}
DIR=${DIR:-/root/tengram}
SRC=$1
CHECK=${2:--h}
base=$(basename "$SRC")
name=$base-$(shasum -a 256 "$SRC" | cut -c1-8)
scp -q -o BatchMode=yes "$SRC" "$PHONE:/dev/shmem/$name"
ssh -n -o BatchMode=yes "$PHONE" "mkdir -p $DIR/denied && cd $DIR && for n in 1 2 3 4 5 6; do
	f=$name-\$n
	[ -e \$f ] || { cp /dev/shmem/$name \$f && chmod +x \$f && /proc/boot/pathtrust !$DIR/\$f; }
	./\$f $CHECK >/dev/null 2>&1 </dev/null; rc=\$?
	if [ \$rc -ne 126 ]; then
		rm -f /dev/shmem/$name; echo $DIR/\$f; exit 0
	fi
	: > \$f; mv \$f denied/
done
rm -f /dev/shmem/$name; echo 'no copy would run' >&2; exit 1"
```

Use: `E=$(scripts/push.sh build/tengramd)` then run `$E` with the arguments on the phone, always with `</dev/null` and in the background when it serves.

| Script | Difference |
|---|---|
| `bbmusic/scripts/push-phone.sh` | `DIR=/root/bbmusic` fixed; extra files are copied beside the program; the program is run bare, or with `PROBE_ARG`, and **must return 0 at once**; there is no exit-code-126 test; older copies of the same program are removed on success; hash with `shasum` |
| `hyperberry/scripts/push-engine.sh` | `DIR=/root/hyperberry`; default file `build/hyperberryd`, named `hbd-<sha8>`; probe is `-h` exiting 0; a copy of this build that already runs is reused ("copying over it (it may be running) would make cp stop and ask"); older builds are removed; hashes with `sha256sum` |
| `tengram/scripts/push.sh` | the one above: probe argument settable, anything but 126 counts as runnable (so a program may exit non-zero on `-h`), `denied/` kept |

**A program started over ssh cannot load a shared library of ours.** `mmap` fails with EPERM for a `.so` outside the system's trusted file system, even after `pathtrust`; the same library loads from an installed bar's `native/lib`. So link a test program static, or test the library inside an installed app (Tengram's `libtgs.so`: commit `f154d37`, `tengram/scripts/build-tgs.sh`).

Rule: **never run the service binary bare** from a root shell: see [09-headless-service.md](09-headless-service.md) section 5. A test run of the engine passes `--data`.

## 4. Installing a `.bar` through the installer's PPS job

Ask first. The route is the phone's own installer, driven from root: the package in `/var/rootdata`, mode 644; a PPS job object the installer may write its result into; three lines appended with `echo`. Learned 2026-10-31 for another app (it is how AutoRoot installs `Term49.bar`); used for hyperberry builds since 0.1.1.304 (`hyperberry/docs/memory/q10-install-bar.md`); written up for tengram in `tengram/docs/install-root-pps.md`: "no Momentics, no BAR deployer and no developer mode"; the installer daemon watches `/pps/system/installer/upd/current/`, runs the job like any install, and appends the outcome to the same file.

`phoenix/scripts/install-phone.sh`, verbatim (`bbmusic/scripts/install-phone.sh` differs only in the usage line and `name=bbm$(date +%s)`):

```sh
#!/bin/sh
# Installs a .bar on the Q10 over root ssh, through the system installer's PPS job object, and
# prints what the installer answered. The route HyperBerry's notes describe
# (../hyperberry/docs/memory/q10-install-bar.md): the package in /var/rootdata, mode 644; a job
# object the installer may write its result into; three lines appended with echo (the phone has
# no printf). The user allowed writes over ssh for tests on 2026-11-14.
#
#   scripts/install-phone.sh build/phoenix.bar
set -eu
PHONE=${PHONE:-bb10rootx}
BAR=$1
name=phx$(date +%s)
scp -q -o BatchMode=yes "$BAR" "$PHONE:/var/rootdata/$name.bar"
ssh -o BatchMode=yes "$PHONE" "chmod 644 /var/rootdata/$name.bar
	J=/pps/system/installer/upd/current/job.$name.bar
	umask 000; touch \$J; chmod 666 \$J
	echo 'action::install' >> \$J
	echo 'package_location::/var/rootdata/$name.bar' >> \$J
	echo 'extras::' >> \$J
	i=0
	while [ \$i -lt 90 ]; do
		if grep '^result::' \$J >/dev/null 2>&1; then break; fi
		sleep 1; i=\$((i+1))
	done
	grep -E '^(result|errorstr|error|message)' \$J
	rm -f /var/rootdata/$name.bar"
```

The same by hand, when a project has no script yet (`tengram/docs/install-root-pps.md`, verbatim; `N` is any tag that keeps job names apart, e.g. the build number). This version also prints the version the installer ended up with and removes the job file:

```sh
# 1. Copy the package to a folder the installer can read
scp build/tengram.bar bb10rootx:/var/rootdata/tgN.bar

ssh bb10rootx '
  chmod 644 /var/rootdata/tgN.bar
  J=/pps/system/installer/upd/current/job.tgN.bar

  # 2. Create the job file, writable by the installer (umask 000, mode 666)
  (umask 000; touch $J; chmod 666 $J)

  # 3. The job: what to do, and where the package is
  echo "action::install"                          >> $J
  echo "package_location::/var/rootdata/tgN.bar"  >> $J
  echo "extras::"                                 >> $J

  # 4. Wait up to 2 minutes for the installer to write result:: into the file
  i=0; while [ $i -lt 60 ]; do grep -q "result::" $J && break; sleep 2; i=$((i+1)); done
  grep -E "result::|actual_version::" $J

  # 5. Clean up
  rm -f /var/rootdata/tgN.bar $J
'
```

A good install prints `result::success` and `actual_version::<versionNumber>.<buildId>`.

Step by step:

1. `scp` the package to `/var/rootdata/<name>.bar` with a unique name (`phx<epoch seconds>`; hyperberry used `job.hb<build>.bar`, tengram `tgN`).
2. `chmod 644` it. The installer reads it from there.
3. The job is a PPS object `/pps/system/installer/upd/current/job.<name>.bar`. `umask 000; touch; chmod 666` makes it writable by the installer, which writes `result::` back into it.
4. Append three lines with `echo >>`: `action::install`, `package_location::/var/rootdata/<name>.bar`, `extras::`. (Not `printf`: the phone has none.)
5. Wait for a line starting `result::` (phoenix: up to 90 s; tengram's inline copy in `tengram/docs/install-root-pps.md` and `tengram/docs/HANDOFF.md` section 4 wait up to 60 x 2 s, "up to 2 minutes").
6. Read the answer: `result::success` on success, `errorstr`/`error`/`message` otherwise. Tengram's version also prints `actual_version::` (`grep -E "result::|actual_version::" $J`): compare it with the `Package-Version` your build script printed.
7. Clean up: `rm -f` the package (and, in tengram's version, the job).

What to know:

- **`Package-Version` must go up.** The installer keeps the installed app when the version did not change. Check the printed `actual_version::` or the `MANIFEST.MF` under `/apps/` ([08-packaging.md](08-packaging.md) section 3).
- **An install closes the app and its engine**; the user reopens the app (`tengram/docs/HANDOFF.md` section 4). The navigator sends `NAVIGATOR_EXIT` with 3 s to go; a headless engine should shut down cleanly ([09-headless-service.md](09-headless-service.md)).
- **Data survives an upgrade**: "The sandbox (`chats.db`, `db/`, logs) is kept" (`tengram/docs/install-root-pps.md`); hyperberry's boot timer survived about ten reinstalls (`hyperberry/docs/plan/phase8.md`). Only an uninstall deletes `data/`. Any other `result::` line carries the installer's error; the job file stays unchanged if nothing answers within 2 minutes.
- The older route, 2026-10-29: pipe a third-party installer app's script into `sh` with the `.bar` in `/var/tmp` (`cat /apps/com.tbbx.barinstaller.pps.testDev_staller_pps9b7dea97/native/barinstall.sh | sh -s /var/tmp/hyperberry.bar`; the path was already gone by 2026-10-31). The PPS job route replaced it (`hyperberry/docs/plan/phase0.md` 0.3).
- A built-but-uninstalled `.bar` is "builds", not "works" ([11-testing-and-debugging.md](11-testing-and-debugging.md)).

## 5. Installing an addon (root installer)

```sh
/var/rootdata/installer <zip>
```

Log: `/var/log/installer.log` (`[HMEM]` lines for the memory addon). Ask first. The installer copies the addon to `/var/addons/<name>`, trusts its binaries, runs `init/start.sh` now and at every boot once root is up. Addon layout follows `bb10-volte`'s; `rlimit-patch/README.md` has the files (`MANIFEST`, `init/start.sh`, `init/perms`, `init/trusted`). Removing: delete `/var/addons/<name>` and reboot. The memory addon's behaviour and the in-app installer: [09-headless-service.md](09-headless-service.md) section 3.

## 6. The other routes and their state

| Route | State |
|---|---|
| **The user's manual route**: Sachesi or DDPB (DBBT is another) with Development Mode on (Settings, Security and Privacy) | the user's own; the usual way before the root route. Unsigned apps need a rooted phone (`hyperberry/.claude/skills/bb10-build-deploy/SKILL.md`, "Install", older) |
| **Root PPS job** (section 4) | works; used for phoenix, bbmusic, hyperberry and tengram builds when the user allows it |
| **`blackberry-deploy` / `BarDeploy.jar` from the Mac** | "exists in the SDK but hasn't been used in this workflow" (skill). Tried 2026-10-31 and **never completed end to end** (`hyperberry/docs/memory/q10-install-bar.md`): the SDK image's container cannot reach 169.254.0.1 (the Docker VM's link-local network), so run `BarDeploy.jar` with the Mac's own Java; Homebrew Java timed out connecting to 169.254.0.1 (probably macOS's Local Network permission), while curl, nc and ssh got through; a local Python TCP relay `127.0.0.1:8443` to `169.254.0.1:443` works and BarDeploy takes `-device 127.0.0.1:8443`; an `ssh -L` tunnel through `bb10rootx` does not work (the phone resets connections that come from itself); the phone speaks only TLS 1.0 on its deploy server, so pass `-Djava.security.properties=<file>` with `jdk.tls.disabledAlgorithms=SSLv3, RC4, DES, NULL, anon` or Java fails with "peer not authenticated"; Development Mode must be on and needs the device password, and `login.cgi` answers with an empty body when it is off |

(The note about TLS 1.0 is about the phone's deploy server, not about Qt: the "Qt 4.8 speaks TLS 1.0 only" claim was corrected on 2026-11-20, [01-target-and-rules.md](01-target-and-rules.md) section 4.)

Never spend time on keystores, author certificates or debug tokens ([01-target-and-rules.md](01-target-and-rules.md) section 3).

## 7. Getting things off the phone

- `ssh bb10rootx 'cat <file>' > local` for one file; `scp -q -o BatchMode=yes bb10rootx:<path> .` for several. Filter on the Mac.
- The sandbox: `/accounts/1000/appdata/<id>.testRel_<name><hash>/data/` (full pattern and an example: [08-packaging.md](08-packaging.md) section 7). Use a glob such as `/accounts/1000/appdata/com.zsavajji.hyperberry.*/data` (`hyperberry/scripts/sim.py`).
- Logs: `data/logs/tengramd.log` and `logs/ui.log` (tengram, `tengram/docs/HANDOFF.md` section 3), `bbmusic-ui.log` and `bbmusicd.log` (bbmusic, `bbmusic/docs/plan/ui.md`), `ui.log` (phoenix), `slog2info -b <app>` for the system log; the older convention was `/accounts/1000/shared/misc/<app>.log` that the user copies to the PC. More: [11-testing-and-debugging.md](11-testing-and-debugging.md).
- Databases: copy `chats.db` off to inspect; `session.db` never leaves the phone (`hyperberry/docs/memory/q10-root-ssh.md`). The phone has no `sqlite3`; tengram's `dbq` test binary reads its encrypted `chats.db` on the phone (`tengram/engine/README.md`).
- **Screenshots**: the user takes one on the phone; "you have a screenshot" means copy the newest `/accounts/1000/shared/camera/IMG_*.png` into the project's `debug/<step>/` (never committed) and look. Measure from it (the Q10 is 720 x 720, 1 du = 9 px) (`hyperberry/docs/memory/working-with-the-user.md`).
- Without a cable: a small HTTP server on the PC on the same Wi-Fi that the phone posts lines to (skill, "Logs and crashes").

## 8. App-specific test drivers

A pattern worth copying: a script that drives the app's engine on the phone over an SSH tunnel to its loopback port.

- `hyperberry/scripts/sim.py`: reads `data/engine.json` (`port`, `token`) through ssh, opens an `ssh -N -L` tunnel to that port, and sends engine commands. Subcommands: `on` (a file `data/sim` simulates the account and restarts the engine), `link`, `msg N [text]`, `typing N`, `voice N [seconds]`, `photo N`, `react N [emoji]`, `story [text]`, `storyview N`, `removed`, `off`, `state`. It changes only the simulated account's made-up chats.
- Allowed without asking for hyperberry's sim only ("yes, 100% run on the phone", the user, 2026-10-31): say what was run and why. Not on a phone that holds the real account (`data/session.db`, no `data/sim`): check the model and the account first.
- Run a notification check with the app minimised (`ui.state` fullscreen false in the engine's log) before `sim.py msg`: the chat on screen never notifies.
- Tengram's equivalents: `tengram/tests/engine_client.py <cmd> '<json>' --listen 10` through an ssh tunnel, `tests/engine_smoke.py`; do not run beside the app on the same session (`tengram/docs/HANDOFF.md` section 5). bbmusic: the engine runs on the phone from a root shell with `--null` and is driven from the Mac through `ssh -L` (`bbmusic/docs/plan/phase1.md`, `bbmusic/docs/plan/ui.md`).

## Where sources disagree

- "The user installs every build themselves" (`hyperberry/docs/memory/q10-install-bar.md`, 2026-10-31; `tengram/docs/ui-fork.md` section 5) against root-route installs by agents when asked (same note, later; phoenix and bbmusic scripts). Rule of thumb that fits both: the user decides, per install.
- The test phone is called "the Q10" in older notes (`bbmusic/docs/plan/ui.md`, `phoenix/scripts/install-phone.sh`) and is mostly the Classic from 2026-11-15; check `modelname`.
- `hyperberry/docs/plan/phase0.md` used a third-party installer script; the PPS job in `q10-install-bar.md` is the newer, simpler route.

## Sources

- `hyperberry/docs/memory/README.md`, `q10-root-ssh.md`, `q10-install-bar.md`, `q10-sim-driver.md`, `working-with-the-user.md`
- `tengram/docs/install-root-pps.md`, `tengram/docs/memory/tengram-direction.md`
- `tengram/docs/HANDOFF.md` (sections 3-5), `tengram/scripts/push.sh`, `bbmusic/scripts/push-phone.sh`, `hyperberry/scripts/push-engine.sh`
- `phoenix/scripts/install-phone.sh`, `bbmusic/scripts/install-phone.sh`
- `hyperberry/scripts/sim.py`, `tengram/tests/engine_client.py`, `bbmusic/scripts/measure-phone.py`
- `rlimit-patch/README.md`, `rlimit-patch/research/findings.md`
- `bbmusic/docs/plan/ui.md`, `bbmusic/docs/plan/phase1.md`, `hyperberry/docs/plan/phase0.md`, `hyperberry/docs/plan/phase8.md`
- `hyperberry/.claude/skills/bb10-build-deploy/SKILL.md` (older)
