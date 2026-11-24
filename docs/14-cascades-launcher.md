# Starting the UI through the Cascades launcher (`Qnx/Cascades`)

Read this when: the app takes seconds to show its first page, you are choosing between a plain executable and a shared object for the UI, or you are switching an existing UI from `Qnx/Elf` to `Qnx/Cascades`.

Code referenced here is in [../samples/launcher/](../samples/launcher/). The plain-executable build this replaces: [07-ui-build.md](07-ui-build.md) sections 1 and 2. The descriptor around it: [08-packaging.md](08-packaging.md). Everything here was measured on a Classic (SQC100-1, 10.3.3.3216) with Tengram on 2026-11-24, with an unsigned package installed by the root route. HyperBerry got the same three changes the same day and runs the same way (the user's check; its timings were not read).

## 1. What it is

BB10's launcher starts an app by its entry point type. `/etc/system/config/launcher.cfg` on the phone:

```
[LaunchDelegate]
name=cascades
exe_path=/bin/cascades
kind=zygote
interface_path=/dev/cascades

[EntryPointType]
type=qnx/elf
cmd_template=%b %a
launch_method=native

[EntryPointType]
type=qnx/cascades
cmd_template=%e %a
launch_method=delegate,cascades
```

- A `Qnx/Elf` entry point is spawned as a new process. It loads every library itself and then builds the Cascades `Application` from nothing.
- A `Qnx/Cascades` entry point is handed to `/bin/cascades`, a process the launcher keeps running with Cascades, QtDeclarative, the font engine and ICU already loaded (`pidin -p <its pid> libs` lists them). It forks, loads the app's shared object and calls its exported `main()`.
- The system's own Cascades apps start this way: their `MANIFEST.MF` has `Entry-Point-Type: Qnx/Cascades` and an entry such as `app/native/libAudioRecorder.so.1`.
- Momentics built release configurations like this (`CONFIG += cascades10`). The four apps here used a plain executable because it was simpler to build, and paid for it at every start.

## 2. What it saves

Tengram on the Classic, 500 chats. The times are milliseconds after the launcher started the process, from the app's own log ([samples/launcher/main.cpp](../samples/launcher/main.cpp), `sinceLaunch()`).

| Point | Plain executable (1.0.2.128) | Through the launcher (1.0.2.129) |
|---|---|---|
| `main()` reached | 1434 to 1724 | 139 to 176 |
| `Application` constructed | 3166 to 3571 | 255 to 293 |
| Scene set | 4012 to 4426 | 1280 to 1474 |

Three starts were measured as a plain executable (the engine running) and two through the launcher (one with the engine running, one with it starting). The same build also moved the chat list's read off the UI thread, so the last row is not the launcher alone; the first two rows are.

Where the plain executable's time went, seen by sampling the UI thread (section 6):

| Phase | Time | What the thread did |
|---|---|---|
| Before `main()` | about 1.45 s | The loader: about 0.7 s waiting on the flash driver to open and map about 100 libraries, about 0.45 s linking them |
| `Application` constructor | 1.7 to 2.0 s | Almost continuously blocked on the flash driver in libc's `_connect` (file lookups). Which files was not found out |

HyperBerry as a plain executable paid the same 1.7 s constructor on the same phone.

## 3. The three changes

The app's files are [samples/launcher/tengram.pro](../samples/launcher/tengram.pro), [samples/launcher/bar-descriptor.xml](../samples/launcher/bar-descriptor.xml) and [samples/launcher/main.cpp](../samples/launcher/main.cpp). They are one commit after the measured build: the same entry point, a later version number. For a small app the whole change is this.

**The `.pro` file** builds a shared object:

```
TEMPLATE = lib
TARGET = hello
CONFIG += qt warn_on release plugin
QMAKE_LFLAGS += -Wl,-Bsymbolic
```

| Line | Why |
|---|---|
| `TEMPLATE = lib` | a shared object, not an executable |
| `plugin` | one file, `libhello.so`, with no version links (the packager does not carry symlinks) |
| `-Wl,-Bsymbolic` | the object is loaded into a process that already holds the system's libraries, so a global of ours with the same name as one loaded before it (a bundled libwebp, say) would bind to theirs. `-Bsymbolic` binds our references to our own definitions. Preventive: no clash was seen |

No `CONFIG += cascades10` and no Momentics `config.pri` are needed; the rest of the `.pro` stays as in [07-ui-build.md](07-ui-build.md). Static libraries linked in must be position independent (OpenSSL's default build is; objects built `no-pic` cannot go into a shared object). The object folder was deleted once when switching the template, as a precaution, so nothing compiled for the executable was reused.

**The descriptor** points the entry asset at the `.so` and gives the UI's entry point its type:

```xml
<configuration name="Device-Release">
    <platformArchitecture>armle-v7</platformArchitecture>
    <asset entryPointId="Hello" path="../build/ui/app/libhello.so" entry="true" type="Qnx/Elf">libhello.so</asset>
</configuration>

<entryPoint id="Hello">
    <name>Hello</name>
    <entryPointType>Qnx/Cascades</entryPointType>
</entryPoint>
```

- The asset keeps `type="Qnx/Elf"`: that attribute is the execute bit, not the entry point type.
- A headless engine in the same package stays a `Qnx/Elf` entry point ([09-headless-service.md](09-headless-service.md)).
- Check the result: `unzip -p build/NAME.bar META-INF/MANIFEST.MF | grep Entry-Point` shows `Entry-Point: ... app/native/libhello.so` and `Entry-Point-Type: Qnx/Cascades`.
- A descriptor that still names an executable after the `.pro` changed fails at packaging with `Invalid asset path` ([11-testing-and-debugging.md](11-testing-and-debugging.md)).

**`main.cpp`** exports `main` and stops asking Qt where the executable is:

```cpp
Q_DECL_EXPORT int main(int argc, char **argv)
{
    Application app(argc, argv);
    QTranslator translator;
    if (translator.load("hello_" + language, QDir::currentPath() + "/app/native/assets/translations"))
        app.installTranslator(&translator);
```

`QCoreApplication::applicationDirPath()` is `/bin` in a process forked from `/bin/cascades`. The working directory is the app's sandbox root, and the bar's `native` folder is `app/native` under it.

## 4. What is different in the running process

| Thing | Through the launcher | Evidence |
|---|---|---|
| Process name | `bin/cascades`, a child of the launcher's `/bin/cascades`. `pidin -P <app>` finds nothing; find it by parent pid, or by the pid in the app's log | seen with `pidin` |
| `applicationDirPath()` | `/bin`; use `QDir::currentPath() + "/app/native/..."` | follows from the above; the app's translations path was changed for it |
| `LD_LIBRARY_PATH` | the descriptor's value with `/usr/lib/appserv` appended; a library in `app/native/lib` loads | seen: `pidin env`, and `libtgs.so` in `pidin libs` |
| `HOME`, `TMPDIR` | the sandbox's `data` and `tmp`, as before | seen: `pidin env` |
| `CASCADES_THEME` | not listed by `pidin env` for the forked process, yet the descriptor's theme and primary colour apply | the user saw both apps' dark theme and brand colour (Tengram and HyperBerry, 2026-11-24) |
| Process start time | the fork, so `sinceLaunch()` still counts from the launch | the log's 139 to 176 ms |
| `slog2` buffer | the app's own `slog2_register` still works | seen: `slog2info -b tengram` has the new process's lines |
| Signing | none needed: an unsigned package installed by the root route starts this way | verified on the Classic |

## 5. Check a phone before relying on it

Every phone runs the same OS ([01-target-and-rules.md](01-target-and-rules.md)), so this is a sanity check, not a version test:

```sh
ssh PHONE 'grep -n -i -E "cascades|zygote" /etc/system/config/launcher.cfg'
ssh PHONE 'pidin -F "%a %e %A" | grep "bin/cascades"'
```

After installing, the app's log should show `main` within about 0.2 s of the launch.

## 6. How the cost was found

`pidin` on the phone prints a thread's state and a short backtrace, which is enough to tell waiting from working and to name the library. A loop over SSH samples the UI thread about every 60 ms while the app starts (read-only, no file written on the phone):

```sh
ssh PHONE 'set -- $(pidin -P APP -F "%a %b" | grep " 1 *$"); old=${1:-0}
new=; while [ -z "$new" ]; do sleep 0.25
  set -- $(pidin -P APP -F "%a %b" | grep " 1 *$" | grep -v "^ *$old "); new=$1; done
i=0; while [ $i -lt 110 ]; do i=$((i+1)); echo "=== $i"
  pidin -p $new -F "%b %p %l %J %B %h" | grep -v tid
  pidin -p $new backtrace | grep -v backtrace; done
pidin -p $new libs' > sample.txt
```

- `%J` is the state and `%B` what the thread is blocked on: `REPLY 741404` is a wait for the process with that pid (here the flash driver, `devb-sdmmc`), `RUNNING` is CPU work, `RECEIVE` an idle event loop.
- The backtrace is addresses; the `libs` listing at the end gives each library's base, and the libc symbol is the nearest one below the offset in `/proc/boot/libc.so.3`'s dynamic symbols.
- The loop finds the process by name, so it works for a plain executable; for an app started through the launcher, wait for a new child of `/bin/cascades` instead.
- Keep the waiting loop light (`sleep 0.25`): a tight `pidin` loop takes a core at the UI thread's priority and slows the start it measures.

## Samples

- [samples/launcher/tengram.pro](../samples/launcher/tengram.pro), [samples/launcher/bar-descriptor.xml](../samples/launcher/bar-descriptor.xml), [samples/launcher/main.cpp](../samples/launcher/main.cpp)
- The plain-executable versions of the same files: [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro), [samples/descriptor/bar-descriptor-full.xml](../samples/descriptor/bar-descriptor-full.xml)
