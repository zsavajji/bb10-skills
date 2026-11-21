import bb.cascades 1.4
import bb.system 1.2
import hello 1.0

// The stock BB10 shape: a NavigationPane of Pages, each with a TitleBar and
// actions (the Signature one in the middle of the bar, less frequent ones in
// the ⋮ action menu), long-press context actions on list rows, and the
// swipe-down application menu for app-wide items (Help far left, Settings far
// right; at most five).
NavigationPane {
    id: nav

    Menu.definition: MenuDefinition {
        helpAction: HelpActionItem {
            onTriggered: nav.push(aboutPage.createObject())
        }
    }

    Page {
        id: notesPage
        titleBar: TitleBar {
            title: qsTr("Notes")
        }

        Container {
            topPadding: ui.du(2)
            leftPadding: ui.du(2)
            rightPadding: ui.du(2)

            TextField {
                id: input
                hintText: qsTr("Write a note")
                input {
                    submitKey: SubmitKey.Submit
                    onSubmitted: notesPage.add()
                }
            }

            ListView {
                topMargin: ui.du(2)
                dataModel: ArrayDataModel {
                    id: model
                }

                // Rows reach the page through ListItem.view: they live in
                // their own context and cannot see ids outside the component.
                function remove(index) {
                    backend.removeNote(index)
                }
                function copy(text) {
                    backend.copy(text)
                }

                listItemComponents: [
                    ListItemComponent {
                        StandardListItem {
                            id: row
                            title: ListItemData.text
                            contextActions: [
                                ActionSet {
                                    title: ListItemData.text
                                    ActionItem {
                                        title: qsTr("Copy")
                                        onTriggered: row.ListItem.view.copy(ListItemData.text)
                                    }
                                    DeleteActionItem {
                                        onTriggered: row.ListItem.view.remove(ListItemData.index)
                                    }
                                }
                            ]
                        }
                    }
                ]
            }
        }

        actions: [
            ActionItem {
                title: qsTr("Add")
                ActionBar.placement: ActionBarPlacement.Signature
                // Keyboard phones: one key picks it, but only while no field
                // is typing, or the shortcut eats the letter.
                shortcuts: Shortcut {
                    key: "A"
                    enabled: !input.focused
                }
                onTriggered: notesPage.add()
            },
            // Less frequent, and about this screen only: the action menu.
            ActionItem {
                title: qsTr("Copy all")
                ActionBar.placement: ActionBarPlacement.InOverflow
                enabled: backend.lastNote != ""
                onTriggered: {
                    var lines = []
                    for (var i = 0; i < model.size(); ++i)
                        lines.push(model.value(i).text)
                    backend.copy(lines.join("\n"))
                }
            }
        ]

        function add() {
            backend.addNote(input.text)
            input.text = ""
        }
    }

    attachedObjects: [
        ComponentDefinition {
            id: aboutPage
            Page {
                titleBar: TitleBar {
                    title: qsTr("About")
                }
                Container {
                    leftPadding: ui.du(2)
                    rightPadding: ui.du(2)
                    topPadding: ui.du(2)
                    Label {
                        multiline: true
                        text: qsTr("A Cascades starter: NavigationPane, actions, a list with context actions, an Active Frame.")
                    }
                }
            }
        },
        SystemToast {
            id: toast
            autoUpdateEnabled: true
        },
        Timer {
            // Example of the registered QTimer: refresh the list shortly
            // after start (a stand-in for any delayed work).
            id: startTimer
            singleShot: true
            interval: 0
            onTimeout: nav.reload()
        }
    ]

    // Pushed pages are destroyed when popped.
    onPopTransitionEnded: page.destroy()

    function reload() {
        model.clear()
        model.append(backend.notes)
    }

    onCreationCompleted: {
        backend.notesChanged.connect(nav.reload)
        backend.message.connect(function (text) {
            toast.body = text
            toast.show()
        })
        startTimer.start()
    }
}
