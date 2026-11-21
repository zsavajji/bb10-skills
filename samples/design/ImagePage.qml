import bb.cascades 1.4
import bb.system 1.2
import "format.js" as Fmt

// A picture, or a gallery of them, full screen on black; pinch zooms from 1 to 4 times and a
// dragged finger pans. `items`: the entries {preview, full, caption, link, media} (Images are
// {url, w, h}); `index`: the one to show first. The preview is shown at once when it is cached
// and gives way to the full picture when that arrives; a spinner while neither is there. An entry
// with `media` (an animated one) shows a play mark and opens the video page. Pushed by main.qml
// (router.go): the properties are set, then start() is called.
Page {
    id: page
    property variant items
    property int index: 0
    property string title: ""
    // Needs every horizontal drag (the tab's NavigationPane turns peeking off while this is on top).
    property bool noPeek: true
    // Only the system's Back and overflow buttons; the actions are in the overflow menu.
    actionBarVisibility: ChromeVisibility.Compact

    property int cur: 0
    property string fullUrl: ""
    property string previewUrl: ""
    property string src: ""             // the file on show ("" while none is there)
    property bool isFull: false
    property string failure: ""         // "" or the error of the full picture when there is nothing to show
    property real w: 0
    property real h: 0
    property bool gallery: page.items != undefined && page.items.length > 1
    property bool nothing: false        // the entry has no picture address at all: no spinner, the "cannot be shown" state

    function entry() {
        return page.items && page.cur < page.items.length ? page.items[page.cur] : null;
    }

    function urlOf(image) {
        return image && image.url ? image.url : "";
    }

    function linkOf() {
        var e = page.entry();
        if (!e) {
            return "";
        }
        return e.link ? e.link : page.urlOf(e.full ? e.full : e.preview);
    }

    // Shows what is there, asks for what is not.
    function refresh() {
        var f = thumbs.file(page.fullUrl);
        var p = thumbs.file(page.previewUrl);
        page.isFull = f != "";
        var next = f != "" ? f : p;
        if (next != page.src) {
            page.src = next;
        }
        var err = page.fullUrl != "" ? thumbs.error(page.fullUrl) : "";
        if (next == "" && err == "" && page.previewUrl != "") {
            err = thumbs.error(page.previewUrl);
        }
        page.failure = next == "" ? err : "";
    }

    function show(i) {
        if (!page.items || i < 0 || i >= page.items.length) {
            return;
        }
        page.cur = i;
        var e = page.items[i];
        page.fullUrl = page.urlOf(e.full);
        page.previewUrl = page.urlOf(e.preview);
        if (page.fullUrl == "") {
            page.fullUrl = page.previewUrl;
        }
        // No preview and no full picture: the entry's own link, if it looks like a picture.
        if (page.fullUrl == "" && Fmt.isPictureUrl(e.link)) {
            page.fullUrl = e.link;
            page.previewUrl = e.link;
        }
        page.nothing = page.fullUrl == "";
        page.src = "";
        scroller.resetViewableArea();
        // The one the user is waiting for goes ahead of the queue.
        thumbs.want(page.previewUrl, true);
        thumbs.want(page.fullUrl, true);
        page.refresh();
    }

    function start() {
        // Previous and Next are a gallery's.
        if (page.gallery) {
            page.addAction(prevAction, ActionBarPlacement.InOverflow);
            page.addAction(nextAction, ActionBarPlacement.InOverflow);
        }
        page.show(page.index);
    }

    function onReady(url, file) {
        if (url == page.fullUrl || url == page.previewUrl) {
            page.refresh();
        }
    }

    function onFailed(url, error) {
        if (url == page.fullUrl || url == page.previewUrl) {
            page.refresh();
        }
    }

    function retry() {
        page.failure = "";
        thumbs.want(page.fullUrl, true);
        thumbs.want(page.previewUrl, true);
    }

    // To the next or the previous picture of a gallery, with the swipe drawn: the picture on show
    // leaves to the side it was sent (slideOut), the new one comes in from the other (slideIn).
    // The buttons and the N and P keys go the same way.
    // ponytail: the new picture is asked for when the old one has left, so on a slow decode it
    // appears a moment into its slide; decode it during the slide-out if that shows.
    property int slideBy: 0             // +1 or -1 while a slide runs, else 0
    function step(by) {
        var to = page.cur + by;
        if (page.slideBy != 0 || !page.items || to < 0 || to >= page.items.length) {
            return;
        }
        page.slideBy = by;
        slideOut.toX = by > 0 ? - page.w : page.w;
        slideOut.play();
    }

    function slidOut() {
        page.show(page.cur + page.slideBy);
        slideIn.fromX = page.slideBy > 0 ? page.w : - page.w;
        slideIn.play();
    }

    function slidIn() {
        page.slideBy = 0;
    }

    function play() {
        var e = page.entry();
        if (e && e.media) {
            router.go("video", {
                "media": e.media,
                "gif": true,
                "title": page.title,
                "preview": page.previewUrl
            });
        }
    }

    function save() {
        var name = page.linkOf().split("?")[0].split("/").pop();
        var saved = opener.saveToShared(page.src, name);
        notice.body = saved != "" ? qsTr("Saved to your pictures") : qsTr("The picture could not be saved.");
        notice.show();
    }

    function cleanup() {
        try {
            thumbs.ready.disconnect(page.onReady);
        } catch (e0) {
        }
        try {
            thumbs.failed.disconnect(page.onFailed);
        } catch (e1) {
        }
    }

    onCreationCompleted: {
        thumbs.ready.connect(page.onReady);
        thumbs.failed.connect(page.onFailed);
    }

    // No title bar (the user, 2026-11-21): the picture has the height. A gallery's "3 / 12" lies
    // over the picture's top corner.

    // The keys do not depend on where the actions are shown.
    shortcuts: [
        Shortcut {
            key: "p"
            enabled: !session.modal && page.gallery && page.cur > 0
            onTriggered: {
                page.step(-1);
            }
        },
        Shortcut {
            key: "n"
            enabled: !session.modal && page.gallery && page.items != undefined && page.cur < page.items.length - 1
            onTriggered: {
                page.step(1);
            }
        }
    ]

    actions: [
        ActionItem {
            title: qsTr("Save")
            imageSource: "asset:///images/ic_save.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            enabled: page.src != ""
            onTriggered: {
                page.save();
            }
        },
        ActionItem {
            title: qsTr("Share link")
            imageSource: "asset:///images/ic_share.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            onTriggered: {
                opener.share(page.linkOf());
            }
        },
        ActionItem {
            title: qsTr("Open in browser")
            imageSource: "asset:///images/ic_open.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            onTriggered: {
                opener.openUrl(page.linkOf());
            }
        }
    ]

    Container {
        id: root
        background: Color.Black
        layout: DockLayout {
        }
        // A swipe across the picture goes to the next or the previous one of a gallery (the user,
        // 2026-11-21), when the picture is not zoomed: zoomed, a drag moves it about. Watched in
        // the capture phase, as the ScrollView under the finger takes the touch for itself.
        // ponytail: the slide starts when the finger lifts, the picture does not follow the
        // finger; a horizontal ListView that snaps a page at a time would, at the price of the
        // pinch zoom.
        property real downX: 0
        property real downY: 0
        property bool swiping: false
        onTouchCapture: {
            if (!page.gallery) {
                return;
            }
            if (event.isDown()) {
                root.downX = event.windowX;
                root.downY = event.windowY;
                // Not from where the system's Back and overflow buttons are (bottom corners).
                var corner = event.windowY > page.h - ui.du(14) && (event.windowX < ui.du(14) || event.windowX > page.w - ui.du(14));
                root.swiping = scroller.contentScale <= 1.01 && !corner;
            } else if (event.isCancel()) {
                root.swiping = false;
            } else if (event.isUp() && root.swiping) {
                root.swiping = false;
                var dx = event.windowX - root.downX;
                var dy = event.windowY - root.downY;
                // A quarter of the screen across (a fifth turned pictures by accident: the user,
                // 2026-11-21), and clearly more across than up or down; a pinch that began
                // meanwhile has changed the scale and is not a swipe.
                if (scroller.contentScale <= 1.01 && Math.abs(dx) > page.w / 4 && Math.abs(dx) > 2 * Math.abs(dy)) {
                    page.step(dx < 0 ? 1 : -1);
                }
            }
        }
        attachedObjects: LayoutUpdateHandler {
            onLayoutFrameChanged: {
                page.w = layoutFrame.width;
                page.h = layoutFrame.height;
            }
        }
        ScrollView {
            id: scroller
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            animations: [
                TranslateTransition {
                    id: slideOut
                    duration: 160
                    easingCurve: StockCurve.QuadraticIn
                    onEnded: {
                        page.slidOut();
                    }
                },
                TranslateTransition {
                    id: slideIn
                    toX: 0
                    duration: 200
                    easingCurve: StockCurve.QuadraticOut
                    onEnded: {
                        page.slidIn();
                    }
                }
            ]
            scrollViewProperties {
                scrollMode: ScrollMode.Both
                pinchToZoomEnabled: true
                minContentScale: 1
                maxContentScale: 4
            }
            ImageView {
                preferredWidth: page.w
                preferredHeight: page.h
                imageSource: page.src
                scalingMethod: ScalingMethod.AspectFit
                loadEffect: ImageViewLoadEffect.None
            }
        }
        // Where in the gallery: over the picture, top right.
        Container {
            visible: page.gallery
            horizontalAlignment: HorizontalAlignment.Right
            verticalAlignment: VerticalAlignment.Top
            leftPadding: ui.du(2)
            rightPadding: ui.du(2)
            topPadding: ui.du(0.8)
            bottomPadding: ui.du(0.8)
            background: Color.create("#99000000")
            // Body-size and bold (the user, 2026-11-21: the small text was too small to read at a glance).
            Label {
                text: page.gallery ? qsTr("%1 / %2").arg(page.cur + 1).arg(page.items.length) : ""
                textFormat: TextFormat.Plain
                textStyle.fontWeight: FontWeight.W500
                textStyle.color: Color.White
            }
        }
        ActivityIndicator {
            running: page.src == "" && page.failure == "" && !page.nothing
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center
            preferredWidth: ui.du(8)
            preferredHeight: ui.du(8)
        }
        // The animated entry of a gallery: a play mark, and a tap on it plays.
        Container {
            visible: page.entry() != null && !!page.entry().media && page.src != ""
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center
            preferredWidth: ui.du(16)
            preferredHeight: ui.du(16)
            layout: DockLayout {
            }
            background: Color.create("#99000000")
            ImageView {
                horizontalAlignment: HorizontalAlignment.Center
                verticalAlignment: VerticalAlignment.Center
                preferredWidth: ui.du(10)
                preferredHeight: ui.du(10)
                imageSource: "asset:///images/ic_play.png"
            }
            gestureHandlers: TapHandler {
                onTapped: {
                    page.play();
                }
            }
        }
        // Nothing to show: why.
        Container {
            visible: page.failure != "" || page.nothing
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center
            leftPadding: ui.du(3)
            rightPadding: ui.du(3)
            Label {
                textStyle.fontSize: (prefs.textScale == 80 || prefs.textScale == 120) ? FontSize.PercentageValue : FontSize.Default
                textStyle.fontSizeValue: prefs.textScale
                text: page.failure == "unsupported" ? qsTr("This picture's format cannot be shown.") : (page.nothing ? qsTr("This picture cannot be shown.") : qsTr("The picture could not be loaded."))
                textFormat: TextFormat.Plain
                multiline: true
                horizontalAlignment: HorizontalAlignment.Center
                textStyle.textAlign: TextAlign.Center
            }
            Button {
                visible: page.failure == "unsupported" || page.nothing
                text: qsTr("Open in browser")
                topMargin: ui.du(2)
                horizontalAlignment: HorizontalAlignment.Fill
                onClicked: {
                    opener.openUrl(page.linkOf());
                }
            }
            Button {
                visible: page.failure != "unsupported" && !page.nothing
                text: qsTr("Retry")
                topMargin: ui.du(2)
                horizontalAlignment: HorizontalAlignment.Fill
                onClicked: {
                    page.retry();
                }
            }
        }
        // The caption of a gallery's picture, under it.
        Container {
            visible: page.entry() != null && !!page.entry().caption
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Bottom
            leftPadding: ui.du(1.5)
            rightPadding: ui.du(1.5)
            topPadding: ui.du(1)
            bottomPadding: ui.du(1)
            background: Color.create("#B3000000")
            Label {
                textStyle.fontSize: (prefs.textScale == 80 || prefs.textScale == 120) ? FontSize.PercentageValue : FontSize.Default
                textStyle.fontSizeValue: prefs.textScale
                text: page.entry() != null && page.entry().caption ? page.entry().caption : ""
                textFormat: TextFormat.Plain
                multiline: true
                autoSize.maxLineCount: 4
                topMargin: 0
                bottomMargin: 0
            }
        }
    }

    attachedObjects: [
        ActionItem {
            id: prevAction
            title: qsTr("Previous")
            imageSource: "asset:///images/ic_previous.png"
            enabled: page.gallery && page.cur > 0
            onTriggered: {
                page.step(-1);
            }
        },
        ActionItem {
            id: nextAction
            title: qsTr("Next")
            imageSource: "asset:///images/ic_next.png"
            enabled: page.gallery && page.items != undefined && page.cur < page.items.length - 1
            onTriggered: {
                page.step(1);
            }
        },
        SystemToast {
            id: notice
            autoUpdateEnabled: true
        }
    ]
}
