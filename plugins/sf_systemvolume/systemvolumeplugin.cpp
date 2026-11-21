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

#include "systemvolumeplugin.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QLoggingCategory>

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(SystemVolumePlugin, "sailfishconnect_systemvolume.json")

Q_LOGGING_CATEGORY(loggerSystemVolume, "kdeconnect.plugin.systemvolume")

namespace {

const QString pulseAudioService = QStringLiteral("org.PulseAudio1");
const QString pulseAudioCorePath = QStringLiteral("/org/pulseaudio/core1");
const QString pulseAudioCoreInterface = QStringLiteral("org.PulseAudio.Core1");
const QString pulseAudioDeviceInterface = QStringLiteral("org.PulseAudio.Core1.Device");
const QString pulseAudioSinkInterface = QStringLiteral("org.PulseAudio.Core1.Sink");
const QString propertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

// PulseAudio's internal volume unit; the protocol uses percentages.
constexpr int pulseVolumeNorm = 65536;
constexpr int protocolMaxVolume = 100;

const QString packetTypeSystemVolume = QStringLiteral("kdeconnect.systemvolume");
const QString packetTypeSystemVolumeRequest =
    QStringLiteral("kdeconnect.systemvolume.request");

QVariant callProperties(const QString &path, const QString &method,
                        const QVariantList &arguments = {})
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        pulseAudioService, path, propertiesInterface, method);
    for (const QVariant &argument : arguments) {
        call << argument;
    }
    const QDBusMessage reply = QDBusConnection::sessionBus().call(
        call, QDBus::Block, 2000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return {};
    }
    return reply.arguments().value(0);
}

QVariantMap getAll(const QString &path, const QString &interface)
{
    const QVariant value = callProperties(path, QStringLiteral("GetAll"), {interface});
    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        return qdbus_cast<QVariantMap>(value.value<QDBusArgument>());
    }
    return value.toMap();
}

int volumePercent(const QVariant &value)
{
    QVariantList channels;
    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        channels = qdbus_cast<QVariantList>(value.value<QDBusArgument>());
    } else {
        channels = value.toList();
    }
    if (channels.isEmpty()) {
        return 0;
    }

    const int channel = channels.first().toInt();
    return qBound(0, qRound(double(channel) * protocolMaxVolume / pulseVolumeNorm),
                  protocolMaxVolume);
}

QList<uint> volumeChannels(int percent)
{
    const uint value = uint(qRound(double(qBound(0, percent, protocolMaxVolume))
                                   * pulseVolumeNorm / protocolMaxVolume));
    // Mono is enough: PulseAudio applies the value to every channel.
    return {value};
}

} // namespace

SystemVolumePlugin::SystemVolumePlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
{
}

void SystemVolumePlugin::connected()
{
    sendSinkList();
}

void SystemVolumePlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() != packetTypeSystemVolumeRequest) {
        return;
    }

    refreshSinks();
    if (np.get<bool>(QStringLiteral("requestSinks"), false)) {
        sendSinkList();
        return;
    }

    const QString name = np.get<QString>(QStringLiteral("name"));
    if (name.isEmpty()) {
        return;
    }

    if (np.has(QStringLiteral("volume"))) {
        applyVolume(name, np.get<int>(QStringLiteral("volume")));
    }
    if (np.has(QStringLiteral("muted"))) {
        applyMute(name, np.get<bool>(QStringLiteral("muted")));
    }
}

SystemVolumePlugin::Sink SystemVolumePlugin::readSink(const QString &path) const
{
    Sink sink;
    sink.path = path;

    QVariantMap properties = getAll(path, pulseAudioSinkInterface);
    const QVariantMap deviceProperties = getAll(path, pulseAudioDeviceInterface);
    for (auto it = deviceProperties.cbegin(); it != deviceProperties.cend(); ++it) {
        properties.insert(it.key(), it.value());
    }

    sink.name = properties.value(QStringLiteral("Name")).toString();
    sink.description = properties.value(QStringLiteral("Description")).toString();
    sink.volume = volumePercent(properties.value(QStringLiteral("Volume")));
    sink.muted = properties.value(QStringLiteral("Mute")).toBool();
    return sink;
}

void SystemVolumePlugin::refreshSinks()
{
    m_sinks.clear();

    const QVariant sinksValue =
        callProperties(pulseAudioCorePath, QStringLiteral("Get"),
                       {pulseAudioCoreInterface, QStringLiteral("Sinks")});
    QList<QDBusObjectPath> sinkPaths;
    if (sinksValue.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument argument = sinksValue.value<QDBusArgument>();
        sinkPaths = qdbus_cast<QList<QDBusObjectPath>>(argument);
    }

    for (const QDBusObjectPath &path : sinkPaths) {
        const Sink sink = readSink(path.path());
        if (!sink.name.isEmpty()) {
            m_sinks.append(sink);
        }
    }

    // Forward live volume/mute changes of the sinks (the connection is
    // idempotent, so this is safe to call on every refresh).
    for (const Sink &sink : m_sinks) {
        QDBusConnection::sessionBus().connect(
            pulseAudioService, sink.path, propertiesInterface,
            QStringLiteral("PropertiesChanged"), this,
            SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    }
}

void SystemVolumePlugin::sendSinkList()
{
    refreshSinks();

    QVariantList sinkList;
    for (const Sink &sink : m_sinks) {
        QVariantMap object;
        object.insert(QStringLiteral("name"), sink.name);
        object.insert(QStringLiteral("description"), sink.description);
        object.insert(QStringLiteral("volume"), sink.volume);
        object.insert(QStringLiteral("muted"), sink.muted);
        object.insert(QStringLiteral("maxVolume"), protocolMaxVolume);
        // PulseAudio does not expose the default sink on its D-Bus interface.
        object.insert(QStringLiteral("enabled"), false);
        sinkList.append(object);
    }

    NetworkPacket np(packetTypeSystemVolume);
    np.set(QStringLiteral("sinkList"), sinkList);
    sendPacket(np);
}

QString SystemVolumePlugin::sinkPathForName(const QString &name) const
{
    for (const Sink &sink : m_sinks) {
        if (sink.name == name) {
            return sink.path;
        }
    }
    return QString();
}

bool SystemVolumePlugin::applyVolume(const QString &name, int volume)
{
    const QString path = sinkPathForName(name);
    if (path.isEmpty()) {
        return false;
    }

    const QVariant value = QVariant::fromValue(volumeChannels(volume));
    callProperties(path, QStringLiteral("Set"),
                   {pulseAudioDeviceInterface, QStringLiteral("Volume"), value});
    return true;
}

bool SystemVolumePlugin::applyMute(const QString &name, bool muted)
{
    const QString path = sinkPathForName(name);
    if (path.isEmpty()) {
        return false;
    }

    callProperties(path, QStringLiteral("Set"),
                   {pulseAudioDeviceInterface, QStringLiteral("Mute"), muted});
    return true;
}

void SystemVolumePlugin::onPropertiesChanged(const QString &interface,
                                             const QVariantMap &changed,
                                             const QStringList &invalidated)
{
    Q_UNUSED(interface)
    Q_UNUSED(invalidated)

    const QString name = changed.value(QStringLiteral("Name")).toString();
    if (name.isEmpty()) {
        return;
    }

    NetworkPacket np(packetTypeSystemVolume);
    np.set(QStringLiteral("name"), name);
    if (changed.contains(QStringLiteral("Volume"))) {
        np.set(QStringLiteral("volume"),
               volumePercent(changed.value(QStringLiteral("Volume"))));
    }
    if (changed.contains(QStringLiteral("Mute"))) {
        np.set(QStringLiteral("muted"),
               changed.value(QStringLiteral("Mute")).toBool());
    }
    if (np.has(QStringLiteral("volume")) || np.has(QStringLiteral("muted"))) {
        sendPacket(np);
    }
}

#include "systemvolumeplugin.moc"
