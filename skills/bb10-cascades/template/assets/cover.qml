import bb.cascades 1.4

// The Active Frame: little room (a Q10 card fits about two lines and an icon),
// so keep it to the one thing worth seeing at a glance.
Container {
    background: Color.create("#121212")
    layout: DockLayout {}
    Container {
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Center
        leftPadding: ui.du(2)
        rightPadding: ui.du(2)
        Label {
            horizontalAlignment: HorizontalAlignment.Center
            text: backend.lastNote != "" ? backend.lastNote : qsTr("No notes")
            multiline: true
            autoSize.maxLineCount: 2   // only with multiline: true
            textStyle.textAlign: TextAlign.Center
            textStyle.fontSize: FontSize.XSmall
            textStyle.color: Color.White
        }
    }
}
