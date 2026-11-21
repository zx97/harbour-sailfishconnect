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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QProcess>
#include <QStringList>
#include <QTimer>

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(SystemVolumePlugin, "sailfishconnect_systemvolume.json")

Q_LOGGING_CATEGORY(loggerSystemVolume, "kdeconnect.plugin.systemvolume")

namespace {

const QString pactlBinary = QStringLiteral("pactl");
const QString packetTypeSystemVolume = QStringLiteral("kdeconnect.systemvolume");
const QString packetTypeSystemVolumeRequest =
    QStringLiteral("kdeconnect.systemvolume.request");

// PulseAudio's internal volume unit; the protocol uses percentages.
constexpr int pulseVolumeNorm = 65536;
constexpr int protocolMaxVolume = 100;

// pactl translates its output, including the event names of "subscribe". The
// C locale keeps the parsing stable whatever the user language is.
QProcessEnvironment pactlEnvironment()
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    environment.insert(QStringLiteral("LANG"), QStringLiteral("C"));
    return environment;
}

QString runPactl(const QStringList &arguments)
{
    QProcess process;
    process.setProcessEnvironment(pactlEnvironment());
    process.start(pactlBinary, arguments);
    if (!process.waitForFinished(3000)) {
        qCWarning(loggerSystemVolume) << "pactl timed out:" << arguments;
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        qCWarning(loggerSystemVolume) << "pactl failed:" << arguments
                                      << process.readAllStandardError();
        return {};
    }
    return QString::fromUtf8(process.readAllStandardOutput());
}

} // namespace

SystemVolumePlugin::SystemVolumePlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , m_refreshTimer(new QTimer(this))
{
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(200);
    connect(m_refreshTimer, &QTimer::timeout, this,
            &SystemVolumePlugin::refreshFromPulseAudio);
}

SystemVolumePlugin::~SystemVolumePlugin()
{
    if (m_subscription) {
        m_subscription->kill();
        m_subscription->waitForFinished(1000);
    }
}

void SystemVolumePlugin::connected()
{
    startEventSubscription();
    sendSinkList();
}

void SystemVolumePlugin::startEventSubscription()
{
    if (m_subscription) {
        return;
    }

    m_subscription = new QProcess(this);
    connect(m_subscription, &QProcess::readyRead, this, [this]() {
        const QString events = QString::fromUtf8(m_subscription->readAll());

        // pactl subscribe reports every object type, including the client
        // created by our own pactl calls: following those would make the
        // refresh feed itself. Only react to the audio objects.
        static const QStringList relevantObjects{
            QStringLiteral(" on sink #"), QStringLiteral(" on sink-input #"),
            QStringLiteral(" on card #"), QStringLiteral(" on server")};
        for (const QString &object : relevantObjects) {
            if (events.contains(object)) {
                // Coalesce bursts of events into one refresh.
                m_refreshTimer->start();
                break;
            }
        }
    });
    connect(m_subscription,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this]() {
                m_subscription->deleteLater();
                m_subscription = nullptr;
            });
    m_subscription->setProcessEnvironment(pactlEnvironment());
    m_subscription->start(pactlBinary, {QStringLiteral("subscribe")});
}

QList<SystemVolumePlugin::Sink> SystemVolumePlugin::readSinks() const
{
    QList<Sink> sinks;

    const QString output =
        runPactl({QStringLiteral("-f"), QStringLiteral("json"), QStringLiteral("list"),
                  QStringLiteral("sinks")});
    const QJsonDocument document = QJsonDocument::fromJson(output.toUtf8());
    if (!document.isArray()) {
        return sinks;
    }

    const QString defaultSinkName = defaultSink();
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();

        Sink sink;
        sink.name = object.value(QStringLiteral("name")).toString();
        sink.description = object.value(QStringLiteral("description")).toString();
        sink.muted = object.value(QStringLiteral("mute")).toBool();
        sink.isDefault = sink.name == defaultSinkName;

        // Take the first channel: all channels are set together.
        const QJsonObject volume = object.value(QStringLiteral("volume")).toObject();
        if (!volume.isEmpty()) {
            const QJsonObject channel = volume.begin().value().toObject();
            const int value = channel.value(QStringLiteral("value")).toInt();
            sink.volume = qBound(0, qRound(double(value) * protocolMaxVolume
                                           / pulseVolumeNorm),
                                 protocolMaxVolume);
        }

        if (!sink.name.isEmpty()) {
            sinks.append(sink);
        }
    }

    return sinks;
}

QString SystemVolumePlugin::defaultSink() const
{
    return runPactl({QStringLiteral("get-default-sink")}).trimmed();
}

void SystemVolumePlugin::sendSinkList()
{
    m_sinks = readSinks();

    QVariantList sinkList;
    for (const Sink &sink : m_sinks) {
        QVariantMap object;
        object.insert(QStringLiteral("name"), sink.name);
        object.insert(QStringLiteral("description"), sink.description);
        object.insert(QStringLiteral("volume"), sink.volume);
        object.insert(QStringLiteral("muted"), sink.muted);
        object.insert(QStringLiteral("maxVolume"), protocolMaxVolume);
        object.insert(QStringLiteral("enabled"), sink.isDefault);
        sinkList.append(object);
    }

    NetworkPacket np(packetTypeSystemVolume);
    np.set(QStringLiteral("sinkList"), sinkList);
    sendPacket(np);
}

void SystemVolumePlugin::refreshFromPulseAudio()
{
    sendSinkList();
}

void SystemVolumePlugin::receivePacket(const NetworkPacket &np)
{
    if (np.type() != packetTypeSystemVolumeRequest) {
        return;
    }

    if (np.get<bool>(QStringLiteral("requestSinks"), false)) {
        sendSinkList();
        return;
    }

    const QString name = np.get<QString>(QStringLiteral("name"));
    if (name.isEmpty()) {
        return;
    }

    if (np.has(QStringLiteral("volume"))) {
        const int volume =
            qBound(0, np.get<int>(QStringLiteral("volume")), protocolMaxVolume);
        runPactl({QStringLiteral("set-sink-volume"), name,
                  QString::number(volume) + QChar::fromLatin1('%')});
    }
    if (np.has(QStringLiteral("muted"))) {
        runPactl({QStringLiteral("set-sink-mute"), name,
                  np.get<bool>(QStringLiteral("muted")) ? QStringLiteral("1")
                                                        : QStringLiteral("0")});
    }
    if (np.has(QStringLiteral("enabled"))
        && np.get<bool>(QStringLiteral("enabled"))) {
        runPactl({QStringLiteral("set-default-sink"), name});
    }

    // Report the real state back, pactl clamps the values.
    m_refreshTimer->start();
}

#include "systemvolumeplugin.moc"
