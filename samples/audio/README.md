# samples/audio

Call audio, proximity, recording, the QSA sink and now-playing. Not built in this repo; read as reference. Docs: [12-video-and-audio.md](../../docs/12-video-and-audio.md).

| File | From | Kind | Shows |
|---|---|---|---|
| CallAudio.h, CallAudio.cpp | hyperberry `df45afc` app/src | verbatim | libasound capture and playback routed through audio_manager |
| Proximity.h, Proximity.cpp | hyperberry `df45afc` app/src | verbatim | Proximity sensor for the earpiece |
| Recorder.qml | hyperberry `df45afc` app/assets | verbatim | AudioRecorder voice notes |
| qsa_sink.h, qsa_sink.cpp | bbmusic `1d80813` engine/src/audio | verbatim | Headless engine opening the speaker through QSA |
| now_playing.h, now_playing.cpp, np_protocol.h, np_protocol.cpp | bbmusic `1d80813` engine/src/platform | verbatim | Volume-overlay now-playing through a PPS object |
| VoicePlayer.qml | hyperberry `df45afc` app/assets | verbatim | The chat page voice-note player: one note at a time |
| equalizer.h, equalizer.cpp | bbmusic `1d80813` engine/src/audio | verbatim | Five-band peaking equalizer on the PCM stream, by preset |
