/*
 * Copyright 2019 Richard Liebscher <richard.liebscher@gmail.com>.
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

#include "contactsmanager.h"

#include <QDir>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlDatabase>

#include "vcardbuilder.h"

namespace SailfishConnect {

static Q_LOGGING_CATEGORY(logger, "kdeconnect.contactsmanager");

// The contacts database lives in the privileged data directory. The daemon
// is a member of the "privileged" group so it can read it directly.
static QString QTCONTACTS_SQLITE_STORE =
    QDir::homePath() % QChar::fromLatin1('/')
        % QStringLiteral(".local/share/system/privileged/Contacts/qtcontacts-sqlite/contacts.db");

namespace {

class SailfishOsContactsManager : public ContactsManager {
public:
    SailfishOsContactsManager();

    QMap<QString, QDateTime> getLastModifiedTimes() override;
    QMap<QString, QString> exportVCards(const QStringList& requestedIds, const QString& deviceId) override;
    QString lookUpName(const QString& phoneNumber) override;

private:
    QSqlDatabase m_db;
};
    
SailfishOsContactsManager::SailfishOsContactsManager()
{
    m_db = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"),
        QStringLiteral("QTCONTACTS_SQLITE_STORE"));
    if (!m_db.isValid()) {
        qCCritical(logger) << "QSQLITE database driver is not available.";
        return;
    }

    m_db.setDatabaseName(QTCONTACTS_SQLITE_STORE);
    if (!m_db.open()) {
        qCCritical(logger) << "Cannot open contacts database"
                           << QTCONTACTS_SQLITE_STORE
                           << ":" << m_db.lastError().text();
        return;
    }
}

QMap<QString, QDateTime> SailfishOsContactsManager::getLastModifiedTimes()
{
    QMap<QString, QDateTime> result;

    QSqlQuery timeStampQuery(m_db);
    if (!timeStampQuery.exec(
            QStringLiteral("SELECT contactId, modified FROM Contacts"))) {
        qCCritical(logger) << "Getting modified timestamps failed:"
                           << timeStampQuery.lastError().text();
        return result;
    }

    while (timeStampQuery.next()) {
        QString id = QString::number(timeStampQuery.value(0).toInt());
        QDateTime modified = timeStampQuery.value(1).toDateTime();
        result.insert(id, modified);
    }

    return result;
}

QMap<QString, QString> SailfishOsContactsManager::exportVCards(const QStringList& ids, const QString& deviceId)
{
    QMap<QString, QString> result;
    QSet<QString> requestedIds = ids.toSet();

    QSqlQuery phoneNumbersQuery(m_db);
    if (!phoneNumbersQuery.exec(QStringLiteral(
            "SELECT DISTINCT contactId, phoneNumber FROM PhoneNumbers"))) {
        qCCritical(logger) << "Getting phone numbers failed:"
                           << phoneNumbersQuery.lastError().text();
        return {};
    }

    QMap<QString, QStringList> phoneNumbers;
    while (phoneNumbersQuery.next()) {
        QString id = QString::number(phoneNumbersQuery.value(0).toInt());
        QString phoneNumber = phoneNumbersQuery.value(1).toString();
        if (phoneNumbers.contains(id)) {
            phoneNumbers[id].append(phoneNumber);
        } else {
            phoneNumbers.insert(id, { phoneNumber });
        }
    }

    QSqlQuery dataQuery(m_db);
    if (!dataQuery.exec(QStringLiteral(
            "SELECT c.contactId, c.modified, d.displayLabel,"
            " n.firstName, n.lastName"
            " FROM Contacts c"
            " LEFT JOIN DisplayLabels d ON d.contactId = c.contactId"
            " LEFT JOIN Names n ON n.contactId = c.contactId"))) {
        qCCritical(logger) << "Getting contact details failed:"
                           << dataQuery.lastError().text();
        return {};
    }

    while (dataQuery.next()) {
        QString id = QString::number(dataQuery.value(0).toInt());
        if (!requestedIds.contains(id))
            continue;

        QDateTime modified = dataQuery.value(1).toDateTime();
        QString displayLabel = dataQuery.value(2).toString();
        QString firstName = dataQuery.value(3).toString();
        QString lastName = dataQuery.value(4).toString();

        VCardBuilder vcf;
        vcf.addRawProperty(QStringLiteral("FN"), displayLabel);
        for (auto& tel : phoneNumbers.value(id)) {
            vcf.addRawProperty(QStringLiteral("TEL;TYPE=home"), tel);
        }
        vcf.addRawProperty(
            QStringLiteral("N"),
            QStringLiteral("%1;%2;;;").arg(lastName, firstName));
        vcf.addRawProperty(
            QStringLiteral("X-KDECONNECT-ID-DEV-") + deviceId,
            id);
        vcf.addRawProperty(
            QStringLiteral("X-KDECONNECT-TIMESTAMP"),
            QString::number(modified.isValid() ? modified.toMSecsSinceEpoch() : 0));

        result.insert(id, vcf.result());
    }

    return result;
}

QString SailfishOsContactsManager::lookUpName(const QString& phoneNumber)
{
    QSqlQuery dataQuery(m_db);
    dataQuery.prepare(QStringLiteral(
        "SELECT d.displayLabel FROM Contacts c"
        " JOIN DisplayLabels d ON d.contactId = c.contactId"
        " JOIN PhoneNumbers p ON p.contactId = c.contactId"
        " WHERE p.phoneNumber = ?"));
    dataQuery.bindValue(0, phoneNumber);

    if (!dataQuery.exec()) {
        qCCritical(logger) << "Getting contact name failed:"
                           << dataQuery.lastError().text();
        return QString();
    }

    if (dataQuery.next()) {
        return dataQuery.value(0).toString();
    } else {
        qCInfo(logger) << "Getting contact name failed: no entry found";
        return phoneNumber;
    }
}

} // namespace 

ContactsManager& ContactsManager::instance() {
    static SailfishOsContactsManager instance;
    return instance;
}

} // namespace SailfishConnect
