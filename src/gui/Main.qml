import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    visible: true
    width: 1280; height: 840
    minimumWidth: 960; minimumHeight: 640
    title: "DiskPilot — Understand your storage"
    color: "#0b1220"
    property int page: 0
    property string exportFormat: "html"
    property var pages: ["Dashboard", "Disk Explorer", "Treemap", "Large Files", "Duplicate Finder", "Developer Cleanup", "Scan History", "Reports", "Settings"]
    palette.window: "#0b1220"
    palette.windowText: "#e3eaf5"
    palette.base: "#111e30"
    palette.text: "#e3eaf5"
    palette.button: "#1d3048"
    palette.buttonText: "#e3eaf5"
    palette.highlight: "#51cec1"
    palette.highlightedText: "#081624"

    component Body: Label {
        color: "#a5b5ca"; font.pixelSize: 13
        textFormat: Text.PlainText; wrapMode: Text.Wrap
    }
    component Heading: Label {
        color: "#e3eaf5"; font.pixelSize: 24; font.weight: Font.DemiBold
        textFormat: Text.PlainText
    }
    component Action: Button {
        id: button
        implicitHeight: 38
        contentItem: Label {
            text: button.text; textFormat: Text.PlainText
            color: button.enabled ? "#deebf7" : "#64748b"
            font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 7; implicitWidth: 120
            color: button.down ? "#28515b" : button.hovered ? "#263e58" : "#192b41"
            border.color: button.activeFocus ? "#51cec1" : "#2a3d55"
        }
    }
    component Metric: Rectangle {
        property string label
        property string value
        implicitHeight: 115; color: "#132135"; radius: 12
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 20
            Body {text: label}
            Label {text: value; textFormat: Text.PlainText; color: "#67dfcf"; font.pixelSize: 25; font.weight: Font.DemiBold}
        }
    }
    component Files: ListView {
        id: files
        property var entries: []
        clip: true; spacing: 5; model: entries
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            required property var modelData
            width: files.width; height: 66; radius: 7; color: "#132135"
            RowLayout {
                anchors.fill: parent; anchors.margins: 10; spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 3
                    Label {
                        text: (modelData.directory ? "▸  " : "") + modelData.name
                        textFormat: Text.PlainText; color: modelData.directory ? "#67dfcf" : "#e3eaf5"
                        font.pixelSize: 14; elide: Text.ElideMiddle; Layout.fillWidth: true
                        MouseArea {anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: if (modelData.directory) pilot.navigate(modelData.node)}
                    }
                    Label {text: modelData.path; textFormat: Text.PlainText; color: "#8094ae"; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.fillWidth: true}
                }
                Body {text: modelData.size; Layout.preferredWidth: 100}
                Body {text: Number(modelData.percent).toFixed(1) + "%"; Layout.preferredWidth: 52}
                Action {text: "Location"; enabled: !pilot.busy; onClicked: pilot.openLocation(modelData.node)}
                Action {text: "Copy path"; onClicked: pilot.copyPath(modelData.node)}
            }
        }
    }

    FolderDialog {id: chooseFolder; onAccepted: pilot.chooseTarget(selectedFolder)}
    FileDialog {
        id: reportDialog; fileMode: FileDialog.SaveFile; title: "Export to a new filename"
        nameFilters: [window.exportFormat.toUpperCase() + " report (*." + window.exportFormat + ")"]
        onAccepted: pilot.exportReport(selectedFile, window.exportFormat)
    }
    FileDialog {id: saveDialog; fileMode: FileDialog.SaveFile; nameFilters: ["Snapshot (*.json)"]; onAccepted: pilot.saveSnapshot(selectedFile)}
    FileDialog {id: compareDialog; fileMode: FileDialog.OpenFile; nameFilters: ["Snapshot (*.json)"]; onAccepted: pilot.compareSnapshot(selectedFile)}

    RowLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.preferredWidth: 202; Layout.fillHeight: true; color: "#101b2c"
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 18; spacing: 7
                Heading {text: "DiskPilot"; color: "#67dfcf"; font.pixelSize: 25; Layout.topMargin: 12}
                Body {text: "STORAGE, UNDERSTOOD"; font.pixelSize: 10; Layout.bottomMargin: 26}
                Repeater {
                    model: window.pages
                    delegate: Button {
                        required property string modelData
                        required property int index
                        Layout.fillWidth: true; implicitHeight: 43
                        onClicked: window.page = index
                        contentItem: Label {text: modelData; textFormat: Text.PlainText; color: window.page === index ? "#67dfcf" : "#a5b5ca"; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter}
                        background: Rectangle {radius: 7; color: window.page === index ? "#1a3441" : "transparent"}
                    }
                }
                Item {Layout.fillHeight: true}
                Body {text: "LOCAL & OFFLINE\nNo accounts. No telemetry."; font.pixelSize: 11; Layout.bottomMargin: 8}
                Body {text: "Development build\nTrash execution disabled"; color: "#e0b36b"; font.pixelSize: 10}
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: 26; spacing: 16
            RowLayout {
                Layout.fillWidth: true
                Heading {text: window.pages[window.page]; Layout.fillWidth: true}
                Body {text: pilot.busy ? "WORKING" : pilot.hasScan ? "LOCAL RESULTS" : "READY"; color: "#67dfcf"; font.pixelSize: 11}
            }
            RowLayout {
                Layout.fillWidth: true
                TextField {id: targetField; Layout.fillWidth: true; placeholderText: "Select a folder or enter its full path"; text: pilot.target; enabled: !pilot.busy; onEditingFinished: pilot.target = text; onAccepted: {pilot.target = text; pilot.scan()}}
                Action {text: "Browse"; enabled: !pilot.busy; onClicked: chooseFolder.open()}
                Action {text: "Scan folder"; enabled: !pilot.busy && targetField.text.length > 0; onClicked: {pilot.target = targetField.text; pilot.scan()}}
                Action {text: "Cancel"; visible: pilot.busy; onClicked: pilot.cancel()}
            }
            ProgressBar {Layout.fillWidth: true; indeterminate: true; visible: pilot.busy}
            Body {text: pilot.status; Layout.fillWidth: true; color: "#e0b36b"}

            StackLayout {
                currentIndex: window.page; Layout.fillWidth: true; Layout.fillHeight: true
                ScrollView {
                    clip: true; contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width; spacing: 20
                        Body {text: pilot.hasScan ? pilot.summary.root : "Your files stay on this computer. Start a read-only scan to see real storage data."; Layout.fillWidth: true}
                        GridLayout {
                            columns: 3; Layout.fillWidth: true; columnSpacing: 12; rowSpacing: 12
                            Metric {label: "Scanned logical size"; value: pilot.summary.logical || "—"; Layout.fillWidth: true}
                            Metric {label: "Unique file allocation"; value: pilot.summary.allocated || "—"; Layout.fillWidth: true}
                            Metric {label: "Volume free space"; value: pilot.summary.free || "—"; Layout.fillWidth: true}
                            Metric {label: "Files"; value: pilot.hasScan ? String(pilot.summary.files) : "—"; Layout.fillWidth: true}
                            Metric {label: "Directories"; value: pilot.hasScan ? String(pilot.summary.directories) : "—"; Layout.fillWidth: true}
                            Metric {label: "Scan duration"; value: pilot.summary.duration || "—"; Layout.fillWidth: true}
                        }
                        Body {text: pilot.hasScan ? (pilot.summary.complete ? "Complete within link and mount policy." : "Partial scan. Totals do not represent a complete inventory.") + " Errors: " + pilot.summary.errors + " · Access denied: " + pilot.summary.accessDenied + " · Skipped links: " + pilot.summary.links : "Links and mount crossings are skipped by default."; color: "#e0b36b"; Layout.fillWidth: true}
                        Body {text: "Volume capacity, used space and scanned file sizes measure different things. Permissions, hard links, sparse files, compression and filesystem metadata can explain differences."; Layout.fillWidth: true}
                        Heading {text: "File types"; font.pixelSize: 18}
                        Repeater {model: pilot.categories; delegate: Body {required property var modelData; text: modelData.extension + "   " + modelData.size}}
                        Body {text: pilot.summary.errorDetails || ""; Layout.fillWidth: true; color: "#e0b36b"}
                    }
                }
                ColumnLayout {
                    RowLayout {
                        Action {text: "Root"; enabled: pilot.hasScan && !pilot.busy; onClicked: pilot.home()}
                        Action {text: "Parent folder"; enabled: pilot.hasScan && !pilot.busy; onClicked: pilot.up()}
                        Body {text: pilot.currentPath; elide: Text.ElideMiddle; wrapMode: Text.NoWrap; Layout.fillWidth: true}
                    }
                    Body {text: "Largest 2,000 direct children • Logical bytes • % of parent. Select a folder name to navigate."; Layout.fillWidth: true}
                    Files {entries: visible ? pilot.rows : []; Layout.fillWidth: true; Layout.fillHeight: true}
                }
                ColumnLayout {
                    RowLayout {
                        Action {text: "Root"; enabled: pilot.hasScan && !pilot.busy; onClicked: pilot.home()}
                        Action {text: "Parent folder"; enabled: pilot.hasScan && !pilot.busy; onClicked: pilot.up()}
                        Body {text: pilot.currentPath; Layout.fillWidth: true; elide: Text.ElideMiddle; wrapMode: Text.NoWrap}
                    }
                    Body {text: "Double-click a folder tile to zoom. Hover for details. Up to 1,000 largest direct children are drawn; smaller children are omitted, and remaining areas are proportional to shown weights."; Layout.fillWidth: true}
                    Canvas {
                        id: map; Layout.fillWidth: true; Layout.fillHeight: true
                        property var rectangles: []
                        property string detail: ""
                        function refresh() {rectangles = pilot.tiles(width, height); requestPaint()}
                        onWidthChanged: Qt.callLater(refresh)
                        onHeightChanged: Qt.callLater(refresh)
                        onVisibleChanged: if(visible)refresh()
                        Connections {target: pilot; function onStateChanged() {if(map.visible)map.refresh()}}
                        onPaint: {
                            const ctx = getContext("2d"); ctx.reset(); ctx.fillStyle = "#101b2c"; ctx.fillRect(0,0,width,height)
                            const colors = ["#1a6876", "#356d87", "#53649a", "#256b61", "#67548d"]
                            for (let tile of rectangles) {
                                ctx.fillStyle = colors[tile.node % colors.length]; ctx.fillRect(tile.x+1,tile.y+1,Math.max(0,tile.w-2),Math.max(0,tile.h-2))
                                if (tile.w > 85 && tile.h > 38) {
                                    ctx.save(); ctx.beginPath(); ctx.rect(tile.x+5,tile.y+4,tile.w-10,tile.h-8); ctx.clip()
                                    ctx.fillStyle = "#edf7fb"; ctx.font = "12px sans-serif"; ctx.fillText(tile.name,tile.x+9,tile.y+20)
                                    ctx.font = "10px sans-serif"; ctx.fillText(tile.size,tile.x+9,tile.y+36); ctx.restore()
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent; hoverEnabled: true
                            function hit(x,y) {for (let t of map.rectangles) if (x>=t.x && y>=t.y && x<t.x+t.w && y<t.y+t.h) return t; return null}
                            onPositionChanged: function(mouse) {let t=hit(mouse.x,mouse.y);map.detail=t ? t.path + "\nLogical: " + t.size + " · Allocated: " + t.allocated + " · " + Number(t.percent).toFixed(2) + "% of parent" : ""}
                            onDoubleClicked: function(mouse) {let t=hit(mouse.x,mouse.y);if(t && t.directory)pilot.navigate(t.node)}
                            ToolTip {
                                id: tileTip
                                visible: parent.containsMouse && map.detail.length > 0
                                text: map.detail
                                contentItem: Label {text: tileTip.text; textFormat: Text.PlainText; color: "#e3eaf5"; wrapMode: Text.Wrap}
                            }
                        }
                    }
                }
                ColumnLayout {
                    RowLayout {
                        TextField {id: search; placeholderText: "Filename contains (case sensitive)"; Layout.fillWidth: true; onTextEdited: pilot.setFilter(text,minimum.value)}
                        Body {text: "Minimum MiB"}
                        SpinBox {id: minimum; from: 0; to: 1000000; editable: true; onValueModified: pilot.setFilter(search.text,value)}
                    }
                    Body {text: "Largest 1,000 matching files. A large file is not a cleanup recommendation."; Layout.fillWidth: true}
                    Files {entries: visible ? pilot.largest : []; Layout.fillWidth: true; Layout.fillHeight: true}
                }
                ColumnLayout {
                    RowLayout {
                        Action {text: "Verify duplicates"; enabled: pilot.hasScan && !pilot.busy; onClicked: pilot.findDuplicates()}
                        Body {text: "Size → partial SHA-256 → full SHA-256 → byte comparison"; Layout.fillWidth: true}
                    }
                    Body {text: "Distinct file identities only. Nothing is preselected. At least one copy stays unselected. Selection is for review; deletion is disabled. View is limited to 2,000 files; export JSON for all findings."; Layout.fillWidth: true}
                    ListView {
                        id: duplicates; model: visible ? pilot.duplicateRows : []; clip: true; spacing: 6; Layout.fillWidth: true; Layout.fillHeight: true
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property var modelData
                            width: duplicates.width; height: 82; color: "#132135"; radius: 8
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 10
                                CheckBox {checked: modelData.selected; enabled: !pilot.busy; onToggled: pilot.selectDuplicate(modelData.node,checked)}
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Body {text: "Group " + modelData.group + " · " + modelData.verification + " · " + modelData.size; color: "#67dfcf"}
                                    Body {text: modelData.path; Layout.fillWidth: true; elide: Text.ElideMiddle; wrapMode: Text.NoWrap}
                                }
                                Action {text: "Location"; enabled: !pilot.busy; onClicked: pilot.openLocation(modelData.node)}
                            }
                        }
                    }
                }
                ColumnLayout {
                    Body {text: "Candidates require project context and manual review. Source-bearing trees, links, protected paths and changed contents cannot enter a cleanup plan. No file is guaranteed safe to remove."; Layout.fillWidth: true}
                    Action {text: "Preview selected candidates"; enabled: pilot.hasScan && !pilot.busy; onClicked: pilot.previewCleanup()}
                    ListView {
                        id: candidates; model: visible ? pilot.cleanupRows : []; clip: true; spacing: 8; Layout.fillWidth: true; Layout.fillHeight: true
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property var modelData
                            width: candidates.width; height: content.implicitHeight + 24; color: "#132135"; radius: 8
                            RowLayout {
                                id: content; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 12
                                CheckBox {checked: modelData.selected; enabled: !pilot.busy && !modelData.protected; onToggled: pilot.selectCleanup(modelData.index,checked)}
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Body {text: modelData.ecosystem + " · " + modelData.type + " · " + modelData.size; color: "#67dfcf"}
                                    Body {text: modelData.path; Layout.fillWidth: true}
                                    Body {text: "Evidence: " + modelData.evidence; Layout.fillWidth: true}
                                    Body {text: modelData.rebuild; Layout.fillWidth: true}
                                    Body {text: modelData.warning; Layout.fillWidth: true; color: "#e0b36b"}
                                }
                            }
                        }
                    }
                    ScrollView {visible: pilot.planPreview.length > 0; Layout.fillWidth: true; Layout.preferredHeight: 190; TextArea {text: pilot.planPreview; readOnly: true; wrapMode: Text.Wrap; textFormat: TextEdit.PlainText}}
                }
                ColumnLayout {
                    Body {text: "Save local metadata snapshots, then compare the current complete scan with a previous snapshot of the same folder. Renames appear as removal plus addition; equal size does not prove unchanged content."; Layout.fillWidth: true}
                    RowLayout {
                        Action {text: "Save snapshot"; enabled: pilot.hasScan && !pilot.busy; onClicked: saveDialog.open()}
                        Action {text: "Compare with snapshot"; enabled: pilot.hasScan && !pilot.busy; onClicked: compareDialog.open()}
                    }
                    ScrollView {Layout.fillWidth: true; Layout.fillHeight: true; TextArea {text: pilot.comparison || "No comparison loaded."; readOnly: true; wrapMode: Text.Wrap; textFormat: TextEdit.PlainText}}
                }
                ColumnLayout {
                    Heading {text: "Take your findings with you"; font.pixelSize: 20}
                    Body {text: "JSON and HTML include scan summaries, errors, duplicate verification and developer cleanup candidates. CSV exports the scan table. Reports contain local paths, never file contents. Choose a new filename; existing files are preserved."; Layout.fillWidth: true}
                    RowLayout {
                        Repeater {
                            model: ["html","json","csv"]
                            delegate: Action {required property string modelData; text: "Export " + modelData.toUpperCase(); enabled: pilot.hasScan && !pilot.busy; onClicked: {window.exportFormat=modelData;reportDialog.open()}}
                        }
                    }
                    Item {Layout.fillHeight: true}
                }
                ColumnLayout {
                    Heading {text: "Scan & privacy"; font.pixelSize: 20}
                    Body {text: "Maximum retained entries (partial results are clearly labelled):"; Layout.fillWidth: true}
                    SpinBox {from: 100; to: 2000000; stepSize: 10000; editable: true; value: pilot.entryLimit; enabled: !pilot.busy; onValueModified: pilot.entryLimit=value}
                    Body {text: "Scanning uses one bounded worker. Links, junctions, reparse points and mount crossings are skipped. No content is read during ordinary scanning. Duplicate verification streams bounded buffers.\n\nPreferences and metadata exports stay local. There is no telemetry, account, cloud upload or project-script execution.\n\nCleanup execution is unavailable until the native trash adapter has passed race-condition and recovery tests. Permanent deletion is not implemented."; Layout.fillWidth: true}
                    Item {Layout.fillHeight: true}
                }
            }
        }
    }
}
