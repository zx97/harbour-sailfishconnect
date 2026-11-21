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
#include <QVariantMap>

/// Exposes the system audio sinks to the peer and applies its volume changes.
///
/// PulseAudio is reached through its D-Bus interface (org.PulseAudio1).
class SystemVolumePlugin : public KdeConnectPlugin
{
    Q_OBJECT

public:
    SystemVolumePlugin(QObject *parent, const QVariantList &args);

    void connected() override;
    void receivePacket(const NetworkPacket &np) override;

private slots:
    void sendSinkList();
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed,
                             const QStringList &invalidated);

private:
    struct Sink
    {
        QString path;
        QString name;
        QString description;
        int volume = 0;
        bool muted = false;
    };

    void refreshSinks();
    Sink readSink(const QString &path) const;
    QString sinkPathForName(const QString &name) const;
    bool applyVolume(const QString &name, int volume);
    bool applyMute(const QString &name, bool muted);

    QList<Sink> m_sinks;
};
