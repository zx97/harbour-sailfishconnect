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

#include <QList>
#include <QString>

class QProcess;
class QTimer;

/// Exposes the system audio sinks to the peer and applies its volume changes.
///
/// PulseAudio is driven through pactl: its JSON output is stable and avoids
/// depending on the optional D-Bus module, which Sailfish OS does not load.
class SystemVolumePlugin : public KdeConnectPlugin
{
    Q_OBJECT

public:
    SystemVolumePlugin(QObject *parent, const QVariantList &args);
    ~SystemVolumePlugin() override;

    void connected() override;
    void receivePacket(const NetworkPacket &np) override;

private slots:
    void sendSinkList();
    void refreshFromPulseAudio();

private:
    struct Sink
    {
        QString name;
        QString description;
        int volume = 0;
        bool muted = false;
        bool isDefault = false;
    };

    QList<Sink> readSinks() const;
    QString defaultSink() const;
    void startEventSubscription();

    QList<Sink> m_sinks;
    QProcess *m_subscription = nullptr;
    QTimer *m_refreshTimer = nullptr;
};
