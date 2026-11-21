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

    MouseArea {
        id: pad

        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: controls.top
        }

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

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: Theme.secondaryColor
            text: i18n("Swipe on the upper area to move the pointer of the computer")
        }

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
