import QtQuick
import QtQuick.Controls
import Qml

Rectangle {
    id: root

    property bool expanded: false
    property string currentScreen: "chat"
    property var chatBackend: null
    property var sessions: []

    signal navigateToChat()
    signal navigateToSettings()
    signal newChatRequested()
    signal sessionSelected(string sessionId)

    // Refresh sessions when sidebar is expanded
    onExpandedChanged: {
        if (expanded) {
            refreshSessions()
        }
    }

    function refreshSessions() {
        if (chatBackend) {
            sessions = chatBackend.getSessions()
        }
    }

    width: expanded ? 240 : 48
    color: Constants.backgroundSecondary

    Behavior on width {
        NumberAnimation {
            duration: 300
            easing.type: Easing.InOutQuad
        }
    }

    Column {
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.left: parent.left
        anchors.leftMargin: 8
        spacing: 16

        // Hamburger menu - toggle sidebar
        Button {
            id: hamburger
            width: 32
            height: 32
            background: Rectangle {
                color: hamburger.hovered ? Constants.surface : "transparent"
                radius: 6
            }

            contentItem: Text {
                anchors.centerIn: parent
                text: "\u2630"
                color: Constants.textMuted
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: root.expanded = !root.expanded
        }

        // New chat button
        Button {
            id: newChatButton
            width: 32
            height: 32
            background: Rectangle {
                radius: 16
                color: newChatButton.hovered ? "#4320c4" : "#2e15b0"
            }

            contentItem: Text {
                anchors.centerIn: parent
                text: "+"
                color: Constants.textPrimary
                font.pixelSize: 18
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: root.newChatRequested()
        }

        // Chat button
        Button {
            id: chatButton
            width: 32
            height: 32
            background: Rectangle {
                color: {
                    if (root.currentScreen === "chat") {
                        return Constants.surface
                    }
                    return chatButton.hovered ? Constants.surface : "transparent"
                }
                radius: 6
            }

            contentItem: Text {
                anchors.centerIn: parent
                text: "\u{1F5E8}"
                color: root.currentScreen === "chat" ? Constants.textPrimary : Constants.textMuted
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: root.navigateToChat()
        }

        // Settings button
        Button {
            id: settingsButton
            width: 32
            height: 32
            background: Rectangle {
                color: {
                    if (root.currentScreen === "settings") {
                        return Constants.surface
                    }
                    return settingsButton.hovered ? Constants.surface : "transparent"
                }
                radius: 6
            }

            contentItem: Text {
                anchors.centerIn: parent
                text: "\u2699"
                color: root.currentScreen === "settings" ? Constants.textPrimary : Constants.textMuted
                font.pixelSize: 28
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: root.navigateToSettings()
        }
    }

    // Expanded content - chat history list
    Column {
        anchors.top: parent.top
        anchors.topMargin: 180
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 4
        visible: root.expanded
        opacity: root.expanded ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }

        Text {
            text: "Recent Chats"
            color: Constants.textMuted
            font.pixelSize: 11
            font.family: Constants.font.family
        }

        // Session list
        ListView {
            id: sessionList
            width: parent.width
            height: Math.min(contentHeight, 300)
            model: root.sessions
            spacing: 4
            clip: true

            delegate: Rectangle {
                width: sessionList.width
                height: 48
                radius: 6
                color: mouseArea.containsMouse ? Constants.surface : "transparent"

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.sessionSelected(modelData.id)
                    }
                }

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 2

                    Text {
                        width: parent.width
                        text: modelData.preview || "New conversation"
                        color: Constants.textPrimary
                        font.pixelSize: 13
                        font.family: Constants.font.family
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: modelData.updatedAt || ""
                        color: Constants.textDisabled
                        font.pixelSize: 10
                        font.family: Constants.font.family
                        elide: Text.ElideRight
                    }
                }
            }
        }

        // No sessions placeholder
        Text {
            text: "No recent chats"
            color: Constants.textDisabled
            font.pixelSize: 13
            font.family: Constants.font.family
            visible: root.sessions.length === 0
        }
    }
}
