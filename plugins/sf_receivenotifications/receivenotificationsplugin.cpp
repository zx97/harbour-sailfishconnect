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

#include "receivenotificationsplugin.h"

#include <KPluginFactory>
#include <QLoggingCategory>

#include <notification.h>

K_PLUGIN_CLASS_WITH_JSON(ReceiveNotificationsPlugin, "sailfishconnect_receivenotifications.json")

Q_LOGGING_CATEGORY(loggerReceiveNotifications, "kdeconnect.plugin.receivenotifications")

static const QString packetTypeNotification = QStringLiteral("kdeconnect.notification");

ReceiveNotificationsPlugin::ReceiveNotificationsPlugin(QObject *parent,
                                                       const QVariantList &args)
    : KdeConnectPlugin(parent, args)
{
}

ReceiveNotificationsPlugin::~ReceiveNotificationsPlugin()
{
    const auto notifications = m_notifications;
    for (Notification *notification : notifications) {
        notification->close();
        notification->deleteLater();
    }
    m_notifications.clear();
}

void ReceiveNotificationsPlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() != packetTypeNotification) {
        return;
    }

    const QString id = np.get<QString>(QStringLiteral("id"));
    if (np.get<bool>(QStringLiteral("isCancel"), false)) {
        closeNotification(id);
        return;
    }

    showNotification(np);
}

void ReceiveNotificationsPlugin::showNotification(const NetworkPacket &np)
{
    const QString id = np.get<QString>(QStringLiteral("id"));
    const QString appName = np.get<QString>(QStringLiteral("appName"));
    const QString title = np.get<QString>(QStringLiteral("title"));
    const QString text = np.get<QString>(QStringLiteral("text"));

    Notification *notification = m_notifications.value(id);
    if (!notification) {
        notification = new Notification(this);
        notification->setAppName(tr("KDE Connect"));
        notification->setCategory(QStringLiteral("x-nemo.service.sailfishconnect"));
        notification->setUrgency(Notification::Normal);
        notification->setIcon(QStringLiteral("icon-m-information"));
        notification->setSubText(appName);
        m_notifications.insert(id, notification);
    }

    // The summary is the notification title, fall back to the application name
    // for notifications which only carry a body.
    notification->setSummary(title.isEmpty() ? appName : title);
    notification->setPreviewBody(text);
    notification->setBody(text);
    notification->publish();
}

void ReceiveNotificationsPlugin::closeNotification(const QString &id)
{
    Notification *notification = m_notifications.take(id);
    if (!notification) {
        return;
    }

    notification->close();
    notification->deleteLater();
}

#include "receivenotificationsplugin.moc"
