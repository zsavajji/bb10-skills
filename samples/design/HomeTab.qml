import bb.cascades 1.4

// The Home tab's content, loaded by its Tab's delegate when the tab is first opened: a
// NavigationPane whose first page is a PostListPage in mode "feed"; what the user opens is pushed
// on it by main.qml.
NavigationPane {
    id: nav
    // A page that needs every horizontal drag (the video's timeline, a zoomed picture: its noPeek)
    // turns peeking off while it is on top, so that the drag does not slide the page away; what
    // the pane had is put back when it goes (tengram's pair).
    property bool noPeek: false
    property bool peekWas: true
    onTopChanged: {
        nav.noPeek = !!nav.top && !!nav.top.noPeek;
    }
    onNoPeekChanged: {
        if (nav.noPeek) {
            nav.peekWas = nav.peekEnabled;
        }
        nav.peekEnabled = nav.noPeek ? false : nav.peekWas;
    }
    PostListPage {
        mode: "feed"
        // The bar goes on a scroll down, back on a scroll up (the user, 2026-11-21: Home only).
        // Only over the content (Overlay), or it does not hide: see PostPage.
        actionBarVisibility: ChromeVisibility.Overlay
        actionBarAutoHideBehavior: ActionBarAutoHideBehavior.HideOnScroll
        title: qsTr("Home")
        rootPage: true
        autoStart: true
    }
    onPopTransitionEnded: {
        if (page.cleanup) {
            page.cleanup();
        }
        page.destroy();
    }
}
