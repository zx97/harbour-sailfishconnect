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

#pragma once

#include <core/kdeconnectplugin.h>

/// Turns the phone into a presenter remote: the peer displays a pointer that
/// the phone moves.
///
/// Only the outgoing direction exists: the peer draws the pointer, so this
/// plugin has no packet to receive.
class PresenterPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.presenter")

public:
    explicit PresenterPlugin(QObject *parent, const QVariantList &args);

    void receivePacket(const NetworkPacket &np) override;

    QString dbusPath() const override;

public Q_SLOTS:
    /// Moves the peer pointer by a fraction of the peer screen size.
    Q_SCRIPTABLE void moveCursor(double dx, double dy);

    /// Hides the peer pointer.
    Q_SCRIPTABLE void stop();
};
