import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qml

Rectangle {
    id: root

    property var configManager: null

    color: Constants.backgroundColor

    // Header
    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 50
        color: Constants.backgroundColor

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            text: "Settings"
            color: Constants.textPrimary
            font.pixelSize: 16
            font.family: Constants.font.family
            font.bold: true
        }

        // Unsaved indicator
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            visible: configManager && configManager.isDirty

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: "#f59e0b"
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: "Unsaved changes"
                color: "#f59e0b"
                font.pixelSize: 12
                font.family: Constants.font.family
            }
        }
    }

    // Settings content
    Flickable {
        id: settingsFlickable
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footer.top
        anchors.topMargin: 20
        contentHeight: settingsColumn.height
        clip: true

        Column {
            id: settingsColumn
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width * 0.6, 600)
            spacing: 32

            // API Keys Section
            Column {
                width: parent.width
                spacing: 16

                Text {
                    text: "API Keys"
                    color: Constants.textPrimary
                    font.pixelSize: 14
                    font.family: Constants.font.family
                    font.bold: true
                }

                Text {
                    text: "Configure your API keys for different LLM providers. Keys are stored locally."
                    color: Constants.textMuted
                    font.pixelSize: 13
                    font.family: Constants.font.family
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                // Claude API Key
                ApiKeyInput {
                    id: claudeKeyInput
                    width: parent.width
                    label: "Anthropic (Claude) API Key"
                    placeholder: "sk-ant-..."
                    targetConfigManager: root.configManager
                    value: targetConfigManager ? targetConfigManager.claudeApiKey : ""
                    onEdited: function(newValue) {
                        console.log("Claude API key onEdited, length:", newValue.length, "configManager:", targetConfigManager)
                        if (targetConfigManager) {
                            targetConfigManager.claudeApiKey = newValue
                            console.log("isDirty after edit:", targetConfigManager.isDirty)
                        }
                    }
                }

                // Gemini API Key
                ApiKeyInput {
                    id: geminiKeyInput
                    width: parent.width
                    label: "Google (Gemini) API Key"
                    placeholder: "AIza..."
                    targetConfigManager: root.configManager
                    value: targetConfigManager ? targetConfigManager.geminiApiKey : ""
                    onEdited: function(newValue) {
                        if (targetConfigManager) targetConfigManager.geminiApiKey = newValue
                    }
                }

                // OpenAI API Key
                ApiKeyInput {
                    id: openaiKeyInput
                    width: parent.width
                    label: "OpenAI API Key"
                    placeholder: "sk-..."
                    targetConfigManager: root.configManager
                    value: targetConfigManager ? targetConfigManager.openaiApiKey : ""
                    onEdited: function(newValue) {
                        if (targetConfigManager) targetConfigManager.openaiApiKey = newValue
                    }
                }
            }

            // Separator
            Rectangle {
                width: parent.width
                height: 1
                color: Constants.surface
            }

            // Web Search Section
            Column {
                width: parent.width
                spacing: 16

                Text {
                    text: "Web Search"
                    color: Constants.textPrimary
                    font.pixelSize: 14
                    font.family: Constants.font.family
                    font.bold: true
                }

                Text {
                    text: "Configure your web search provider and API keys. At least one provider is required for web search to work."
                    color: Constants.textMuted
                    font.pixelSize: 13
                    font.family: Constants.font.family
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                // Search Provider
                Column {
                    width: parent.width
                    spacing: 6

                    Text {
                        text: "Search Provider"
                        color: Constants.textSecondary
                        font.pixelSize: 13
                        font.family: Constants.font.family
                    }

                    ComboBox {
                        id: searchProviderCombo
                        width: parent.width
                        height: 36
                        model: ["perplexity", "tavily", "google"]

                        Component.onCompleted: {
                            var provider = configManager ? configManager.searchProvider : "perplexity"
                            currentIndex = model.indexOf(provider)
                            if (currentIndex < 0) currentIndex = 0
                        }

                        background: Rectangle {
                            color: Constants.surface
                            radius: 6
                            border.color: searchProviderCombo.activeFocus ? "#2e15b0" : "transparent"
                            border.width: 1
                        }

                        contentItem: Text {
                            text: searchProviderCombo.displayText
                            color: Constants.textPrimary
                            font.pixelSize: 14
                            font.family: Constants.font.family
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }

                        onActivated: {
                            if (configManager) configManager.searchProvider = currentText
                        }
                    }
                }

                // Perplexity API Key
                ApiKeyInput {
                    id: perplexityKeyInput
                    width: parent.width
                    label: "Perplexity API Key"
                    placeholder: "pplx-..."
                    targetConfigManager: root.configManager
                    value: targetConfigManager ? targetConfigManager.perplexityApiKey : ""
                    onEdited: function(newValue) {
                        if (targetConfigManager) targetConfigManager.perplexityApiKey = newValue
                    }
                }

                // Tavily API Key
                ApiKeyInput {
                    id: tavilyKeyInput
                    width: parent.width
                    label: "Tavily API Key"
                    placeholder: "tvly-..."
                    targetConfigManager: root.configManager
                    value: targetConfigManager ? targetConfigManager.tavilyApiKey : ""
                    onEdited: function(newValue) {
                        if (targetConfigManager) targetConfigManager.tavilyApiKey = newValue
                    }
                }

                // Google Search API Key
                ApiKeyInput {
                    id: googleSearchKeyInput
                    width: parent.width
                    label: "Google Custom Search API Key"
                    placeholder: "AIza..."
                    targetConfigManager: root.configManager
                    value: targetConfigManager ? targetConfigManager.googleSearchApiKey : ""
                    onEdited: function(newValue) {
                        if (targetConfigManager) targetConfigManager.googleSearchApiKey = newValue
                    }
                }

                // Google Search CX (Engine ID)
                ApiKeyInput {
                    id: googleSearchCxInput
                    width: parent.width
                    label: "Google Search Engine ID (CX)"
                    placeholder: "Search engine ID..."
                    targetConfigManager: root.configManager
                    value: targetConfigManager ? targetConfigManager.googleSearchCx : ""
                    onEdited: function(newValue) {
                        if (targetConfigManager) targetConfigManager.googleSearchCx = newValue
                    }
                }
            }

            // Separator
            Rectangle {
                width: parent.width
                height: 1
                color: Constants.surface
            }

            // Model Settings Section
            Column {
                width: parent.width
                spacing: 16

                Text {
                    text: "Model Settings"
                    color: Constants.textPrimary
                    font.pixelSize: 14
                    font.family: Constants.font.family
                    font.bold: true
                }

                // Primary Provider
                Column {
                    width: parent.width
                    spacing: 6

                    Text {
                        text: "Primary Provider"
                        color: Constants.textSecondary
                        font.pixelSize: 13
                        font.family: Constants.font.family
                    }

                    ComboBox {
                        id: providerCombo
                        width: parent.width
                        height: 36
                        model: ["claude", "gemini", "openai"]

                        Component.onCompleted: {
                            var provider = configManager ? configManager.primaryProvider : "claude"
                            currentIndex = model.indexOf(provider)
                        }

                        background: Rectangle {
                            color: Constants.surface
                            radius: 6
                            border.color: providerCombo.activeFocus ? "#2e15b0" : "transparent"
                            border.width: 1
                        }

                        contentItem: Text {
                            text: providerCombo.displayText
                            color: Constants.textPrimary
                            font.pixelSize: 14
                            font.family: Constants.font.family
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }

                        onActivated: {
                            if (configManager) configManager.primaryProvider = currentText
                        }
                    }
                }

                // Primary Model
                Column {
                    width: parent.width
                    spacing: 6

                    Text {
                        text: "Primary Model"
                        color: Constants.textSecondary
                        font.pixelSize: 13
                        font.family: Constants.font.family
                    }

                    ComboBox {
                        id: modelCombo
                        width: parent.width
                        height: 36

                        function updateModel() {
                            var provider = configManager ? configManager.primaryProvider : "claude"
                            if (provider === "claude") {
                                model = ["claude-opus-4-5-20251101", "claude-sonnet-4-20250514", "claude-3-5-haiku-20241022"]
                            } else if (provider === "gemini") {
                                model = ["gemini-pro", "gemini-pro-vision"]
                            } else {
                                model = ["gpt-4", "gpt-4-turbo", "gpt-3.5-turbo"]
                            }
                        }

                        function syncFromConfig() {
                            var m = configManager ? configManager.primaryModel : ""
                            var idx = model.indexOf(m)
                            currentIndex = idx >= 0 ? idx : 0
                        }

                        Component.onCompleted: {
                            updateModel()
                            syncFromConfig()
                        }

                        Connections {
                            target: providerCombo
                            function onActivated() {
                                modelCombo.updateModel()
                                modelCombo.currentIndex = 0
                                if (configManager) configManager.primaryModel = modelCombo.currentText
                            }
                        }

                        background: Rectangle {
                            color: Constants.surface
                            radius: 6
                            border.color: modelCombo.activeFocus ? "#2e15b0" : "transparent"
                            border.width: 1
                        }

                        contentItem: Text {
                            text: modelCombo.displayText
                            color: Constants.textPrimary
                            font.pixelSize: 14
                            font.family: Constants.font.family
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }

                        onActivated: {
                            if (configManager) configManager.primaryModel = currentText
                        }
                    }
                }

                // Temperature
                Column {
                    width: parent.width
                    spacing: 6

                    Row {
                        width: parent.width
                        spacing: 8

                        Text {
                            text: "Temperature"
                            color: Constants.textSecondary
                            font.pixelSize: 13
                            font.family: Constants.font.family
                        }

                        Text {
                            text: temperatureSlider.value.toFixed(2)
                            color: Constants.textMuted
                            font.pixelSize: 13
                            font.family: Constants.font.family
                        }
                    }

                    Slider {
                        id: temperatureSlider
                        width: parent.width
                        from: 0.0
                        to: 1.0
                        stepSize: 0.05

                        Component.onCompleted: {
                            value = configManager ? configManager.temperature : 0.7
                        }

                        background: Rectangle {
                            x: temperatureSlider.leftPadding
                            y: temperatureSlider.topPadding + temperatureSlider.availableHeight / 2 - height / 2
                            width: temperatureSlider.availableWidth
                            height: 4
                            radius: 2
                            color: Constants.surface

                            Rectangle {
                                width: temperatureSlider.visualPosition * parent.width
                                height: parent.height
                                color: "#2e15b0"
                                radius: 2
                            }
                        }

                        handle: Rectangle {
                            x: temperatureSlider.leftPadding + temperatureSlider.visualPosition * (temperatureSlider.availableWidth - width)
                            y: temperatureSlider.topPadding + temperatureSlider.availableHeight / 2 - height / 2
                            width: 16
                            height: 16
                            radius: 8
                            color: temperatureSlider.pressed ? "#4320c4" : "#2e15b0"
                        }

                        onMoved: {
                            if (configManager) configManager.temperature = value
                        }
                    }

                    Text {
                        text: "Lower values make the model more focused and deterministic. Higher values make it more creative."
                        color: Constants.textDisabled
                        font.pixelSize: 11
                        font.family: Constants.font.family
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                }
            }

            // Separator
            Rectangle {
                width: parent.width
                height: 1
                color: Constants.surface
            }

            // About Section
            Column {
                width: parent.width
                spacing: 12

                Text {
                    text: "About"
                    color: Constants.textPrimary
                    font.pixelSize: 14
                    font.family: Constants.font.family
                    font.bold: true
                }

                Text {
                    text: "GPAgent v0.1.0"
                    color: Constants.textSecondary
                    font.pixelSize: 13
                    font.family: Constants.font.family
                }

                Text {
                    text: "An AI agent powered by Claude, Gemini, and other LLMs."
                    color: Constants.textMuted
                    font.pixelSize: 13
                    font.family: Constants.font.family
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
            }

            // Spacer
            Item {
                width: 1
                height: 40
            }
        }
    }

    // Footer with save/reset buttons
    Rectangle {
        id: footer
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 70
        color: Constants.backgroundColor

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: Constants.surface
        }

        Row {
            anchors.centerIn: parent
            spacing: 16

            Button {
                id: resetButton
                text: "Reset to Defaults"
                width: 150
                height: 40

                background: Rectangle {
                    color: resetButton.hovered ? Constants.surface : "transparent"
                    border.color: Constants.textMuted
                    border.width: 1
                    radius: 8
                }

                contentItem: Text {
                    text: resetButton.text
                    color: Constants.textSecondary
                    font.pixelSize: 14
                    font.family: Constants.font.family
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    if (configManager) configManager.reset()
                }
            }

            Button {
                id: saveButton
                text: "Save"
                width: 150
                height: 40
                enabled: configManager && configManager.isDirty

                background: Rectangle {
                    color: saveButton.hovered ? "#4320c4" : "#2e15b0"
                    opacity: saveButton.enabled ? 1.0 : 0.5
                    radius: 8
                }

                contentItem: Text {
                    text: saveButton.text
                    color: Constants.textPrimary
                    font.pixelSize: 14
                    font.family: Constants.font.family
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    console.log("Save button clicked")
                    if (configManager) {
                        console.log("ConfigManager exists, isDirty:", configManager.isDirty)
                        console.log("Claude API key:", configManager.claudeApiKey)
                        var success = configManager.save()
                        console.log("Save result:", success)
                        if (success) {
                            savedNotification.show()
                        }
                    } else {
                        console.log("ConfigManager is null!")
                    }
                }
            }
        }
    }

    // Save notification
    Rectangle {
        id: savedNotification
        anchors.bottom: footer.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 20
        width: 200
        height: 40
        radius: 20
        color: "#22c55e"
        opacity: 0
        visible: opacity > 0

        Text {
            anchors.centerIn: parent
            text: "Settings saved"
            color: "white"
            font.pixelSize: 14
            font.family: Constants.font.family
        }

        function show() {
            showAnimation.start()
        }

        SequentialAnimation {
            id: showAnimation
            NumberAnimation { target: savedNotification; property: "opacity"; to: 1.0; duration: 200 }
            PauseAnimation { duration: 2000 }
            NumberAnimation { target: savedNotification; property: "opacity"; to: 0.0; duration: 300 }
        }
    }

    // API Key Input Component
    component ApiKeyInput: Column {
        property string label: ""
        property string placeholder: ""
        property string value: ""
        property bool showKey: false
        property var targetConfigManager: null

        signal edited(string newValue)

        spacing: 6

        // Track if we're currently updating to prevent loops
        property bool _updating: false

        // Sync external value changes to TextInput (only when not focused)
        onValueChanged: {
            if (!_updating && !keyInput.activeFocus && keyInput.text !== value) {
                keyInput.text = value
            }
        }

        Text {
            text: label
            color: Constants.textSecondary
            font.pixelSize: 13
            font.family: Constants.font.family
        }

        TextField {
            id: keyInput
            width: parent.width
            height: 36
            color: Constants.textPrimary
            font.pixelSize: 14
            font.family: Constants.font.family
            echoMode: showKey ? TextInput.Normal : TextInput.Password
            placeholderText: placeholder
            placeholderTextColor: Constants.textDisabled
            selectByMouse: true
            activeFocusOnPress: true

            background: Rectangle {
                color: Constants.surface
                radius: 6
                border.color: keyInput.activeFocus ? "#2e15b0" : "transparent"
                border.width: 1
            }

            rightPadding: 40

            onTextChanged: {
                if (!_updating && text !== value) {
                    _updating = true
                    edited(text)
                    _updating = false
                }
            }

            Component.onCompleted: text = value

            // Show/hide toggle
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 24
                height: 24
                radius: 4
                color: toggleArea.containsMouse ? Constants.surfaceElevated : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: showKey ? "\u{1F441}" : "\u{1F576}"
                    font.pixelSize: 14
                }

                MouseArea {
                    id: toggleArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: showKey = !showKey
                }
            }
        }
    }
}
