#ifndef BACKUPMANAGER_H
#define BACKUPMANAGER_H

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

#include <QString>
#include <QStringList>

class projectWidget;

/*
 * Rotating autosave backups.
 *
 * Layout for a project saved as  <dir>/MyCoaster.fvd :
 *
 *   <dir>/MyCoaster.fvd
 *   <dir>/Backups/MyCoaster_backup_1.fvd
 *   <dir>/Backups/MyCoaster_backup_2.fvd
 *   ...                                     (up to slotCount files)
 *
 * Every write goes to the next slot: first an unused one, otherwise the one
 * with the oldest modification time, so the files are overwritten in a ring.
 * A snapshot identical to the newest existing backup is not written, so idle
 * periods don't push older, useful states out of the ring.
 *
 * Work that has never been saved goes to <Documents>/LiquidFVD/Autosave/unsaved_backup_N.fvd
 */
class backupManager
{
public:
    enum Result { Written, Unchanged, Failed };

    static QString backupFolderName() { return QString("Backups"); }

    // folder that holds the backups of the given project file (or of unsaved work if empty)
    static QString backupDirFor(const QString& projectFile);

    // file name stem used for the slots ("MyCoaster" -> MyCoaster_backup_N.fvd)
    static QString baseNameFor(const QString& projectFile);

    // existing backup files of a project, newest first
    static QStringList existingBackups(const QString& projectFile, int slotCount);

    // true if the file lives in a Backups folder and follows the slot naming scheme
    static bool isBackupFile(const QString& file);

    // for a backup file, the project file it belongs to (<dir>/MyCoaster.fvd)
    static QString originalForBackup(const QString& backupFile);

    // turns "<dir>/MyCoaster.fvd" into "<dir>/MyCoaster/MyCoaster.fvd" and creates that folder,
    // unless the file already sits in a folder with the project's name
    static QString makeProjectFolder(const QString& chosenFile, QString* error);

    // writes the project into the next slot; writtenPath receives the target on success
    static Result writeBackup(projectWidget* project, const QString& projectFile, int slotCount, QString* writtenPath);
};

#endif // BACKUPMANAGER_H
