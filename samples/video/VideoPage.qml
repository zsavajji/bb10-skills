import bb.cascades 1.4
import bb.multimedia 1.4
import bb.system 1.2
import phoenix 1.0
import "format.js" as Fmt

// A video, full screen on black, through MediaPlayer into a ForeignWindowControl (as tengram's
// viewer does). The engine prepares the file (`media`): while it does, the preview picture, a
// ring that fills, with a cross to cancel (which leaves a play mark to start again) and the
// megabytes under it; then it plays, with a timeline, play/pause and the time (a tap on the
// picture shows or hides them); a `gif` loops, is muted and has no timeline. Pushed by main.qml
// (router.go): the properties are set, then start() is called, after the push (the player's
// window is made when it prepares, and the control binds to it after).
// `media`/`ready` events are matched by url from the moment the request is sent, not after its
// reply: the engine may say "ready" before it has answered "pending".
Page {
    id: page
    property string media: ""
    property bool gif: false
    property string title: ""
    // Needs every horizontal drag (the tab's NavigationPane turns peeking off while this is on top).
    property bool noPeek: true
    property string preview: ""

    property string phase: "ask"        // ask, pending, ready, error, stopped (cancelled: the play badge)
    property string reqId: ""
    property string errorCode: ""
    property string errorMessage: ""
    property string file: ""
    property real done: 0
    property real total: 0
    property string previewFile: ""
    property real ratio: 1.7777
    property real knownMs: 0
    // The timeline is under a finger, or was let go and the player has not moved yet.
    property bool scrubbing: false
    property bool settling: false
    property real at: 0
    property bool ended: false
    property bool barShown: true
    property bool playing: player.mediaState == MediaState.Started
    property real dur: player.duration > 0 ? player.duration : page.knownMs
    property bool canSeek: player.duration > 0
    property bool seekAsked: false
    property real frac: page.scrubbing || page.settling ? page.at
                        : page.ended ? 1 : page.dur > 0 ? Math.min(player.position / page.dur, 1) : 0
    property real dot: ui.du(3)
    // The system's compact bar (Back and overflow only) is shown with our control bar between its
    // two buttons; always while it is made ready, on an error and for a gif.
    // chromeH and chromeSide are the compact bar's height and the width of one of its buttons,
    // measured on the Q10's screenshot (0.1.0.24, 2026-11-21): 108 px each, 12 du. With them the
    // control bar runs from the one button to the other, at their height.
    property real chromeH: ui.du(12)
    property real chromeSide: ui.du(12)
    property bool chromeShown: page.phase != "ready" || page.gif || page.barShown
    // When a touch on our own controls last began: the tap under them must not also toggle the bar.
    property real rowHit: 0
    actionBarVisibility: page.chromeShown ? ChromeVisibility.Compact : ChromeVisibility.Hidden

    function clock(ms) {
        var s = Math.floor(ms / 1000);
        var m = Math.floor(s / 60);
        s = s % 60;
        return m + ":" + (s < 10 ? "0" : "") + s;
    }

    function ask() {
        page.phase = "ask";
        page.errorCode = "";
        page.reqId = engine.request("media", {
            "url": page.media,
            "max_height": prefs.get("videoHeight", 480)
        });
        if (page.reqId == "") {
            page.fail("disconnected", "");
        }
    }

    function start() {
        page.previewFile = thumbs.file(page.preview);
        page.ask();
    }

    function fail(code, message) {
        page.phase = "error";
        page.errorCode = code;
        page.errorMessage = message ? message : "";
    }

    // The file is in the cache: play it. w, h and the duration may be 0 (the engine does not know):
    // the player says then.
    function ready(r) {
        if (page.phase == "ready" || !r.file) {
            return;
        }
        page.phase = "ready";
        page.file = r.file;
        if (r.w > 0 && r.h > 0) {
            page.ratio = r.h / r.w;
        }
        page.knownMs = r.duration_s > 0 ? r.duration_s * 1000 : 0;
        player.sourceUrl = "file://" + r.file;
        page.check(player.play());
        idle.start();
    }

    function check(e) {
        if (e != 0) {
            page.fail("player", qsTr("The player stopped (error %1).").arg(e));
        }
    }

    function onReply(id, ok, result, code, message) {
        if (id == "" || id != page.reqId) {
            return;
        }
        page.reqId = "";
        if (!ok) {
            page.fail(code, message);
            return;
        }
        if (result.state == "ready") {
            page.ready(result);
        } else if (page.phase == "ask") {
            page.phase = "pending";
        }
    }

    function onEvent(name, fields) {
        // Only while it is being made: a cancelled job's late "ready" or "error" is not heard.
        if (name != "media" || fields.url != page.media || (page.phase != "ask" && page.phase != "pending")) {
            return;
        }
        if (fields.state == "progress") {
            page.phase = "pending";
            page.done = fields.done;
            page.total = fields.total;
        } else if (fields.state == "ready") {
            page.ready(fields);
        } else if (fields.state == "error") {
            page.fail(fields.code, fields.message);
        }
    }

    function onThumb(url, file) {
        if (url == page.preview) {
            page.previewFile = file;
        }
    }

    function cancel() {
        if (page.phase == "ask" || page.phase == "pending") {
            engine.request("media_cancel", {
                "url": page.media
            });
            page.reqId = "";
            page.phase = "stopped";
        }
    }

    function toggle() {
        page.rowHit = Date.now();
        page.check(page.playing ? player.pause() : player.play());
        page.wake();
    }

    function wake() {
        page.barShown = true;
        idle.stop();
        idle.start();
    }

    function tapped() {
        if (Date.now() - page.rowHit < 400) {
            return;
        }
        if (page.barShown && page.phase == "ready") {
            idle.stop();
            page.barShown = false;
        } else {
            page.wake();
        }
    }

    // A finger on the timeline; the player is asked at the end, not at every move.
    function scrub(event, width) {
        if (event.isDown()) {
            page.rowHit = Date.now();
        }
        if (!page.canSeek) {
            if (event.isDown()) {
                page.wake();
            }
            return;
        }
        if (event.isDown() || event.isMove()) {
            page.scrubbing = true;
            page.at = Math.max(0, Math.min(1, (event.localX - page.dot / 2) / (width - page.dot)));
            stale.start();
        } else {
            page.release();
        }
    }

    function release() {
        stale.stop();
        if (!page.scrubbing) {
            return;
        }
        page.scrubbing = false;
        page.ended = false;
        page.settling = true;
        page.seekAsked = true;
        if (player.seekTime(Math.round(page.at * page.dur)) != 0) {
            page.settling = false;
        }
        page.wake();
    }

    function save() {
        var name = page.media.split("?")[0].split("/").pop();
        var saved = opener.saveToShared(page.file, name == "" ? "reddit-video.mp4" : (name.indexOf(".") < 0 ? name + ".mp4" : name));
        notice.body = saved != "" ? qsTr("Saved to your videos") : qsTr("The video could not be saved.");
        notice.show();
    }

    // The engine died while the video was being made: nothing would ever say so (the request's own
    // reply came before, as "pending").
    function onLink() {
        if (!engine.connected && (page.phase == "ask" || page.phase == "pending")) {
            page.fail("disconnected", "");
        }
    }

    // Leaving the page: the engine stops preparing, the player lets go.
    function cleanup() {
        page.cancel();
        idle.stop();
        stale.stop();
        try {
            player.stop();
            player.reset();
        } catch (e0) {
        }
        try {
            engine.reply.disconnect(page.onReply);
        } catch (e1) {
        }
        try {
            engine.event.disconnect(page.onEvent);
        } catch (e2) {
        }
        try {
            thumbs.ready.disconnect(page.onThumb);
        } catch (e3) {
        }
        try {
            engine.connectedChanged.disconnect(page.onLink);
        } catch (e4) {
        }
    }

    onCreationCompleted: {
        engine.reply.connect(page.onReply);
        engine.event.connect(page.onEvent);
        engine.connectedChanged.connect(page.onLink);
        thumbs.ready.connect(page.onThumb);
    }

    // Play and pause are in the control bar (a tap on the picture shows it) and on the space bar.
    shortcuts: [
        Shortcut {
            key: "Space"
            enabled: !session.modal && page.phase == "ready"
            onTriggered: {
                page.toggle();
            }
        }
    ]

    actions: [
        ActionItem {
            title: qsTr("Save")
            imageSource: "asset:///images/ic_save.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            enabled: page.phase == "ready"
            onTriggered: {
                page.save();
            }
        },
        ActionItem {
            title: qsTr("Share link")
            imageSource: "asset:///images/ic_share.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            onTriggered: {
                opener.share(page.media);
            }
        },
        ActionItem {
            title: qsTr("Open in browser")
            imageSource: "asset:///images/ic_open.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            onTriggered: {
                opener.openUrl(page.media);
            }
        }
    ]

    attachedObjects: [
        MediaPlayer {
            id: player
            videoOutput: VideoOutput.PrimaryDisplay
            windowId: surface.windowId
            windowGroupId: surface.windowGroup
            statusInterval: 250
            repeatMode: page.gif ? RepeatMode.Track : RepeatMode.None
            volume: page.gif ? 0 : 1
            onPositionChanged: {
                page.settling = false;
                page.seekAsked = false;
            }
            onPlaybackCompleted: {
                page.ended = true;
            }
            onMediaStateChanged: {
                if (mediaState == MediaState.Started) {
                    page.ended = false;
                }
            }
            // The video's own shape, once the player has read it: the engine's w and h may be 0.
            onVideoDimensionsChanged: {
                if (videoDimensions.width > 0 && videoDimensions.height > 0) {
                    page.ratio = videoDimensions.height / videoDimensions.width;
                }
            }
            onError: {
                if (page.seekAsked) {
                    page.seekAsked = false;
                    page.settling = false;
                } else {
                    page.fail("player", qsTr("The player stopped (error %1).").arg(mediaError));
                }
            }
        },
        // A drag whose end never came ends 1.5 s after its last move.
        Timer {
            id: stale
            interval: 1500
            singleShot: true
            onTimeout: {
                page.release();
            }
        },
        // 3 s without a touch, while it plays: the bars go.
        Timer {
            id: idle
            interval: 3000
            singleShot: true
            onTimeout: {
                if (page.playing && !page.scrubbing) {
                    page.barShown = false;
                }
            }
        },
        SystemToast {
            id: notice
            autoUpdateEnabled: true
        }
    ]

    Container {
        background: Color.Black
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        layout: DockLayout {
        }
        attachedObjects: LayoutUpdateHandler {
            id: area
        }
        // The picture, at its own shape, as big as the screen lets it be.
        ForeignWindowControl {
            id: surface
            windowId: "phoenixVideo"
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center
            property real fit: Math.min(area.layoutFrame.width, area.layoutFrame.height / page.ratio)
            preferredWidth: fit
            preferredHeight: fit * page.ratio
            visible: boundToWindow && page.phase == "ready"
            updatedProperties: WindowProperty.Size | WindowProperty.Position | WindowProperty.Visible
        }
        // While it is made ready: the preview, dimmed, under the ring.
        ImageView {
            visible: page.phase != "ready" && page.previewFile != ""
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            imageSource: page.previewFile
            scalingMethod: ScalingMethod.AspectFit
            loadEffect: ImageViewLoadEffect.None
            opacity: 0.5
        }
        // The ring tengram shows for a download: it fills as the bytes arrive, the cross in its
        // middle cancels (a tap anywhere on it), and under it how much has come of how much.
        Container {
            visible: page.phase == "ask" || page.phase == "pending"
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center
            ImageView {
                horizontalAlignment: HorizontalAlignment.Center
                preferredWidth: ui.du(12)
                preferredHeight: ui.du(12)
                imageSource: "asset:///images/ring/ring_" + Fmt.ringFrame(page.done, page.total) + ".png"
                scalingMethod: ScalingMethod.AspectFit
                loadEffect: ImageViewLoadEffect.None
                gestureHandlers: TapHandler {
                    onTapped: {
                        page.cancel();
                    }
                }
            }
            Label {
                horizontalAlignment: HorizontalAlignment.Center
                topMargin: ui.du(1)
                text: Fmt.progressText(page.done, page.total)
                textFormat: TextFormat.Plain
                textStyle.base: SystemDefaults.TextStyles.SmallText
                textStyle.color: Color.White
            }
        }
        // Cancelled: the play mark in the ring's place; a tap makes it ready again.
        ImageView {
            visible: page.phase == "stopped"
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center
            preferredWidth: ui.du(12)
            preferredHeight: ui.du(12)
            imageSource: "asset:///images/play_badge.png"
            scalingMethod: ScalingMethod.AspectFit
            loadEffect: ImageViewLoadEffect.None
            gestureHandlers: TapHandler {
                onTapped: {
                    page.ask();
                }
            }
        }
        Container {
            visible: page.phase == "error"
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Center
            ErrorNote {
                code: page.errorCode
                message: page.errorMessage
                onRetry: {
                    page.ask();
                }
            }
            Button {
                visible: page.errorCode == "unsupported"
                text: qsTr("Open in browser")
                leftMargin: ui.du(3)
                rightMargin: ui.du(3)
                horizontalAlignment: HorizontalAlignment.Fill
                onClicked: {
                    opener.openUrl(page.media);
                }
            }
        }
        // A tap on the picture: the bars away and back. Under our bar, which takes its own.
        Container {
            visible: page.phase == "ready"
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            gestureHandlers: TapHandler {
                onTapped: {
                    page.tapped();
                }
            }
        }
        // Our control bar, over the picture, at the bottom between the system's Back and overflow
        // buttons: play/pause, the timeline, the time.
        Container {
            visible: page.phase == "ready" && !page.gif && page.barShown
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Bottom
            preferredHeight: page.chromeH
            leftPadding: page.chromeSide
            rightPadding: page.chromeSide
            overlapTouchPolicy: OverlapTouchPolicy.Allow
            layout: DockLayout {
            }
            Container {
                horizontalAlignment: HorizontalAlignment.Fill
                verticalAlignment: VerticalAlignment.Fill
                background: Color.create(0.0, 0.0, 0.0, 0.55)
                // The time keeps off the overflow button.
                rightPadding: ui.du(2)
                layout: StackLayout {
                    orientation: LayoutOrientation.LeftToRight
                }
                Container {
                    id: playBtn
                    verticalAlignment: VerticalAlignment.Center
                    preferredWidth: ui.du(9)
                    preferredHeight: ui.du(9)
                    layout: DockLayout {
                    }
                    ImageView {
                        horizontalAlignment: HorizontalAlignment.Center
                        verticalAlignment: VerticalAlignment.Center
                        preferredWidth: ui.du(6.5)
                        preferredHeight: ui.du(6.5)
                        imageSource: page.playing ? "asset:///images/ic_pause.png" : "asset:///images/ic_play.png"
                        filterColor: ui.palette.primary
                    }
                    gestureHandlers: TapHandler {
                        onTapped: {
                            page.toggle();
                        }
                    }
                }
                // The timeline: the whole 8 du band takes the touch, the track is the thin line in
                // it, the fill is the accent and the dot sits at its end.
                Container {
                    id: line
                    property real w: 0
                    verticalAlignment: VerticalAlignment.Center
                    preferredHeight: ui.du(8)
                    layoutProperties: StackLayoutProperties {
                        spaceQuota: 1
                    }
                    layout: DockLayout {
                    }
                    attachedObjects: LayoutUpdateHandler {
                        onLayoutFrameChanged: {
                            line.w = layoutFrame.width;
                        }
                    }
                    onTouch: {
                        page.scrub(event, line.w);
                    }
                    Container {
                        horizontalAlignment: HorizontalAlignment.Fill
                        verticalAlignment: VerticalAlignment.Center
                        preferredHeight: ui.px(6)
                        background: Color.create(1.0, 1.0, 1.0, 0.25)
                    }
                    Container {
                        horizontalAlignment: HorizontalAlignment.Fill
                        verticalAlignment: VerticalAlignment.Center
                        implicitLayoutAnimationsEnabled: false
                        layout: StackLayout {
                            orientation: LayoutOrientation.LeftToRight
                        }
                        Container {
                            verticalAlignment: VerticalAlignment.Center
                            preferredHeight: ui.px(6)
                            implicitLayoutAnimationsEnabled: false
                            layoutProperties: StackLayoutProperties {
                                spaceQuota: Math.max(page.frac, 0.001)
                            }
                            background: ui.palette.primary
                        }
                        Container {
                            visible: page.canSeek
                            verticalAlignment: VerticalAlignment.Center
                            preferredWidth: page.dot
                            preferredHeight: page.dot
                            implicitLayoutAnimationsEnabled: false
                            background: ui.palette.primary
                        }
                        Container {
                            implicitLayoutAnimationsEnabled: false
                            layoutProperties: StackLayoutProperties {
                                spaceQuota: Math.max(1 - page.frac, 0.001)
                            }
                        }
                    }
                }
                Label {
                    verticalAlignment: VerticalAlignment.Center
                    leftMargin: ui.du(1)
                    text: page.clock(page.frac * page.dur) + " / " + page.clock(page.dur)
                    textFormat: TextFormat.Plain
                    textStyle.base: SystemDefaults.TextStyles.SmallText
                    textStyle.color: Color.White
                }
            }
        }
    }
}
