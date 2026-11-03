/*
#    FVD++, an advanced coaster design tool for NoLimits
#    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "backupmanager.h"
#include "projectwidget.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QRegExp>
#include <QStandardPaths>
#include <fstream>
#include <algorithm>

static const char* kSlotPattern = "^(.+)_backup_(\\d+)\\.fvd$";

QString backupManager::backupDirFor(const QString& projectFile)
{
    if(projectFile.isEmpty()) {
        QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if(docs.isEmpty()) docs = QDir::homePath();
        return QDir(docs).filePath("LiquidFVD/Autosave");
    }
    return QFileInfo(projectFile).absoluteDir().filePath(backupFolderName());
}

QString backupManager::baseNameFor(const QString& projectFile)
{
    if(projectFile.isEmpty()) return QString("unsaved");
    QString name = QFileInfo(projectFile).fileName();
    if(name.endsWith(".fvd", Qt::CaseInsensitive)) name.chop(4);
    return name;
}

static QString slotPath(const QString& dir, const QString& base, int slot)
{
    return QDir(dir).filePath(QString("%1_backup_%2.fvd").arg(base).arg(slot));
}

QStringList backupManager::existingBackups(const QString& projectFile, int slotCount)
{
    QString dir = backupDirFor(projectFile);
    QString base = baseNameFor(projectFile);
    QList<QFileInfo> found;
    for(int i = 1; i <= slotCount; ++i) {
        QFileInfo fi(slotPath(dir, base, i));
        if(fi.exists()) found.append(fi);
    }
    // newest first
    for(int i = 0; i < found.size(); ++i)
        for(int j = i + 1; j < found.size(); ++j)
            if(found[j].lastModified() > found[i].lastModified()) std::swap(found[i], found[j]);

    QStringList result;
    for(int i = 0; i < found.size(); ++i) result.append(found[i].absoluteFilePath());
    return result;
}

bool backupManager::isBackupFile(const QString& file)
{
    QFileInfo fi(file);
    return fi.absoluteDir().dirName() == backupFolderName()
            && QRegExp(kSlotPattern).exactMatch(fi.fileName());
}

QString backupManager::originalForBackup(const QString& backupFile)
{
    QFileInfo fi(backupFile);
    QRegExp rx(kSlotPattern);
    if(!rx.exactMatch(fi.fileName())) return QString();
    QDir projectDir = fi.absoluteDir();
    projectDir.cdUp();
    return projectDir.filePath(rx.cap(1) + ".fvd");
}

QString backupManager::makeProjectFolder(const QString& chosenFile, QString* error)
{
    QFileInfo fi(chosenFile);
    QString base = baseNameFor(chosenFile);
    QDir parent = fi.absoluteDir();

    // already inside a folder named like the project -> don't nest again
    if(parent.dirName() == base) {
        return parent.filePath(base + ".fvd");
    }
    if(!parent.exists(base) && !parent.mkdir(base)) {
        if(error) *error = QString("Could not create project folder %1").arg(parent.filePath(base));
        return QString();
    }
    return QDir(parent.filePath(base)).filePath(base + ".fvd");
}

static QByteArray readAll(const QString& path)
{
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)) return QByteArray();
    return f.readAll();
}

backupManager::Result backupManager::writeBackup(projectWidget* project, const QString& projectFile, int slotCount, QString* writtenPath)
{
    if(slotCount < 1) slotCount = 1;
    QString dir = backupDirFor(projectFile);
    QString base = baseNameFor(projectFile);
    if(!QDir().mkpath(dir)) return Failed;

    // 1) write the snapshot to a temp file first, so a crash mid-write never
    //    destroys an existing backup
    QString tmp = QDir(dir).filePath(base + "_backup.tmp");
    {
        std::fstream fout(tmp.toLocal8Bit().data(), std::ios::out | std::ios::binary);
        if(!fout) return Failed;
        project->saveProject(fout);
        fout.close();
        if(fout.fail()) { QFile::remove(tmp); return Failed; }
    }

    // 2) skip if nothing changed since the newest backup
    QStringList existing = existingBackups(projectFile, slotCount);
    if(!existing.isEmpty() && readAll(existing.first()) == readAll(tmp)) {
        QFile::remove(tmp);
        if(writtenPath) *writtenPath = existing.first();
        return Unchanged;
    }

    // 3) pick a slot: first free one, otherwise the oldest
    QString target;
    QDateTime oldest;
    for(int i = 1; i <= slotCount; ++i) {
        QFileInfo fi(slotPath(dir, base, i));
        if(!fi.exists()) { target = fi.absoluteFilePath(); break; }
        if(target.isEmpty() || fi.lastModified() < oldest) {
            target = fi.absoluteFilePath();
            oldest = fi.lastModified();
        }
    }

    QFile::remove(target);
    if(!QFile::rename(tmp, target)) {
        QFile::remove(tmp);
        return Failed;
    }
    // make sure the ring order follows write order even on coarse filesystem clocks
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    QFile f(target);
    if(f.open(QIODevice::ReadWrite)) {
        f.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
        f.close();
    }
#endif
    if(writtenPath) *writtenPath = target;
    return Written;
}
