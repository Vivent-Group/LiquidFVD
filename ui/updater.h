#ifndef UPDATER_H
#define UPDATER_H

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

#include <QObject>
#include <QString>

class QProcess;
class QProgressDialog;
class QWidget;
class QTimer;

/*
 * Looks for a newer LiquidFVD release on GitHub and installs it.
 *
 *  - asks https://api.github.com/repos/Vivent-Group/LiquidFVD/releases/latest
 *  - downloads with the system's curl (built into macOS and Windows 10+), so no SSL
 *    libraries have to be shipped with the app
 *  - macOS: downloads the app zip, swaps the installed LiquidFVD.app and restarts it
 *  - other systems: opens the release page
 *
 * Developer / test switches (environment):
 *   LIQUIDFVD_UPDATE_API          other release JSON URL (e.g. a local test server)
 *   LIQUIDFVD_UPDATE_ASSUME_YES=1 install without asking (automated tests)
 */
class updater : public QObject
{
    Q_OBJECT
public:
    explicit updater(QWidget* parentWindow);

    static QString currentVersion();
    // <0: a older than b, 0: same, >0: a newer.  Accepts "v0.80.12", "0.80.12", "0.80.0-dev"
    static int compareVersions(const QString& a, const QString& b);

    struct releaseInfo {
        QString version;      // without leading v
        QString title;
        QString notes;
        QString pageUrl;
        QString assetUrl;     // download for this system, may be empty
        QString assetName;
        qint64  assetSize;
        releaseInfo() : assetSize(0) {}
    };
    // parses the GitHub API answer; false if it isn't a release
    static bool parseRelease(const QByteArray& json, releaseInfo* out, QString* error);

public slots:
    void checkNow();              // from the menu: always reports the result
    void checkOnStartup();        // quiet, at most once a day, honours the options

signals:
    void checkFinished(bool updateFound);
    // emitted right before the app quits to restart into the new version
    void aboutToInstall();

private slots:
    void onCheckFinished();
    void onDownloadFinished();
    void onDownloadProgress();

private:
    void startCheck(bool interactive);
    void startFallbackCheck();     // when the API is rate-limited: read the tag from the release page redirect
    void handleRelease(const releaseInfo& info);
    void offer(const releaseInfo& info);
    void install(const releaseInfo& info);
    bool installMac(const QString& zipFile, QString* error);
    void fail(const QString& message);
    QString apiUrl() const;
    QString curlProgram() const;
    QString macBundlePath() const;

    QWidget*  mWindow;
    QProcess* mCheck;
    QProcess* mDownload;
    QProgressDialog* mProgress;
    QTimer*   mProgressTimer;
    bool      mInteractive;
    bool      mFallback;
    releaseInfo mPending;
    QString   mDownloadFile;
};

#endif // UPDATER_H
