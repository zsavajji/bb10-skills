# Video, camera and audio

Read this when: your app must play a video, show decoded H.264 frames from the network, encode the camera, play or record PCM, keep sound going with the app closed, or show now-playing controls. It also holds the full OpenMAX research, so nobody repeats the failed routes.

Paths in parentheses are relative to the folder that holds the app repos side by side (the author's `bb10/`; adjust to where yours are). Why the engine is a separate process and what stays in the UI: [02-architecture.md](02-architecture.md). The headless service, its descriptor and its memory cap: [09-headless-service.md](09-headless-service.md). Phone rules (every phone rooted, one OS, 10.3.3): [01-target-and-rules.md](01-target-and-rules.md).

Evidence labels. **Q10 / Classic (Q20)** with a date and build = seen on that phone. **builds** = compiled, not tried on a phone. **untried** = not even run. Dates 2026-11 are the newest and win over older notes and over the skills. The Classic became the test phone on 2026-11-15 (`bbmusic/docs/plan/status.md`); older notes say "Q10" for it.

## 1. Decision table

| I need to... | The apps use | Code | Evidence |
|---|---|---|---|
| Play a video file full screen | `MediaPlayer` (`bb.multimedia`) into a `ForeignWindowControl`; the file is a local `file://` path | `hyperberry/app/assets/VideoViewer.qml`, `tengram/app/assets/VideoViewer.qml`, `phoenix/app/assets/VideoPage.qml` | Q10 2026-11-12 (stories gate, `hyperberry/docs/plan/stories.md`); Classic 1.0.0.90 for length and refused seek (`tengram` commit `74bb400`); Phoenix: builds |
| Play a video from a URL | Download to a file first, then play the file. Phoenix's engine fetches, merges and remuxes it | `phoenix/engine/src/media/video.cpp`, `phoenix/engine/src/mp4/` | builds |
| Show video inside my own page | The same control; the system player creates the window | `StoryViewer.qml` (`hyperberry/app/assets/`) | Q10 2026-11-12 |
| Decode H.264 frames I receive from the network | OpenMAX **IL**, `OMX.qcom.video.decoder.avc`, `dlopen`ed, in the UI process | `hyperberry/app/src/VideoDecoder.*`, `VideoFormat.cpp`, `VideoOutput.cpp`, `VideoWindow.*` | Q10 0.0.1.190 2026-11-07, up to 4 at once 0.0.1.259 2026-11-11; Classic 2026-11-19 (Tengram) |
| Encode the camera to H.264 | The camera API's own hardware encoder, `camera_start_encode` (`-lcamapi`, `use_camera`) | `hyperberry/app/src/CameraEncoder.*` | Q10 0.0.1.193 2026-11-07; Classic 0.1.0.49 and .50 2026-11-19 |
| Play PCM | `libasound` plugin API opened through `audio_manager` (`-lasound -laudio_manager`) | `hyperberry/app/src/CallAudio.cpp`, `bbmusic/engine/src/audio/qsa_sink.cpp` | Q10 2026-11-07 (calls), 2026-11-14 (music) |
| Play Opus audio | There is no Opus on BB10: decode with static libopus yourself, then PCM (music) or a WAV for `MediaPlayer` (voice notes) | `bbmusic/engine/src/audio/`, `hyperberry/docs/plan/phase0.md` 0.8 | Q10 2026-10-29, 2026-11-14 |
| Record the microphone | Notes: `AudioRecorder` (48 kHz stereo WAV). Calls: `libasound` capture | `hyperberry/app/assets/Recorder.qml`, `CallAudio.cpp` | Q10 2026-10-29; calls Q10 0.0.1.187 |
| Keep audio playing with the app closed | The headless engine opens the speaker itself | `bbmusic/engine/src/audio/qsa_sink.cpp` | Q10 gate 0.4, 2026-11-14 |
| Show now-playing and media buttons | The volume overlay, through a PPS object the engine writes | `bbmusic/engine/src/platform/now_playing.cpp` | Q10 gate 0.5, 2026-11-14 |
| Ring on an incoming call | mm-renderer plays the phone's tone (no audio buffers in your process) | `hyperberry/docs/plan/phase7.md` "Ringing" | Q10 0.0.1.185 to .186; Tengram Q10 0.1.0.40-44 |
| Move a voice note to the earpiece | `AUDIO_TYPE_VOICE` handle routed to `AUDIO_DEVICE_HANDSET` + the proximity sensor | `hyperberry/app/src/Proximity.cpp`, `VoicePlayer.qml` | builds (0.0.1.181); no phone report found |

## 2. Playing a video file or stream (the "video player")

No app uses the system player as its viewer, and none decodes a file with OpenMAX. All three viewers do the same:

| App | Viewer | What happens before it plays |
|---|---|---|
| HyperBerry | `VideoViewer.qml`, `StoryViewer.qml` | the engine downloads the file (`media.get`); the UI plays `file://<path>` |
| Tengram | `VideoViewer.qml` (a fork with the fragmented-MP4 handling below), `MediaOpener` | same, through TDLib's download |
| Phoenix | `VideoPage.qml` | the engine resolves, downloads and remuxes (below) |
| BBMusic | no video | Opus decoded by the engine, no `MediaPlayer` |

The core of all three (`tengram/app/assets/VideoViewer.qml`):

```qml
MediaPlayer {
    id: player
    videoOutput: VideoOutput.PrimaryDisplay
    windowId: surface.windowId
    windowGroupId: surface.windowGroup
    statusInterval: 250
    ...
}
...
ForeignWindowControl {
    id: surface
    windowId: "chatVideo"
    ...
    visible: boundToWindow
    updatedProperties: WindowProperty.Size | WindowProperty.Position | WindowProperty.Visible
}
```

`start()` is called after the page is pushed, not on the control's bind: "the player creates the window when it prepares, and the control binds to it after, so waiting for the bind would never start anything" (same file). Set `player.sourceUrl = "file://" + path` and call `play()`. The window mm-renderer makes sits under Cascades' hole, so Cascades controls draw over it and a tap on a button over it reaches Cascades; the first play took 1.3 s to prepare and a replay 0.12 s, so the next story's file is fetched ahead (Q10, 2026-11-12: `hyperberry/docs/plan/stories.md`). An H.264/AAC MP4 played to the end with sound there.

### Limits found

| Limit | Fact | Source |
|---|---|---|
| Picture shape | "mm-renderer stretches the picture to the window", and a message's width and height can be wrong (a TikTok clip came out square, 2026-11-17). Size the control from `onVideoDimensionsChanged` (`ratio = height / width`) and fit it to the screen | `hyperberry/app/assets/VideoViewer.qml`, commit `0345caf` |
| Fragmented MP4 | moof boxes; `mvhd`, `tkhd` and `mdhd` say 0 and the length is only in `mvex`'s `mehd`. mm-renderer reports "Track duration 0" and refuses every seek (`UnsupportedOperation`). A 3:44 video showed 0:00 / 0:00 and a touch on the timeline restarted it | `tengram` commit `74bb400`, `tengram/app/src/Mp4Length.h` |
| What Tengram does about it | reads the length from the file (`Mp4Length`: `mvhd`, else `mehd`); the player's position still moves, so the time reads 0:02 / 3:44. For a fragmented file the timeline is hidden and an "Open in player" button hands the file to the system (`bb.action.OPEN`). A seek the player refuses is not taken for a file that cannot play. A file whose length the player does not know takes no seek | same |
| Evidence | the length and the refused seek were seen on the Classic, 1.0.0.90 (`ui.log`); the hidden timeline and the button were "not built" when that commit was written and "untried on a phone" in `tengram/docs/HANDOFF.md` | same |
| Remote URL | "mmrenderer only opens complete local files"; [bbtube](https://github.com/BBerryLife/bbtube)'s audio-only mode plays a remote URL but with seek disabled; its video path stitches MP4s on disk | `bbmusic/PLAN.md` section 2 |
| Failure | on a player error both viewers call `opener.openFile(path)`, the system's app gets the file (`fail()`) | `VideoViewer.qml` |
| Loop and mute | a GIF: `repeatMode: RepeatMode.Track`, `volume: 0` in Phoenix | `phoenix/app/assets/VideoPage.qml` |
| Audio focus | not stated in any source read | |

### What Phoenix's engine does to MP4 and DASH before the UI plays it

Reddit serves DASH: separate single-file video and audio renditions. mm-renderer takes one source and cannot seek a fragmented file, so the engine (`phoenix/engine/src/media/video.cpp`) does, on one worker, one video at a time (a new `media` cancels the old):

1. `mpd.cpp` reads the DASH manifest with a hand-written tag scanner. `pick_video` takes the rendition whose shorter side is the greatest not above `max_height` (default 480, setting "Video quality"), only codecs starting `avc1` ("the phone decodes H.264 only"); `pick_audio` takes the best audio at most 128000 bit/s, `mp4a` only.
2. It downloads the pieces into the cache (cap 300 MB per video, `kMaxVideoBytes`).
3. `remux.cpp` writes one progressive MP4 (ftyp, moov, mdat) by stream copy, no decoding, merging video and audio. Choices copied from bbtube's remuxer "which plays there": `ftyp` major `isom` (a copied `dash` major made mm-renderer reject the file), `ctts` version 0, movie timescale 1000, half-second chunks alternating between tracks, no `mvex`. It reports width, height, duration and whether there is audio. Result "not fragmented" means the file is played as it is.
4. The UI gets `media` events (`progress`, `ready` with `file`, `w`, `h`, `duration_s`, `audio`) and plays the file with a timeline (`phoenix/docs/protocol.md`, `media` command).

Evidence for Phoenix: the code and unit tests build; the only phone fact in the sources is the action-bar measurement on a Q10 screenshot (`VideoPage.qml`, 0.1.0.24, 2026-11-21). No source reports a video played on a phone.

## 3. The OpenMAX research (HyperBerry, 2026-11-07)

The user's rule: video calls only if the Q10 decodes H.264 in hardware; software decoding on a Krait is not an option. The question was whether received H.264 access units can be decoded from memory, one frame at a time (`hyperberry/docs/plan/phase7.md`, "Video calls: the hardware-decode gate").

### Which API is which

| Name | What it is | Result |
|---|---|---|
| OpenMAX **AL** | The SDK's documented media API (`libOpenMAXAL`, headers `OMXAL/OpenMAXAL*.h`, QNX extensions) | decodes **a URI** in hardware; **cannot decode from memory** |
| OpenMAX **IL** | Qualcomm's core on the phone, `/base/lib/libOmxCore.so`, standard IL entry points, undocumented on BB10 | **works**, one access unit per buffer |
| mm-renderer | the system's player service (`MediaPlayer` sits on it) | plays finished files only |

### Round 1: OpenMAX AL from memory (`hyperberry/tools/h264gate/h264gate.c`)

Test streams: 320x240@15 and 640x480@30, baseline, `ffmpeg testsrc`, run as root on the Q10. **No frame decoded.**

| Source | Sink | Result |
|---|---|---|
| `XA_DATALOCATOR_QNXBUFFERQUEUE` + `XADataFormat_QNXEncoded` (H.264) | Screen buffer queue, native display | `CreateMediaPlayer`: CONTENT_UNSUPPORTED (every variation of config, profile, level) |
| Android buffer-queue locator + the QNX encoded format | both | created; `Realize`: CONTENT_UNSUPPORTED (slog2: "No video stream found", "can't finish building the graph") |
| the same, realized asynchronously | Screen buffer queue | FEATURE_UNSUPPORTED |
| Android buffer queue + MIME `video/avc` and the like | Screen buffer queue | `Realize` blocks, then CONTENT_UNSUPPORTED ("no parser" for raw H.264) |
| Android buffer queue + MPEG-TS (one PES a chunk) | Screen buffer queue | `Realize` never returns: the probe wants 512 KB, then 1 MB, and spins |
| Android buffer queue, no source format | Screen buffer queue | a crash inside libOpenMAXAL |

The tool's flags (`-t` MPEG-TS, `-F <mime>|null`, `-E` enqueue before Realize, `-e` EOS early, `-R/-A` Realize orders, `-l q|a`, `-s bq|nd`) cover the variations in the table.

What was learned: the decoder exists. `/base/lib/dll/mmedia/e7_video_decoder.so` wraps OpenMAX IL (`libomxctx`), has H.264/AVC1 inputs and uses the resource arbiter, "so it is most likely the hardware one". Through AL its buffer-queue sources do not expose a video stream to the decoder search. Two copies of the probe hung and were killed on the phone (against the agent's brief; nothing else touched).

### Round 2: the URI route, the BBTube way (`h264uri.c`)

[BBTube](https://github.com/BBerryLife/bbtube) (`doctorrokter.BBTube`) plays H.264 through OpenMAX AL: its `libbbplayer.so` remuxes with FFmpeg and plays a URI into the QNX video buffer-queue sink, with no buffer-queue source (its strings show `fmp4gen` and `bbplayer_open_segments`). That library is not in this workspace's `bbtube/` folder; everything here comes from `phase7.md`.

```c
XADataLocator_URI lu = {XA_DATALOCATOR_URI, (XAchar *)uri};
XADataFormat_MIME fm = {XA_DATAFORMAT_MIME, (XAchar *)"video/x-matroska", XA_CONTAINERTYPE_UNSPECIFIED};
XADataLocator_QNXScreenBufferQueue ls = {XAQ_DATALOCATOR_SCREENBUFFERQUEUE, NULL, NULL};
```
(`hyperberry/tools/h264gate/h264uri.c`)

- **A finished Matroska file works, in hardware.** Sink `XAQ_DATALOCATOR_SCREENBUFFERQUEUE` with `XA_IID_QNXVIDEOBUFFERQUEUESINK`: each frame a Screen buffer (format 65548) with its timestamp. Realize 250-500 ms, first frame 100-115 ms after Play. Decoding in the system's `videoCore`: about 2 ms a frame at 320x240, 3.5 ms at 640x480. The player holds back the file's last 16-32 KB until 32 KB of padding follow.
- **Live is not possible with low latency:** a growing file stalls after the frames there at open (its size is read once); a FIFO blocks Realize; a local HTTP stream is walked to its end (Range requests) before Realize returns. One finished file per GOP with a fresh player each decoded everything at 320x240@15 with 1 s files, but 1-3 s behind, at about 25 % of a core in our process plus 15 % in `videoCore`; shorter files or 640x480 fell behind real time (a player's set-up outlasts its file).
- Verdict after round 2: hardware decode is real; no live path fit for a call. Left: BBTube's fragmented-MP4 segments with the next player realized ahead, OpenMAX IL directly, and a hint from BBTube's developer (asked for, no answer in the sources).

### Round 3: OpenMAX IL directly (`h264il.c`): it works

Table from `phase7.md` (Q10, as root, 2026-11-07):

| | 320x240@15 | 640x480@30 |
|---|---|---|
| first picture after the first frame in | 40 ms | 39 ms |
| per frame, real time (mean / max) | 15 / 40 ms | 24 / 54 ms |
| as fast as it goes | 124 fps | 54 fps |
| CPU, ours / `videoCore`, real time | about 5 % / 10 % of a core | about 9 % / 20 % |

Component `OMX.qcom.video.decoder.avc` (`libmmiVdec_asic_8960.so`, the MSM8960's codec driver loading its firmware in `videoCore`), `nVersion` 1.1.2.0. The process reaches the hardware "through connections to `videoCore`, not device files, the libraries are world-readable". The output is Qualcomm's tiled NV12 (`0x7f000004`, 64x32 tiles, 384x256 buffers for 320x240), de-tiled in C in 0.7 ms (320x240) or 2.2 ms (640x480) a frame. A wrong declared size costs a 270 ms output reconfiguration, losing nothing.

### Conclusion and what stayed untried

- Use IL for frames from memory, AL/`MediaPlayer` only for finished files.
- Left untried at the end of the gate: loss and resynchronisation, a resolution change mid-stream, other profiles. The calls later covered them (section 4).
- The app proved the decoder is reachable from an unsigned app's sandbox: "Every IL step ok in the sandbox, an NV12 window accepted" (Q10, 0.0.1.190, 2026-11-07).

## 4. The OpenMAX IL decoder that works

### Build and load

- Headers: Khronos IL 1.1.2 as AOSP carries them, fetched at build time by `hyperberry/scripts/fetch-omxil.sh` into `lib/omxil/` (pinned to a commit, sha256-checked, not committed). `hyperberry/app/hyperberry.pro` adds `INCLUDEPATH += $$PWD/../lib/omxil`. **Nothing links**; the core is opened at run time:

```cpp
static const char *ComponentName = "OMX.qcom.video.decoder.avc";
// The system's core first, so a different working directory or library path can't shadow it.
static const char *CorePath = "/base/lib/libOmxCore.so";
```
(`hyperberry/app/src/VideoDecoder.cpp`) It `dlsym`s `OMX_Init`, `OMX_Deinit`, `OMX_GetHandle`, `OMX_FreeHandle` and keeps the library loaded for the process's life.
- Tengram carries the same files (`tengram/app/src/VideoDecoder.cpp`, `tengram/app/tengram.pro`).
- The sources name no extra bar permission for the decoder. The descriptor of both apps has `use_camera`, `record_audio`, `play_audio`, `run_when_backgrounded` (`hyperberry/app/bar-descriptor.xml`).

### Which process

The UI process, never the headless engine. Decoding, encoding and drawing are in the UI; the engine carries compressed frames "a few KB each" (`hyperberry/docs/plan/phase7c.md`, Notes). Reasons in the sources: the screen and camera belong to the UI, and the engine runs under a memory cap (`hyperberry/docs/plan/phase7.md`, "The user's calls"). Frames travel on a third loopback connection (`CallVideo.h`, `hyperberry/docs/protocol2.md`). Tengram says the same: "the phone's H.264 is hardware and lives in the UI process" (`tengram/NOTES.md`, Video calls).

### Sequence (as `VideoDecoder::setUp` does it)

| Step | Call | Note |
|---|---|---|
| 1 | `OMX_Init`, `OMX_GetHandle(name, this, &callbacks)` | callbacks `{ onEvent, onEmptied, onFilled }` |
| 2 | `GetParameter(OMX_IndexParamVideoInit)` | `nStartPortNumber` = input port, output = input + 1 |
| 3 | `SetParameter(OMX_IndexParamPortDefinition)` on the input | `nFrameWidth/Height`, `xFramerate = fps << 16`, `eCompressionFormat = OMX_VIDEO_CodingAVC`, `nBufferSize` raised to the largest access unit + 4 KB (rounded to 4 KB) |
| 4 | `SetParameter(OMX_IndexParamVideoPortFormat)` on the input | AVC, same rate; failure is only logged |
| 5 | read the output port definition | width, height, `nStride`, `nSliceHeight`, `eColorFormat`, `nBufferCountActual` (capped at 40), `nBufferSize` |
| 6 | `SendCommand(StateSet, Idle)` | then **allocate every buffer of both ports** with `OMX_AllocateBuffer` while the transition is pending |
| 7 | wait for Idle (3 s), `SendCommand(Executing)`, wait | |
| 8 | `FillThisBuffer` on every output buffer | |

Declare the size the stream really has: a wrong one costs the 270 ms output reconfiguration, handled (`VideoDecoder.h`).

### Feeding (`feed()`)

The decoder wants SPS and PPS as a buffer of their own first, flagged `OMX_BUFFERFLAG_CODECCONFIG`, timestamp 0; the access unit that carries them then goes in whole (the gate did the same). Nothing is accepted before the first SPS/PPS: `feed()` returns false and the caller waits for the next IDR. Each unit is one `EmptyThisBuffer` with `OMX_BUFFERFLAG_ENDOFFRAME` (plus `STARTTIME` on the first). `feed()` waits up to 200 ms for a free input buffer and refuses a unit bigger than the buffer. Units are Annex-B; `VideoDecoder::splitAccessUnits` (`VideoFormat.cpp`) cuts a stream at a slice followed by a slice, SEI, SPS, PPS or delimiter, so one slice per picture (a multi-slice stream would need `first_mb_in_slice`).

### Output, tiles, stride and crop

- Output format is Qualcomm's `0x7f000004` (`TiledNV12`) or plain `21` (`LinearNV12`); any other is logged as a warning. `VideoDecoder::Frame` carries `width`, `height`, `stride`, `sliceHeight`, `colorFormat`, `timestampUs`: "the tiles follow width and height" (`VideoDecoder.h`).
- Tile layout, "found by trying": `VideoFormat.cpp` `tilePos`, 64x32 tiles of 2048 bytes in an interleaved order two rows apart; the chroma plane follows the luma plane, which is padded to 8 KB. `detile()` checks the buffer is long enough and copies to a linear NV12 with the caller's strides.
- Linear NV12: the chroma plane follows `sliceHeight` luma rows (`VideoWindow::toLinear`).
- A **crop-only** change (`OMX_EventPortSettingsChanged`, `data2 = OMX_IndexConfigCommonOutputCrop`) keeps the port: re-read the definition. Any other change reconfigures (below).

### Reconfiguration on a resolution change

```cpp
bool ok = step("SendCommand(PortDisable, output)", OMX_SendCommand(m_handle, OMX_CommandPortDisable, m_outPort, NULL));
if (!waitOutputReturned(1000))
    qWarning("video decoder: output buffers not all returned in 1 s; freeing them anyway");
for (int i = 0; i < m_outHeaders.size(); i++)
    OMX_FreeBuffer(m_handle, m_outPort, m_outHeaders[i]);
```
(`hyperberry/app/src/VideoOutput.cpp`) Then wait for the disable (2 s), re-read the port, `PortEnable`, allocate again, wait for the enable (2 s), `FillThisBuffer` all. Why the buffers must come back first: a peer's stream went 320x180 then 320x212; the second reconfiguration freed buffers the decoder still held, the port never disabled (`output port not disabled in 2 s`, `OMX_ErrorIncorrectStateOperation`), the engine had 149 frames and the decoder gave 3 (Q10, 0.0.1.202, 2026-11-08). Fixed in 0.0.1.203: 65 in, 65 out. A reconfiguration that still fails sets `broken()`; `CallVideo` closes and reopens the decoder and asks the engine for a keyframe. The reconfiguration itself costs about 50 ms in calls (Android switching 320x212, 480x320, 640x424 every 6-12 s, nothing refused: `phase7c.md`, 0.0.1.210) and 270 ms when the declared size was wrong.

### Threads and callbacks

- IL work runs on the caller of `open()/feed()/close()` (never the UI thread) plus **one output thread** (`outputLoop`).
- `onFilled` only queues the buffer and wakes the thread: "the component's thread must not wait for the sink, and FillThisBuffer is called again from the output thread". The sink's `frame()` runs on the output thread, in order, and the buffer returns to the decoder only when it returns: keep it short.
- All shared state sits under one mutex with one `QWaitCondition`; `MaxBuffers = 40` caps both ports.
- Errors: `OMX_EventError` is counted and the first 5 logged by name (`errName`).

### Teardown rules

Order in `close()`: stop the output thread, `SendCommand(Idle)` and wait 3 s, `SendCommand(Loaded)`, `OMX_FreeBuffer` on every input and output header, wait for Loaded (3 s), `OMX_FreeHandle`, `OMX_Deinit`. `close()` is safe twice. Never free output buffers the decoder still holds (above).

### Measured (all Q10 unless noted)

| What | Number | Build, date |
|---|---|---|
| 640x480@30 baseline clip in the app | 300 in, 300 out, 30.0 fps, first frame 39 ms, latency 24.3 mean / 53.4 max ms, de-tile 1.6 ms, 19 % of a core for the whole app | 0.0.1.190, 2026-11-07 |
| RGB conversion instead of NV12 | 60-90 % of a core, so NV12 stays | 0.0.1.191 |
| NV12 under the hole vs RGB | 17-20 % of a core against 43-90 % | `phase7b.md` "Video, later" |
| 4 decoders at once, each 640x480@30 into its own window | every one 300/300, 30.0 fps, first frame 32-41 ms, latency 16-26 ms mean (max 64), de-tile about 1 ms, process about 40 % of one core | 0.0.1.259, 2026-11-11 (test removed 2026-11-12; in git at `6401d93`) |
| Android's High profile (CABAC) and sizes 320x176, 480x240, 640x352, 640x360 by reconfiguration | accepted, about 55-65 ms decode latency, UI process 7-14 % of a core with camera and decoder running | Classic, Tengram 0.1.0.49, 2026-11-19 |

Profiles: baseline was what the gate tested. Tengram offered constrained baseline only, and the Android peer sent **High with CABAC**; the decoder takes it (`tengram/NOTES.md`, Gate V1 and V2). Other profiles are not listed in any source.

### How many at once

Four, measured (row above); the group-video grid is limited to 4 tiles by that test (`hyperberry/docs/plan/phase7e.md`, V0). The code has one `VideoDecoder` and one `VideoWindow` per tile (`CallVideo.h`, `Tiles = 4`). A comment in `VideoDecoder.cpp` still says "the core stays loaded for the life of the process (no dlclose); one decoder at a time", written at 0.0.1.190; phase 7e's V0 (0.0.1.259) is newer and shows four. The newer one wins.

## 5. Drawing decoded frames

`VideoWindow` (`hyperberry/app/src/VideoWindow.cpp`) implements `VideoDecoder::Sink`. It de-tiles each frame straight into the window's buffer, on the decoder's output thread, then posts.

```cpp
if (screen_set_window_property_cv(m_win, SCREEN_PROPERTY_ID_STRING, id.size() + 1, id.constData())
    || screen_join_window_group(m_win, group.constData())) {
```
```cpp
const int dirty[4] = { 0, 0, m_w, m_h };
if (screen_post_window(m_win, buf, 1, dirty, 0)) {
```

| Setting | Value | Why (source comment) |
|---|---|---|
| Window type | `screen_create_window_type(..., SCREEN_CHILD_WINDOW)` | a child the control can adopt |
| Format | `SCREEN_FORMAT_NV12`, else `SCREEN_FORMAT_RGBX8888` with a C conversion | NV12 accepted on the Q10 |
| Usage | `SCREEN_USAGE_WRITE \| SCREEN_USAGE_NATIVE`, 2 buffers | write-only from the CPU, the compositor reads in place |
| Id | `SCREEN_PROPERTY_ID_STRING` = the control's `windowId`, set **before** `screen_join_window_group(main window's groupId())` | "an id set after never got the window adopted (0.0.1.191: unsized, full screen, nothing in the control)" |
| Z-order | `-1` (under Cascades' hole) | `+1` also tried; under the hole is the one kept |
| Focus | `SCREEN_SENSITIVITY_NO_FOCUS` | a tap in the child takes keys from Cascades |
| Post | `screen_post_window` with a full dirty rectangle | "with no rectangle Screen may take it as nothing changed (the window in front showed one frozen picture, 0.0.1.191)" |
| Render buffers | read into room for both | "Screen writes them all (one variable took two before)" |
| Size change | `screen_destroy_window_buffers`, set `BUFFER_SIZE` and `SOURCE_SIZE`, `screen_create_window_buffers(2)` | the window follows the (turned) frame size |
| Crop to the control | Screen's source viewport (`SOURCE_POSITION`, `SOURCE_SIZE`) set by `centreCrop` | a bigger control than its parent is squeezed by Cascades (0.0.1.204: the picture was squeezed); the control fills the frame and the viewport shows the centred part |

Rotation: `setRotation(quarterTurns)` clockwise, from the next frame; `Nv12Rotate.h` (Qt-free, tested on the Mac by `hyperberry/app/tests/rotate_test.cpp`) turns linear NV12 into the buffer, reading the destination in order because "the window's buffer is write-only to the CPU and a scattered write is the slow one". The tiles do not survive a turn, so the frame is de-tiled to a scratch buffer first. WhatsApp's quarter turns are anticlockwise: `CallVideo` uses `(4 - o) & 3` (Q10, 0.0.1.207: "the picture orientation is right"). Telegram's are clockwise: no reversal (`tengram/NOTES.md`, Gate V2). The RGB fallback draws unturned.

The first picture did not show for several builds. Causes found, in order: the page's own black background sat over the hole (0.0.1.190); the window was not adopted because the id was set after the group join, and its posts did not refresh it (0.0.1.191, 192). Fixed in 0.0.1.192 (Q10: "the test pattern plays, in NV12 and in RGB").

### Native-window traps (ForeignWindowControl and Screen windows)

From `base-bb10/skills/bb10-pitfalls/SKILL.md` unless noted; all [Q10].

- Binding needs `SCREEN_CHILD_WINDOW`, `screen_join_window_group(main group id)` after the control exists, `SCREEN_PROPERTY_ID_STRING` = the control's `windowId`.
- The SDK says put it at a negative z-order under Cascades' hole. At z -1 the page shows but gets **no touches** (Cascades forwards only keys). Above Cascades it takes touches, and then **nothing Cascades draws can appear over it**: no SystemToast, action menu, text magnifier or overlay. Older text in `bb10-cascades/SKILL.md` ("draws above all Cascades content and takes touches") describes the second case only; the pitfalls skill is newer and wins.
- A window made by the **system player** (`MediaPlayer`) or by `VideoWindow` at z -1 sits under the hole: Cascades controls draw over it and taps on them work (`stories.md`). A **camera viewfinder** window sits above Cascades: nothing Cascades draws can cover it; keep overlays out of its rectangle (hyperberry hides the self-view at the ear for that reason: `CallVideoView.qml`).
- A native window **follows a Cascades `translationX`** on an ancestor of its control (`updatedProperties: WindowProperty.Position`): a page sliding 548 px carried its video windows smoothly (0.0.1.265).
- A full-screen overlay swallows every touch under it, even where it draws nothing: give its root `overlapTouchPolicy: OverlapTouchPolicy.Allow`.
- Screen gives the keyboard to the window last touched: set `SCREEN_SENSITIVITY_NO_FOCUS` on the child (touches still work) and never `navigator_request_events` in the child.
- Workarounds that shipped: clip the child (`SCREEN_PROPERTY_CLIP_POSITION`/`CLIP_SIZE`, one rectangle, window pixels); hide the control while a menu, sheet, dialog or text edit is up and show a **snapshot** in an `ImageView` (`screen_create_pixmap`, `SCREEN_USAGE_NATIVE | SCREEN_USAGE_READ`, `screen_read_window`; Screen's RGBX is B,G,R,X in memory, `bb::ImageData` is R,G,B,X: swap bytes 0 and 2).
- Anything left behind the window shows through whenever it hides; hide the control when the child exits. `windowHandle()` is valid only after `boundToWindow`.

## 6. The hardware encoder

`CameraEncoder` (`hyperberry/app/src/CameraEncoder.cpp`) uses the **camera API's own H.264 encoder**, not OpenMAX: `camera_start_encode()` hands compressed frames to a callback. Link `-lcamapi`, permission `use_camera` (`EACCESS (is use_camera granted?)` is the error text for a missing one).

```cpp
camera_set_video_property(m_handle,
    CAMERA_IMGPROP_WIDTH, w, CAMERA_IMGPROP_HEIGHT, h, CAMERA_IMGPROP_FRAMERATE, rate,
    CAMERA_IMGPROP_ROTATION, rotation,
    CAMERA_IMGPROP_VIDEOCODEC, CAMERA_VIDEOCODEC_H264,
    CAMERA_IMGPROP_AUDIOCODEC, CAMERA_AUDIOCODEC_NONE)
camera_set_videoencoder_parameter(m_handle,
    CAMERA_H264AVC_PROFILE, CAMERA_H264AVC_PROFILE_BASELINE,
    CAMERA_H264AVC_BITRATE, bitrate,
    CAMERA_H264AVC_KEYFRAMEINTERVAL, fps * keyframeSeconds,
    CAMERA_H264AVC_RATECONTROL, CAMERA_H264AVC_RATECONTROL_CBR)
```

| Item | Fact |
|---|---|
| Open | `camera_open(CAMERA_UNIT_FRONT or REAR, CAMERA_MODE_RW)`; `camera_set_vf_mode(VIDEO)`; blocks (seconds), so never on the UI thread |
| Viewfinder | `camera_set_vf_property` with `CAMERA_IMGPROP_WIN_GROUPID` and `WIN_ID`: the camera library makes the window in our group for a `ForeignWindowControl` of that id; `camera_start_viewfinder` before `camera_start_encode` |
| Rotation | the property is 0 (the sensor's own); `camera_get_native_orientation` / 90 = quarter turns sent as our orientation. `CalibrationTurns = 0` is the knob if the peer sees us sideways; not needed on the Q10 or the Classic (native orientation 0, pictures upright both ways, front and rear: Classic 2026-11-19) |
| Mirroring | none applied. The front camera goes out as is; WhatsApp "sends the front camera unmirrored". A mirror added in 0.0.1.206 was "wrong and reverted" (the picture had been turned the wrong way) |
| Stream | `CAMERA_VIDEOCODEC_H264` is **Annex-B** (the SDK header says AVC1 would be length-prefixed). The callback parses NAL types (7 SPS, 8 PPS, 5 IDR, 1 slice, 6 SEI, 9 delimiter), keeps SPS/PPS and puts them before **every IDR** if the camera did not; parameter sets alone are held back |
| Keyframes | `camera_force_video_keyframe()` on a peer's request, only from the thread that starts and stops |
| Bit rate change | `camera_set_videoencoder_parameter(CAMERA_H264AVC_BITRATE, n)` while encoding: **the camera takes it** (400 to 600 kbit/s, EOK, output followed; Classic, Tengram 2026-11-19) |
| Timestamps | the camera's monotonic clock, in microseconds from the first frame |
| Stats | frames, keyframes, dropped (`CAMERA_STATUS_FRAME_DROPPED`), interval and age of callbacks |

Settings by build (newest wins):

| Where | Size and rate | Bit rate | IDR |
|---|---|---|---|
| HyperBerry `CallVideo.cpp` (2026-11-20 comments) | steps 1280x720 (cap 1000 kbit/s), 640x480 (600), 320x240 (300); 30 fps, 15 at the lowest step; the camera starts at 640x480; optional 352x640 (9:16) crop | the peer's allowance, at least 100 kbit/s, 2/3 of the cap before any allowance; changed at most every 2 s and only by 15 % | every 4 s ("at 128 kbit/s an IDR every 2 s took most of the budget") |
| HyperBerry 0.0.1.193-195 (older) | 640x480@15 | 400-500 kbit/s | every 2 s |
| Tengram `CallVideo.cpp` | same steps, 15 fps | same rule | every 2 s |

Step logic (both apps): down a step when the allowance stays below the step's floor 2 s (700 kbit/s at 1280x720, 250 at 640x480), no step down in the first 10 s (Android's allowance starts low and ramps), up when it stays at the next step's entry 4 s (HyperBerry; Tengram's was 8 s), one size change per 8 s. A size change restarts the camera. The Q10's front camera lists 352x640 as its own 9:16 size (`CallVideo.cpp`).

Evidence: the camera loopback (camera, encoder, decoder, screen, no network) "camera works" on the Q10, 0.0.1.193, 2026-11-07, with no numbers read; its `camera test:` lines were not in `data/logs/ui.log`. 1:1 and group calls ran on the Q10; the encoder at 1280x720 about 1000 kbit/s 15 fps and mid-encode rate changes on the Classic (Tengram, 2026-11-19). A step down on a throttled link was "still to see" in that note.

Traps: the camera library creates the viewfinder window itself (above Cascades, section 5); a requested size the camera does not have is logged as "SIZE DIFFERS" and then decoder and window mismatch; the sink callback runs on the camera library's thread and must be short; `requestKeyframe`/`setBitrate` take no lock on the handle.

## 7. Audio

### PCM through libasound and audio_manager

Link `-lasound -laudio_manager`, headers `<audio/audio_manager_routing.h>`, `<sys/asoundlib.h>` (`hyperberry/app/hyperberry.pro`, `CallAudio.cpp`). Both apps open the device the same way:

```cpp
rc = audio_manager_snd_pcm_open_name(CallAudioType, &h, &am, names[i],
                                     capture ? SND_PCM_OPEN_CAPTURE : SND_PCM_OPEN_PLAYBACK);
```
(`hyperberry/app/src/CallAudio.cpp`; BBMusic: `AUDIO_TYPE_MULTIMEDIA`, name `pcmPreferred`, `bbmusic/engine/src/audio/qsa_sink.cpp`)

| | HyperBerry / Tengram call (UI process) | BBMusic music (engine) |
|---|---|---|
| Audio type | `AUDIO_TYPE_VOICE` | `AUDIO_TYPE_MULTIMEDIA` |
| Device name | `voice` (the Q10 has `/dev/snd/voicec`, `voicep`), else `pcmPreferred` | `pcmPreferred`; `a2dpp` when the default device is A2DP |
| Format | 16 kHz mono S16_LE, 60 ms frames of 1920 bytes | 48 kHz stereo S16_LE (Opus decoder output) |
| Mode | `SND_PCM_MODE_BLOCK`, `START_DATA`, `STOP_STOP` | `BLOCK`, `START_FULL`, `STOP_STOP` (an underrun stops it so it can be counted) |
| Fragment | asked 1024 bytes, `frags_max` 8 | asked 4096 bytes, `frags_max` 8; the device gave 4096 x 9 at 48 kHz, 2 voices |
| Route | `audio_manager_set_handle_type(handle, VOICE, AUDIO_DEVICE_SPEAKER or AUDIO_DEVICE_DEFAULT, AUDIO_DEVICE_DEFAULT)` | the system's default device, watched twice a second |

Calls, lessons: the first real call gave 1024-byte fragments, not the 1920 asked; playback ran dry about once a second and the microphone gave short reads (512, once -22). 0.0.1.187 reads and writes one granted fragment at a time, gathers the microphone into 60 ms frames keeping short reads, starts playback with two frames in (120 ms, again after running dry without dropping what hasn't played), drops the oldest beyond four frames "so the delay can't grow", queues up to 8 fragments. A PCM failing 50 reads or writes in a row is given up on. **Calls and audio work on the Q10** (0.0.1.187, 2026-11-07, user); no audible echo on the speaker with `AUDIO_TYPE_VOICE`, so `AUDIO_TYPE_VIDEO_CHAT` (the fallback named in the code) was not needed. Tengram: with all WebRTC echo, noise and gain processing off "BB10's voice path does the echo" (Q10, 0.1.0.39, 2026-11-19, user).

Music, lessons (`bbmusic/docs/plan/phase0.md` 0.4): ten minutes of 48 kHz Opus: **0 underruns, 5.8 % of a core** for the whole process, the decoder alone 78x real time (10x while playing; the phone clocks the CPU down between packets). As the headless app 157 s with 0 underruns at 5.6 % of a core, through the app being swiped away (Q10, 2026-11-14, user). **Integer-math lesson:** scaling samples with `std::lrint` cost 18.6 % of a core for the same playback; an integer multiply and shift cost 5.7 %. "No libm call and no allocation per audio sample" (`bbmusic/docs/plan/phase1.md`). The shipped equalizer (`equalizer.cpp`) still computes biquads in `float` per sample and clamps with `std::lrintf`; its cost was not measured in the sources read.

Routing in the engine (`QsaSink`): a stream stays on the output it was opened on (Q10, 2026-11-14: headphones took effect only after a stop and start), so `check_route` reads `audio_manager_get_default_device()` every half second of written audio; a change to the speaker from headset, headphone or A2DP is "unplugged" (stop and report), any other change closes and reopens the PCM and plays on. Headphones pass (Q10, 2026-11-14). **Bluetooth:** `pcmPreferredp` stays on the codec card; A2DP is its own card behind `a2dpp`; the sink opens it when the default device is A2DP. Verified on the Q20 (2026-11-15, 0.1.0.43): "speaker: a2dpp (a2dp)" on a play, a mid-song reopen, Bluetooth off pauses at 9153 ms and play went on on the speaker; user: "it yes it works". `bbmusic/docs/plan/status.md` (2026-11-14) still lists Bluetooth as untested; the commit is newer and wins. A phone call during music: untested.

### Earpiece, speaker, proximity

- `CallAudio::setSpeaker` re-routes both handles under a mutex. Default is the handset (earpiece).
- `Proximity` (`hyperberry/app/src/Proximity.cpp`): BPS sensor domain, `sensor_request_events(SENSOR_TYPE_PROXIMITY)`, `near` when the reading is below 0.5 (0.0 close, 1.0 far). `acquire(holder)` / `release(holder)` count holders; a holder destroyed while it holds is released by itself (Cascades QML has no `Component.onDestruction`). While held it sets `ScreenIdleMode::KeepAwake`: "an idle lock during a call would minimise the app and drop the audio". The call sheet covers itself in black at the ear unless on the speaker.
- A voice note at the ear: `MediaPlayer.audioManagerHandle` set to `proximity.earHandle()` (an `AUDIO_TYPE_VOICE` handle routed to `AUDIO_DEVICE_HANDSET`) and back to 0; `settings.earSensor` turns it off "for a sensor that misreads". Older `ui.md` says "the speaker only, no earpiece" for notes; phase 7.5 (2026-11-07) supersedes it. Phone result for 7.5: the first build crashed the chat page (`Component.onDestruction`), fixed in 0.0.1.181; no later report found.
- Bluetooth headsets for calls: not in the sources.

### Opus, voice notes, ringtones

| What | Where | Numbers and facts |
|---|---|---|
| Voice-note recording | `AudioRecorder` in `Recorder.qml` | writes a **48 kHz stereo 16-bit PCM WAV**, no rate or format to set (Q10, 2026-10-29). Tap to start, tap to send; under 1 s is dropped; the screen stays awake while recording (Q10, 0.0.1.94) |
| Encode | the Go engine, libopus 1.6.1 static, `--disable-rtcd` + `-mfpu=neon` (QNX has no runtime CPU detection) | 20 kbit/s mono, complexity 5; encode 0.117 of real time (764 ms for 6.5 s), decode 79 ms for 6.5 s (Q10, 2026-10-29); encoding runs while recording |
| Play a note | engine decodes Ogg Opus to `data/cache/voice/<id>.wav` on first play; `MediaPlayer` plays it | a voice note decoded whole once took 31 MB and ran the capped engine out of memory (`hyperberry/docs/plan/pitfalls.md`); decode in pieces |
| Call codec | MLOW in meowcaller (Go) in the engine | encode 1.23-1.30 of real time as published, **0.415** after caching the FFT twiddles (`engine/meowcaller.patch`), decode 0.02 (Q10, 2026-11-07). `math.Cos`/`math.Sin` per call were the cost: float64 to uint64 is a software routine on 32-bit ARM. Same lesson as BBMusic's `lrint` |
| Tengram calls | tgcalls, Opus 48 kHz, 60 ms packets | about 25 % of one core for the whole phone during a call; libopus built `--enable-fixed-point` (Q10, 2026-11-19) |
| Ringtone | engine, mm-renderer (`libmmrndclient`, `libstrm`), audio type `ringtone` | the phone's tone from `cat.phone` in PPS; mm-renderer's `repeat` did not loop, so the engine restarts it; rings only when `/pps/services/notify/status` says `sound::on` (Q10, 0.0.1.185-186). The system's own ringing needs `use_notify_system`, granted to `sys.*` apps only |

### Where audio lives, and why both choices work

| App | Where | Why (source) |
|---|---|---|
| HyperBerry, Tengram calls | UI process; the engine keeps codec and signalling; PCM crosses a second loopback connection | "the engine runs headless under a memory cap (`SetMemoryLimit(24 MB)`; four out-of-memory crashes came from single 9 MB allocations), so the microphone, the speaker and their buffers belong to the UI process, which has no such cap" (`hyperberry/docs/plan/phase7.md`). A link that closes during a call, with no new one within 3 s, hangs the call up: the UI was closed and nothing can play or record |
| BBMusic | the headless engine opens the speaker | gate 0.4 passed: a headless entry point may open the speaker and outlives the UI. The decoder is small and the engine's size is 4.2 MB (`status.md`), so the cap does not bite. If 0.4 had failed the plan was the HyperBerry arrangement and "music stops when the app is closed" (`bbmusic/PLAN.md` 9) |

The first is for **calls**, which need the UI's camera and screen anyway and are allowed to end with the UI; the second is for **music**, which must survive it. Memory cap and addon: [09-headless-service.md](09-headless-service.md).

## 8. Now-playing, the volume overlay and media keys (BBMusic gates 0.5, 0.6)

- BB10 shows no player on the lock screen. The controls are in the **volume overlay**, shown when a volume key is pressed.
- `bb::multimedia::NowPlayingConnection` is only a client of a PPS service, `/usr/sbin/nowplaying`, object `/pps/services/multimedia/mediaplayer/control`; it dies with the UI, so with the app closed the overlay has no player (Q10, 2026-11-14, second round).
- **A plain process with no Qt can speak to it, and the overlay's buttons reach it** (Q10, 2026-11-14, with the user: "ok it flips, pressed all of them"). So the engine owns now-playing and the buttons work with the app closed. Messages, each answered `res::<msg>` with the same `id`, or `err::badparam`:

```
msg::register
id::1
dat:json:{"name":"bbmusic-probe","prio":"low","audio":"general","overlay":"fancy","pid":<pid>}

msg::acquire
id::2

msg::metadata
id::3
dat:json:{"title":"…","artist":"…","album":"…","duration":600000,"position":0,"nextEnabled":true,"prevEnabled":true}

msg::state
id::4
dat::playing          (also paused, stopped; a JSON object here is refused)
```
(`bbmusic/docs/plan/phase0.md`, 0.5) In: `msg::track` with `dat::sendData` (the overlay opened: resend metadata), `pause`, `play`, `prev`, `next`, `holdData` (closed); `msg::revoke` when another player acquired. `audio` is required and undocumented. The glyph follows the `state` the player writes, so send `state` on **every** change (a gate app that did not saw the buttons out of step).
- Cover: `albumArtwork` is a `file://` path. The overlay service (uid 393, group `1000_shared`) cannot read the app's sandbox, so the engine copies the cover to `/accounts/1000/shared/misc/.bbmusic/` and the descriptor needs `access_shared` (`bbmusic/docs/plan/ui.md`, 0.1.0.34).
- The overlay showed the cover and artist line but never the title; since 0.1.0.36 the artist line is "Title – Artist" on speaker and headphones and plain fields over Bluetooth, where the same metadata reaches a car (AVRCP). Position is resent every second for the car. Untried in a car.
- The headset button with the app closed: untried; `audio_manager_add_device_button_change_event()` is a C candidate (`bbmusic/docs/plan/phase0.md`).
- `MediaKeyWatcher` (`bb::multimedia`, `-lbbmultimedia`) "misses the mute/play key on some devices" (`bb10-pitfalls` skill); none of the four apps uses it.

## 9. Not proven / open

| Item | Label |
|---|---|
| Decoding profiles other than baseline and High/CABAC; loss and resync on a bad network | untried (bad-network work, `phase7c.md` 7c.4, not built) |
| Camera encoder's own numbers (fps, bit rate, frame sizes, callback age) from a run | not recorded in `ui.log` (0.0.1.193); logs `call video:` every 10 s exist since |
| A step down on a throttled link | "still to see" (Tengram note) |
| Phoenix video on a phone | builds; no report found |
| Hidden timeline and "Open in player" for fragmented MP4 | untried on a phone (`tengram/docs/HANDOFF.md`) |
| Audio focus with a video playing | not in the sources |
| BBMusic: a phone call during playback, headset button with the app closed, the overlay on a car | untried |
| Bluetooth headset for calls | not in the sources |
| Several `VideoDecoder`s sharing `OMX_Init` | works for 4 (0.0.1.259); the `ponytail` comment says one |
| Voice note at the ear (7.5) | builds; no phone report found |
| Opus complexity 5 vs 9 in Tengram calls | "not verified yet" |

## Where sources disagree

- `phase7c.md` says the camera sends 640x480@15, 400 kbit/s, IDR every 2 s; `hyperberry/app/src/CallVideo.cpp` (2026-11-20) has 30 fps, steps and IDR every 4 s. The code wins.
- `tengram/docs/ui-fork.md` 3.3 says the video player classes "stay: received videos use it"; the viewer uses `MediaPlayer`, and `VideoDecoder` is used by `CallVideo` only (`tengram/app/src/`).
- `bbmusic/docs/plan/status.md` lists Bluetooth untested; commit `5dc735d` (Q20, 2026-11-15) verified it.

## Sources

- `hyperberry/docs/plan/phase7.md` (video gate, calls, audio link, ringing), `phase7b.md`, `phase7c.md`, `phase7e.md`, `phase0.md` (0.8), `phase4.md`, `stories.md`, `status.md`, `pitfalls.md`
- `hyperberry/tools/h264gate/h264gate.c`, `h264uri.c`, `h264il.c`
- `hyperberry/app/src/VideoDecoder.h`, `VideoDecoder.cpp`, `VideoOmx.h`, `VideoFormat.cpp`, `VideoOutput.cpp`, `VideoWindow.h`, `VideoWindow.cpp`, `Nv12Rotate.h`, `CameraEncoder.h`, `CameraEncoder.cpp`, `CallVideo.h`, `CallVideo.cpp`, `CallAudio.h`, `CallAudio.cpp`, `Proximity.h`, `Proximity.cpp`
- `hyperberry/scripts/fetch-omxil.sh`, `hyperberry/app/hyperberry.pro`, `hyperberry/app/bar-descriptor.xml`, `hyperberry/app/assets/VideoViewer.qml`, `StoryViewer.qml`, `VoicePlayer.qml`, `Recorder.qml`, `CallVideoView.qml`
- `base-bb10/skills/bb10-pitfalls/SKILL.md` ("ForeignWindowControl and Screen windows"), `bb10-cascades/SKILL.md` ("Native windows")
- `tengram/NOTES.md` ("Calls", "Video calls"), `tengram/docs/ui-fork.md`, `tengram/docs/HANDOFF.md`, `tengram/app/assets/VideoViewer.qml`, `tengram/app/src/Mp4Length.h`, `tengram/app/src/CallVideo.cpp`; git commits `74bb400`, `f154d37`
- `phoenix/app/assets/VideoPage.qml`, `phoenix/engine/src/mp4/`, `phoenix/engine/src/media/video.cpp`, `phoenix/docs/protocol.md`
- `bbmusic/PLAN.md` (sections 2, 4.3, 4.4, "If the PCM path fails its gate"), `bbmusic/docs/plan/phase0.md` (0.4-0.6), `phase1.md`, `status.md`, `ui.md`, `bbmusic/engine/src/audio/qsa_sink.cpp`, `equalizer.cpp`, `bbmusic/engine/src/platform/now_playing.cpp`, `np_protocol.cpp`; git commit `5dc735d`
