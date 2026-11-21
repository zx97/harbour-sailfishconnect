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

/// Reports the cellular network type and signal strength to the peer.
///
/// The values come from oFono's org.ofono.NetworkRegistration interface; the
/// report is sent when the peer connects and whenever the registration
/// properties change.
class ConnectivityReportPlugin : public KdeConnectPlugin
{
    Q_OBJECT

public:
    ConnectivityReportPlugin(QObject *parent, const QVariantList &args);
    ~ConnectivityReportPlugin() override;

    void connected() override;
    void receivePacket(const NetworkPacket &np) override;

private slots:
    void sendReport();

private:
    void watchModem(const QString &modem);

    QStringList m_watchedModems;
};
