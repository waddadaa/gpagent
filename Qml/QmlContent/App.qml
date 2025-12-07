import QtQuick
import QtQuick.Controls
import Qml
import GPAgent 1.0

Window {
    id: mainWindow
    width: Constants.width
    height: Constants.height
    visible: true
    title: "GPAgent"
    color: Constants.backgroundColor

    // Backend instances
    ChatBackend {
        id: appChatBackend

        onErrorOccurred: function(message) {
            console.log("ERROR from ChatBackend:", message)
            errorDialog.message = message
            errorDialog.open()
        }

        onInitialized: {
            console.log("Chat backend initialized")
        }
    }

    ConfigManager {
        id: appConfigManager

        Component.onCompleted: {
            load()
        }

        onSaved: {
            console.log("Configuration saved, reinitializing chat backend...")
            appChatBackend.initialize()
        }

        onSaveError: function(message) {
            errorDialog.message = "Failed to save: " + message
            errorDialog.open()
        }
    }

    // Initialize chat backend after config is loaded
    Component.onCompleted: {
        Qt.callLater(function() {
            appChatBackend.initialize()
        })
    }

    // Main layout
    Item {
        anchors.fill: parent

        // Sidebar
        Sidebar {
            id: sidebar
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            currentScreen: screenStack.currentItem ? screenStack.currentItem.objectName : "chat"
            chatBackend: appChatBackend

            onNavigateToChat: {
                if (screenStack.currentItem.objectName !== "chat") {
                    screenStack.replace(chatScreen)
                }
            }

            onNavigateToSettings: {
                if (screenStack.currentItem.objectName !== "settings") {
                    screenStack.replace(settingsScreen)
                }
            }

            onNewChatRequested: {
                appChatBackend.newChat()
                sidebar.refreshSessions()
                if (screenStack.currentItem.objectName !== "chat") {
                    screenStack.replace(chatScreen)
                }
            }

            onSessionSelected: function(sessionId) {
                console.log("Switching to session:", sessionId)
                if (appChatBackend.switchSession(sessionId)) {
                    if (screenStack.currentItem.objectName !== "chat") {
                        screenStack.replace(chatScreen)
                    }
                }
            }
        }

        // Main content area with screen stack
        StackView {
            id: screenStack
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            initialItem: chatScreen

            pushEnter: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: 200
                }
            }

            pushExit: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 1
                    to: 0
                    duration: 200
                }
            }

            replaceEnter: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: 200
                }
            }

            replaceExit: Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 1
                    to: 0
                    duration: 200
                }
            }
        }
    }

    // Screen components
    Component {
        id: chatScreen

        Chat {
            objectName: "chat"
            chatBackend: appChatBackend
        }
    }

    Component {
        id: settingsScreen

        Settings {
            objectName: "settings"
            configManager: appConfigManager
        }
    }

    // Error dialog
    Dialog {
        id: errorDialog
        title: "Error"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok
        width: 400

        property string message: ""

        background: Rectangle {
            color: Constants.backgroundSecondary
            radius: 12
            border.color: Constants.surface
            border.width: 1
        }

        header: Rectangle {
            color: "transparent"
            height: 50

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: "Error"
                color: "#ef4444"
                font.pixelSize: 16
                font.family: Constants.font.family
                font.bold: true
            }
        }

        contentItem: Text {
            text: errorDialog.message
            color: Constants.textPrimary
            font.pixelSize: 14
            font.family: Constants.font.family
            wrapMode: Text.WordWrap
            leftPadding: 20
            rightPadding: 20
            topPadding: 10
            bottomPadding: 60
        }
    }

    // Keyboard shortcuts
    Shortcut {
        sequence: "Ctrl+N"
        onActivated: {
            appChatBackend.newChat()
            screenStack.replace(chatScreen)
        }
    }

    Shortcut {
        sequence: "Ctrl+,"
        onActivated: screenStack.replace(settingsScreen)
    }

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (appChatBackend.isBusy) {
                appChatBackend.stopGeneration()
            }
        }
    }
}
