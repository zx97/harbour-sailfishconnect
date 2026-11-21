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

#include <QHash>

class Notification;

/// Shows notifications sent by the peer on the device.
///
/// The desktop sends kdeconnect.notification packets (and isCancel packets to
/// remove them again); each one is turned into a Sailfish OS notification,
/// reusing the same notification for updates of the same id.
class ReceiveNotificationsPlugin : public KdeConnectPlugin
{
    Q_OBJECT

public:
    ReceiveNotificationsPlugin(QObject *parent, const QVariantList &args);
    ~ReceiveNotificationsPlugin() override;

    void connected() override
    {
    }
    void receivePacket(const NetworkPacket &np) override;

private:
    void showNotification(const NetworkPacket &np);
    void closeNotification(const QString &id);

    QHash<QString, Notification *> m_notifications;
};
