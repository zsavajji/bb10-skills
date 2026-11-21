import bb.cascades 1.4
import tengram 1.0

// One chat (PLAN.md §7, step 3.1): a FreeForm title bar with the tile, name and status line, and
// the messages as cards. chat is a ChatListModel row, set right after createObject().
Page {
    id: page
    property variant chat
    // [...] app logic left out: topic, staged file and secret-chat properties, the back action, the failure toasts
    onChatChanged: {
        ctl.open(chat, topic)
        // The list starts hidden (listHidden): the guard shows it whatever happens, from the open on.
        revealGuard.start()
        // [...] app logic left out: the overflow actions per kind of chat
        ctl.messages.itemAdded.connect(page.added)
        ctl.messages.newestComing.connect(page.coming)
        // [...] app logic left out: the other signal connections (loading, mentions, media)
    }
    attachedObjects: [
        // [...] app logic left out: the file picker, the unread-settle timer, the actions, the dialogs and toasts
        ChatController {
            id: ctl
            link: engine
        },
        // [...] app logic left out: the other timers, actions, dialogs and toasts
        // The player, the recorder and the search, reaction and staged bars are made at their first use, by
        // the functions of their names: together they were 135 of the 215 ms a chat's page took to build
        // before it could slide in, at every open (Q10, 0.1.1.313).
        // Each in a Container that is never shown: a definition makes only Cascades' own objects ("root object
        // must inherit from bb::cascades::BaseObject"), which a MediaPlayer and an AudioRecorder aren't, so made
        // bare they came back null and a voice note's tap did nothing (Q10, 0.1.1.314 and 315).
        ComponentDefinition {
            id: playerDef
            Container {
                property alias part: voicePlayer
                visible: false
                attachedObjects: VoicePlayer {
                    id: voicePlayer
                    jid: page.chat ? page.chat.jid : ""
                    onFailed: voiceFailed.show()
                    onOffline: notSent.show()
                }
            }
        },
        // [...] app logic left out: the recorder, search, reaction and staged bars, made the same way, and the rest of the attached objects
    ]
    // [...] app logic left out: the Search action, the key listener, the emoji picker's first open
    // Inserts at the cursor and keeps the field focused, so the Q10 keyboard still types into it.
    function picked(emoji) {
        composer.editor.insertPlainText(emoji)
        emojis.use(emoji)
        composer.requestFocus()
    }
    // [...] app logic left out: the objects made on demand, the translate warning, the send-files page
    function player() {
        if (!playerObj) {
            var holder = playerDef.createObject()
            partSlot.add(holder) // the page's, so it goes with the page
            var p = holder.part
            ctl.voiceReady.connect(p.ready)
            playerObj = p
        }
        return playerObj
    }
    // [...] app logic left out: the recorder and bars, composer and send functions, the files and stickers, the slide-in of a new message (slidePin, coming)
    function added(indexPath) {
        if (indexPath[0] != 0) {
            return
        }
        if (slidePin) {
            followOwn = false
            list.slideId = ctl.messages.idAt(indexPath[0])
            cardsProvider.slideRow(list.slideId) // a card already showing it reports now (MessageCards::slideRow)
        } else if (followOwn) {
            followOwn = false
            list.scrollToPosition(ScrollPosition.Beginning, ScrollAnimation.None)
            startAim("bottom", "")
        }
    }
    // [...] app logic left out: the aims, the hidden list and its reveal, the unread jump, the title bar
    Container {
        layout: DockLayout {}
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        Container {
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            layout: StackLayout {
                orientation: LayoutOrientation.TopToBottom
            }
            // [...] app logic left out: the search and part slots
            // The list, and over it the note of an empty chat. No wallpaper (taken off at the user's ask, 2026-11-18): the page's
            // own colour, painted here so that the picture cards' corner masks (MediaBubble) match what is behind the cards.
            Container {
                layoutProperties: StackLayoutProperties {
                    spaceQuota: 1
                }
                background: Color.create(nav.pageColor)
                layout: DockLayout {}
                horizontalAlignment: HorizontalAlignment.Fill
                verticalAlignment: VerticalAlignment.Fill
                ListView {
                    id: list
                    // Filled: a DockLayout's child sits at its top left, at its own size, otherwise.
                    horizontalAlignment: HorizontalAlignment.Fill
                    verticalAlignment: VerticalAlignment.Fill
                    // The C++ cards call these functions on the list: the long-press menu's actions, a mention's
                    // tap, the reactions' chips.
                    function reply(msg) {
                        ctl.reply(msg)
                        composer.requestFocus()
                    }
                    function copy(text) {
                        ctl.copy(text)
                    }
                    // [...] app logic left out: the other functions the cards call (translate, forward, edit, react, polls, voice, media, delete) and the player and media state they read
                    // The cards reach the shared emoji style (main.qml) through ListItem.view.
                    property variant emojiStyle: nav.emojiStyle
                    property int bodySize: nav.bodySize
                    property int smallSize: nav.smallSize
                    property real textScale: settings.textScale
                    // Round mode (MessageCard.cpp): the cards' corners and the sender's tile.
                    property bool round: settings.round
                    // A quote's background: white at 8 % lifts it off a dark card, black at 5 % off
                    // a bright one.
                    property string quoteTint: settings.theme == 1 ? "#0D000000" : "#14FFFFFF"
                    property string pressColor: nav.pressColor
                    // Telegram's bubble and pill colours (main.qml), read by the C++ cards.
                    property string inColor: nav.inColor
                    property string outColor: nav.outColor
                    property string outMeta: nav.outMeta
                    property string serviceColor: nav.serviceColor
                    // What is behind the cards, for a picture's rounded corners (MessageCard::paintBackground).
                    property string pageColor: nav.pageColor
                    dataModel: ctl.messages
                    // Out of sight until the first placement (page.listHidden); revealList fades it in.
                    opacity: 0
                    // [...] app logic left out: the touch handler that ends a running aim
                    // Bottom to top: the model is newest first, so the list opens at the newest message
                    // with no scroll call, and an older page is appended at the visual top without
                    // moving the rows on screen.
                    layout: StackListLayout {
                        orientation: LayoutOrientation.BottomToTop
                    }
                    // [...] app logic left out: the bottom padding and the stick-to-edge policy
                    // The new message's slide (page.coming): the row whose card measures itself. In one go the list jumps to
                    // the bottom (the rows move up by the row's height and the gap between cards) and is moved down as far,
                    // so nothing moves on screen; then it comes back up in 110 ms. The composer, after the list, covers what is
                    // moved below it.
                    property string slideId: ""
                    // The row's latest height: the slide starts when it has held for 30 ms (slideSettle): a reused card reports its old height, and its new layout came within 17 ms (0.1.0.74's log).
                    property real slideH: 0
                    function slideMeasured(h) {
                        slideGuard.stop() // the card is there: the slide waits for its height to hold
                        slideH = h
                        slideSettle.start()
                        return true
                    }
                    function slideIn(h) {
                        slideGuard.stop()
                        list.scrollToPosition(ScrollPosition.Beginning, ScrollAnimation.None)
                        list.translationY = h + ui.du(1)
                        slideAnim.fromY = list.translationY
                        slideAnim.play()
                        return true
                    }
                    // [...] app logic left out: attachedObjects, the fade-in, the slide animation, the guard timers, the scroll state handler
                    // The cards are C++ (MessageCards.cpp, step 8.6): they reach the page through these functions and properties.
                    listItemProvider: MessageCards {
                        id: cardsProvider
                    }
                }
                // [...] app logic left out: the empty note, the bottom spinner, the jump buttons
            }
            // [...] app logic left out: the reaction slot, mention bar, reply bar, staged and recording bars, emoji picker, the composer
        }
        // [...] app logic left out: the black cover while a voice note plays at the ear
    }
}
