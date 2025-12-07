pragma Singleton
import QtQuick

QtObject {
    readonly property int width: 1920
    readonly property int height: 1080

    readonly property font font: Qt.font({
        family: "Segoe UI",
        pixelSize: 14
    })

    readonly property font largeFont: Qt.font({
        family: "Segoe UI",
        pixelSize: 22
    })

    // Background Colors
    readonly property color backgroundColor: "#1e1e1e"
    readonly property color backgroundSecondary: "#252526"
    readonly property color surface: "#2d2d30"
    readonly property color surfaceElevated: "#3e3e42"

    // Text Colors
    readonly property color textPrimary: "#ffffff"
    readonly property color textSecondary: "#d4d4d4"
    readonly property color textMuted: "#bbbbbb"
    readonly property color textDisabled: "#858585"
}
