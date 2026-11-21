# samples/video

Video decoding, camera encoding, playback and remuxing. Not built in this repo; read as reference. Docs: [12-video-and-audio.md](../../docs/12-video-and-audio.md).

| File | From | Kind | Shows |
|---|---|---|---|
| VideoDecoder.h, VideoDecoder.cpp | hyperberry `df45afc` app/src | verbatim | OpenMAX IL H.264 decoder, dlopen of the Qualcomm component |
| VideoOmx.h | hyperberry `df45afc` app/src | verbatim | OMX IL declarations used by the decoder |
| VideoFormat.cpp | hyperberry `df45afc` app/src | verbatim | Output format and color handling |
| VideoOutput.cpp | hyperberry `df45afc` app/src | verbatim | Frame output path |
| VideoWindow.h, VideoWindow.cpp | hyperberry `df45afc` app/src | verbatim | Screen window for decoded frames |
| Nv12Rotate.h | hyperberry `df45afc` app/src | verbatim | Qt-free NV12 rotation |
| CameraEncoder.h, CameraEncoder.cpp | hyperberry `df45afc` app/src | verbatim | Hardware camera H.264 encoder (camapi) |
| CallVideo.h, CallVideo.cpp | hyperberry `df45afc` app/src | verbatim | Call video glue, bit-rate steps |
| fetch-omxil.sh | hyperberry `df45afc` scripts | verbatim | Fetches the Khronos OMX IL headers at build time |
| VideoViewer.qml | tengram `5a65cdf` app/assets | verbatim | Full-screen MediaPlayer into a ForeignWindowControl |
| Mp4Length.h | tengram `5a65cdf` app/src | verbatim | MP4 length read for fragmented files |
| h264gate/h264gate.c, h264uri.c, h264il.c | hyperberry `df45afc` tools/h264gate | verbatim | The OpenMAX AL and IL gate programs |
| remux/mpd.h, mpd.cpp, remux.h, remux.cpp | phoenix `4e06969` engine/src/mp4 | verbatim | DASH manifest parse and remux to a seekable MP4 |
| remux/video.cpp | phoenix `4e06969` engine/src/media | verbatim | Video fetch, merge and remux in the engine |
| StoryViewer.qml | hyperberry `df45afc` app/assets | verbatim | Full-screen story viewer with progress segments |
| CallVideoView.qml | hyperberry `df45afc` app/assets | verbatim | Video call view: peer picture, own camera in a corner |
| rotate_test.cpp | hyperberry `df45afc` app/tests | verbatim | Frame rotation check run on the Mac |
| VideoPage.qml | phoenix `4e06969` app/assets | verbatim | Full-screen video through MediaPlayer into a ForeignWindowControl |
