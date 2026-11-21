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

#include "presenterplugin.h"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(PresenterPlugin, "sailfishconnect_presenter.json")

namespace {

const QString packetTypePresenter = QStringLiteral("kdeconnect.presenter");

} // namespace

PresenterPlugin::PresenterPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
{
}

void PresenterPlugin::receivePacket(const NetworkPacket &np)
{
    Q_UNUSED(np)
}

QString PresenterPlugin::dbusPath() const
{
    return QStringLiteral("/modules/kdeconnect/devices/%1/sailfishconnect_presenter")
        .arg(device()->id());
}

void PresenterPlugin::moveCursor(double dx, double dy)
{
    NetworkPacket np(packetTypePresenter);
    np.set(QStringLiteral("dx"), dx);
    np.set(QStringLiteral("dy"), dy);
    sendPacket(np);
}

void PresenterPlugin::stop()
{
    NetworkPacket np(packetTypePresenter);
    np.set(QStringLiteral("stop"), true);
    sendPacket(np);
}

#include "presenterplugin.moc"
