/*
 * Copyright 2026 Manuel FLURY <zx97@users.noreply.github.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property QtObject device

    // Portion of the peer screen covered by a full swipe across the pad.
    property real sensitivity: 1.4

    function sendMove(dx, dy) {
        if (!device) {
            return
        }
        device.pluginCallWithArgs(
            "sailfishconnect_presenter", "moveCursor", [ dx, dy ])
    }

    PageHeader {
        id: header
        title: i18n("Presenter")
    }

    // The margins keep the pad away from the screen edges, where the system
    // back gesture would take over the swipe.
    Rectangle {
        id: padFrame

        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: controls.top
            topMargin: Theme.paddingLarge
            bottomMargin: Theme.paddingLarge
            leftMargin: Theme.itemSizeSmall
            rightMargin: Theme.itemSizeSmall
        }

        radius: Theme.paddingMedium
        color: Theme.rgba(Theme.highlightBackgroundColor,
                          pad.pressed ? 0.20 : 0.08)
        border.width: 1
        border.color: pad.pressed ? Theme.highlightColor
                                  : Theme.secondaryHighlightColor

        Label {
            anchors.centerIn: parent
            width: parent.width - 2 * Theme.paddingLarge
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: Theme.secondaryColor
            visible: !pad.pressed
            text: i18n("Move your finger inside this area to move the pointer of the computer")
        }

        MouseArea {
            id: pad

            anchors.fill: parent
            preventStealing: true

            property real lastX: 0
            property real lastY: 0

            onPressed: {
                lastX = mouse.x
                lastY = mouse.y
            }

            onPositionChanged: {
                if (!pressed) {
                    return
                }

                var dx = (mouse.x - lastX) / width * sensitivity
                var dy = (mouse.y - lastY) / height * sensitivity
                lastX = mouse.x
                lastY = mouse.y
                page.sendMove(dx, dy)
            }
        }
    }

    // The peer hides its pointer when it receives nothing for a moment, so
    // keep it alive while the finger stays on the pad.
    Timer {
        interval: 250
        repeat: true
        running: pad.pressed
        onTriggered: page.sendMove(0, 0)
    }

    Column {
        id: controls

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: Theme.paddingLarge
            bottomMargin: Theme.paddingLarge
        }

        spacing: Theme.paddingMedium

        Button {
            width: parent.width
            text: i18n("Stop")
            onClicked: {
                if (device) {
                    device.pluginCall("sailfishconnect_presenter", "stop")
                }
                pageStack.pop()
            }
        }
    }
}
