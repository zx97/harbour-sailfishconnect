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

#include "connectivityreportplugin.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QLoggingCategory>
#include <QTimer>

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(ConnectivityReportPlugin, "sailfishconnect_connectivityreport.json")

Q_LOGGING_CATEGORY(loggerConnectivityReport, "kdeconnect.plugin.connectivityreport")

namespace {

const QString packetTypeConnectivityReport =
    QStringLiteral("kdeconnect.connectivity_report");

// oFono reports the access technology as a lowercase string; the protocol
// and the peer's icon naming expect the usual uppercase names.
QString networkTypeForTechnology(const QString &technology)
{
    const QString value = technology.toLower();
    if (value == QLatin1String("lte")) {
        return QStringLiteral("LTE");
    }
    if (value == QLatin1String("hspa") || value == QLatin1String("hspa+")) {
        return QStringLiteral("HSPA");
    }
    if (value == QLatin1String("umts")) {
        return QStringLiteral("UMTS");
    }
    if (value == QLatin1String("edge")) {
        return QStringLiteral("EDGE");
    }
    if (value == QLatin1String("gsm")) {
        return QStringLiteral("GPRS");
    }
    if (value == QLatin1String("nr") || value.contains(QLatin1String("5g"))) {
        return QStringLiteral("5G");
    }
    return QString();
}

// The protocol uses Android's SignalStrength levels (0-4).
int signalLevel(int strength)
{
    if (strength <= 0) {
        return 0;
    }
    if (strength < 25) {
        return 1;
    }
    if (strength < 50) {
        return 2;
    }
    if (strength < 75) {
        return 3;
    }
    return 4;
}

QVariantMap readProperties(const QString &modem)
{
    QDBusMessage getProperties = QDBusMessage::createMethodCall(
        QStringLiteral("org.ofono"), modem,
        QStringLiteral("org.ofono.NetworkRegistration"),
        QStringLiteral("GetProperties"));
    const QDBusMessage reply = QDBusConnection::systemBus().call(
        getProperties, QDBus::Block, 3000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return {};
    }

    const QVariant value = reply.arguments().value(0);
    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        return qdbus_cast<QVariantMap>(value.value<QDBusArgument>());
    }
    return value.toMap();
}

} // namespace

ConnectivityReportPlugin::ConnectivityReportPlugin(QObject *parent,
                                                   const QVariantList &args)
    : KdeConnectPlugin(parent, args)
{
    // The report also has to be sent when this plugin is instantiated after
    // the link is already up (connected() is not called then).
    QTimer::singleShot(2000, this, &ConnectivityReportPlugin::sendReport);
}

ConnectivityReportPlugin::~ConnectivityReportPlugin() = default;

void ConnectivityReportPlugin::connected()
{
    sendReport();
}

void ConnectivityReportPlugin::receivePacket(const NetworkPacket &np)
{
    Q_UNUSED(np)
}

void ConnectivityReportPlugin::watchModem(const QString &modem)
{
    if (m_watchedModems.contains(modem)) {
        return;
    }

    const bool connected = QDBusConnection::systemBus().connect(
        QStringLiteral("org.ofono"), modem,
        QStringLiteral("org.ofono.NetworkRegistration"),
        QStringLiteral("PropertyChanged"), this, SLOT(sendReport()));
    if (connected) {
        m_watchedModems.append(modem);
    }
}

void ConnectivityReportPlugin::sendReport()
{
    // The SIM can be in either slot, ofono exposes one object per slot.
    const QStringList modems{QStringLiteral("/ril_0"), QStringLiteral("/ril_1")};
    for (const QString &modem : modems) {
        const QVariantMap properties = readProperties(modem);
        if (properties.isEmpty()) {
            continue;
        }
        watchModem(modem);

        const QString status = properties.value(QStringLiteral("Status")).toString();
        if (status != QLatin1String("registered")
            && status != QLatin1String("roaming")) {
            continue;
        }

        QVariantMap subscription;
        subscription.insert(QStringLiteral("name"),
                            properties.value(QStringLiteral("Name")).toString());
        subscription.insert(
            QStringLiteral("networkType"),
            networkTypeForTechnology(
                properties.value(QStringLiteral("Technology")).toString()));
        subscription.insert(
            QStringLiteral("signalStrength"),
            signalLevel(properties.value(QStringLiteral("Strength")).toInt()));
        subscription.insert(QStringLiteral("slot"), 0);

        QVariantMap subscriptions;
        subscriptions.insert(QString::number(0), subscription);

        NetworkPacket np(packetTypeConnectivityReport);
        np.set(QStringLiteral("signalStrengths"), subscriptions);
        sendPacket(np);
        return;
    }
}

#include "connectivityreportplugin.moc"
