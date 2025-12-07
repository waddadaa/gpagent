import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Qml

Rectangle {
    id: root

    property var chatBackend: null
    property string currentModel: chatBackend ? chatBackend.currentModel : "claude-opus-4-5-20251101"
    property var attachedFiles: []

    color: Constants.backgroundColor

    // File dialog for selecting files
    FileDialog {
        id: fileDialog
        title: "Select a file to attach"
        nameFilters: ["Images (*.jpg *.jpeg *.png *.gif *.webp *.bmp)", "PDF files (*.pdf)", "Text files (*.txt *.md *.json *.yaml *.yml)", "All files (*)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file://", "")
            root.attachedFiles = root.attachedFiles.concat([path])
            console.log("Attached file:", path)
        }
    }

    // Header
    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 50
        color: Constants.backgroundColor

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Text {
                text: "Chat"
                color: Constants.textPrimary
                font.pixelSize: 14
                font.family: Constants.font.family
            }

            Text {
                text: "\u2304"
                color: Constants.textMuted
                font.pixelSize: 14
            }
        }

        // Status indicator
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            visible: chatBackend && chatBackend.statusMessage.length > 0

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: "#2e15b0"
                anchors.verticalCenter: parent.verticalCenter

                SequentialAnimation on opacity {
                    running: chatBackend && chatBackend.isBusy
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 500 }
                    NumberAnimation { to: 1.0; duration: 500 }
                }
            }

            Text {
                text: chatBackend ? chatBackend.statusMessage : ""
                color: Constants.textMuted
                font.pixelSize: 12
                font.family: Constants.font.family
            }
        }
    }

    // Chat area
    Rectangle {
        id: chatArea
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: inputArea.top
        color: Constants.backgroundColor

        ListView {
            id: messageList
            anchors.fill: parent
            anchors.leftMargin: parent.width * 0.15
            anchors.rightMargin: parent.width * 0.15
            anchors.topMargin: 20
            anchors.bottomMargin: 20

            model: chatBackend ? chatBackend.messages : null
            spacing: 24
            clip: true

            // Auto-scroll to bottom
            onCountChanged: {
                Qt.callLater(function() {
                    messageList.positionViewAtEnd()
                })
            }

            delegate: Item {
                width: messageList.width
                height: messageContent.height

                Column {
                    id: messageContent
                    width: parent.width
                    spacing: 8

                    // User message
                    Row {
                        spacing: 12
                        visible: model.role === "user"

                        Rectangle {
                            width: 28
                            height: 28
                            radius: 4
                            color: Constants.surface

                            Text {
                                anchors.centerIn: parent
                                text: "U"
                                color: Constants.textPrimary
                                font.pixelSize: 12
                                font.family: Constants.font.family
                                font.bold: true
                            }
                        }

                        TextEdit {
                            text: model.content
                            color: Constants.textPrimary
                            font.pixelSize: 15
                            font.family: Constants.font.family
                            wrapMode: Text.WordWrap
                            width: messageList.width - 60
                            readOnly: true
                            selectByMouse: true
                            textFormat: TextEdit.MarkdownText
                        }
                    }

                    // Assistant message
                    Column {
                        width: parent.width
                        spacing: 12
                        visible: model.role === "assistant"

                        Row {
                            spacing: 12

                            Rectangle {
                                width: 28
                                height: 28
                                radius: 4
                                color: "#2e15b0"

                                Text {
                                    anchors.centerIn: parent
                                    text: "\u2728"
                                    color: Constants.textPrimary
                                    font.pixelSize: 14
                                }
                            }

                            Text {
                                text: "Assistant"
                                color: Constants.textMuted
                                font.pixelSize: 12
                                font.family: Constants.font.family
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            // Streaming indicator
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: "#2e15b0"
                                visible: model.isStreaming
                                anchors.verticalCenter: parent.verticalCenter

                                SequentialAnimation on opacity {
                                    running: model.isStreaming
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.3; duration: 400 }
                                    NumberAnimation { to: 1.0; duration: 400 }
                                }
                            }
                        }

                        TextEdit {
                            text: model.content
                            color: Constants.textPrimary
                            font.pixelSize: 15
                            font.family: Constants.font.family
                            wrapMode: Text.WordWrap
                            width: parent.width
                            leftPadding: 40
                            readOnly: true
                            selectByMouse: true
                            textFormat: TextEdit.MarkdownText
                        }

                        // Action buttons (copy, thumbs up/down, retry)
                        Row {
                            spacing: 16
                            leftPadding: 40
                            visible: !model.isStreaming

                            IconButton {
                                icon: "\u2750"
                                tooltip: "Copy"
                                onClicked: {
                                    // Copy to clipboard
                                }
                            }

                            IconButton {
                                icon: "\u25b2"
                                tooltip: "Good response"
                            }

                            IconButton {
                                icon: "\u25bc"
                                tooltip: "Bad response"
                            }

                            IconButton {
                                icon: "\u21bb"
                                tooltip: "Regenerate"
                            }
                        }
                    }

                    // Tool message
                    Rectangle {
                        width: parent.width
                        height: toolContent.height + 16
                        color: Constants.surface
                        radius: 8
                        visible: model.role === "tool"

                        Column {
                            id: toolContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 8
                            spacing: 4

                            Text {
                                text: "Tool: " + (model.toolName || "unknown")
                                color: Constants.textMuted
                                font.pixelSize: 11
                                font.family: Constants.font.family
                            }

                            TextEdit {
                                text: model.content
                                color: Constants.textSecondary
                                font.pixelSize: 13
                                font.family: Constants.font.family
                                wrapMode: Text.WordWrap
                                width: parent.width
                                readOnly: true
                                selectByMouse: true
                                textFormat: TextEdit.MarkdownText
                            }
                        }
                    }
                }
            }

            // Empty state
            Text {
                anchors.centerIn: parent
                text: "Start a conversation"
                color: Constants.textDisabled
                font.pixelSize: 16
                font.family: Constants.font.family
                visible: messageList.count === 0
            }
        }
    }

    // Input area
    Rectangle {
        id: inputArea
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 120
        color: Constants.backgroundColor

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.6
            height: 90
            radius: 16
            color: Constants.surface

            // Input field
            ScrollView {
                id: inputScroll
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 16
                height: 40
                clip: true

                TextArea {
                    id: messageInput
                    color: Constants.textPrimary
                    font.pixelSize: 15
                    font.family: Constants.font.family
                    placeholderText: "Message..."
                    placeholderTextColor: Constants.textDisabled
                    wrapMode: TextArea.Wrap
                    background: null

                    Keys.onReturnPressed: function(event) {
                        if (event.modifiers & Qt.ShiftModifier) {
                            event.accepted = false
                        } else {
                            sendMessage()
                            event.accepted = true
                        }
                    }
                }
            }

            // Bottom toolbar
            Row {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.bottomMargin: 12
                anchors.leftMargin: 16
                spacing: 8

                // Attach file button
                Rectangle {
                    width: 28
                    height: 28
                    radius: 6
                    color: attachBtn.containsMouse ? Constants.surface : Constants.surfaceElevated

                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        color: Constants.textMuted
                        font.pixelSize: 16
                    }

                    MouseArea {
                        id: attachBtn
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: fileDialog.open()
                    }

                    ToolTip.visible: attachBtn.containsMouse
                    ToolTip.text: "Attach file (PDF, text)"
                    ToolTip.delay: 500
                }

                // Show attached files
                Repeater {
                    model: root.attachedFiles

                    Rectangle {
                        width: attachedFileRow.width + 16
                        height: 24
                        radius: 4
                        color: Constants.surface

                        Row {
                            id: attachedFileRow
                            anchors.centerIn: parent
                            spacing: 4

                            Text {
                                text: {
                                    var path = modelData
                                    var parts = path.split("/")
                                    return parts[parts.length - 1]
                                }
                                color: Constants.textSecondary
                                font.pixelSize: 11
                                font.family: Constants.font.family
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: "\u2715"
                                color: Constants.textMuted
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var files = root.attachedFiles.slice()
                                        files.splice(index, 1)
                                        root.attachedFiles = files
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Model selector and send button
            Row {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.bottomMargin: 12
                anchors.rightMargin: 16
                spacing: 12

                // Model selector
                Item {
                    width: modelSelectorRow.width
                    height: parent.height

                    Row {
                        id: modelSelectorRow
                        spacing: 4
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            text: {
                                var model = root.currentModel
                                if (model.includes("opus")) return "Opus 4.5"
                                if (model.includes("sonnet")) return "Sonnet"
                                if (model.includes("haiku")) return "Haiku"
                                if (model.includes("gemini")) return "Gemini"
                                return model.substring(0, 15)
                            }
                            color: Constants.textMuted
                            font.pixelSize: 13
                            font.family: Constants.font.family
                        }

                        Text {
                            text: "\u2304"
                            color: Constants.textMuted
                            font.pixelSize: 12
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: modelMenu.open()
                    }
                }

                // Send button
                Rectangle {
                    id: sendButton
                    width: 32
                    height: 32
                    color: sendArea.containsMouse ? "#4320c4" : "#2e15b0"
                    radius: 16
                    opacity: (chatBackend && chatBackend.isBusy) ? 0.5 : 1.0

                    Text {
                        anchors.centerIn: parent
                        text: (chatBackend && chatBackend.isBusy) ? "\u25A0" : "\u2191"
                        color: Constants.textPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }

                    MouseArea {
                        id: sendArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (chatBackend && chatBackend.isBusy) {
                                chatBackend.stopGeneration()
                            } else {
                                sendMessage()
                            }
                        }
                    }
                }
            }
        }

        // Disclaimer
        Text {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 8
            text: "AI can make mistakes. Please verify important information."
            color: Constants.textDisabled
            font.pixelSize: 11
            font.family: Constants.font.family
        }
    }

    // Model selection menu
    Menu {
        id: modelMenu

        MenuItem {
            text: "Claude Opus 4.5"
            onTriggered: if (chatBackend) chatBackend.currentModel = "claude-opus-4-5-20251101"
        }
        MenuItem {
            text: "Claude Sonnet 4"
            onTriggered: if (chatBackend) chatBackend.currentModel = "claude-sonnet-4-20250514"
        }
        MenuItem {
            text: "Claude Haiku 3.5"
            onTriggered: if (chatBackend) chatBackend.currentModel = "claude-3-5-haiku-20241022"
        }
        MenuSeparator {}
        MenuItem {
            text: "Gemini Pro"
            onTriggered: if (chatBackend) chatBackend.currentModel = "gemini-pro"
        }
    }

    function sendMessage() {
        var text = messageInput.text.trim()
        console.log("sendMessage called, text:", text)
        if (text.length === 0 && root.attachedFiles.length === 0) return
        if (chatBackend && chatBackend.isBusy) return

        // If we have attached files, prepend file reading instructions
        var fullMessage = text
        if (root.attachedFiles.length > 0) {
            var fileInstructions = "I have attached the following file(s). Please read and analyze them:\n"
            for (var i = 0; i < root.attachedFiles.length; i++) {
                fileInstructions += "- " + root.attachedFiles[i] + "\n"
            }
            if (text.length > 0) {
                fullMessage = fileInstructions + "\nUser request: " + text
            } else {
                fullMessage = fileInstructions + "\nPlease summarize the content of the attached file(s)."
            }
        }

        console.log("chatBackend:", chatBackend)
        if (chatBackend) {
            console.log("Sending message to backend, messages count before:", chatBackend.messages.count)
            chatBackend.sendMessage(fullMessage)
            console.log("Message sent, messages count after:", chatBackend.messages.count)
        }
        messageInput.text = ""
        root.attachedFiles = []  // Clear attached files after sending
    }

    // Icon button component
    component IconButton: Rectangle {
        property string icon: ""
        property string tooltip: ""
        signal clicked()

        width: 24
        height: 24
        color: iconArea.containsMouse ? Constants.surface : "transparent"
        radius: 4

        Text {
            anchors.centerIn: parent
            text: parent.icon
            color: Constants.textMuted
            font.pixelSize: 14
        }

        MouseArea {
            id: iconArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }

        ToolTip.visible: iconArea.containsMouse && tooltip.length > 0
        ToolTip.text: tooltip
        ToolTip.delay: 500
    }
}
