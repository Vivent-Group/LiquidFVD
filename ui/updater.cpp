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

#include "updater.h"

#include <QProcess>
#include <QProgressDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QSettings>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegExp>
#include <QStringList>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QtDebug>

#ifndef LIQUIDFVD_VERSION
#define LIQUIDFVD_VERSION "0.0.0-dev"
#endif

static const char* kReleaseApi = "https://api.github.com/repos/Vivent-Group/LiquidFVD/releases/latest";

updater::updater(QWidget* parentWindow)
    : QObject(parentWindow), mWindow(parentWindow), mCheck(NULL), mDownload(NULL),
      mProgress(NULL), mProgressTimer(NULL), mInteractive(false)
{
}

QString updater::currentVersion()
{
    return QString(LIQUIDFVD_VERSION);
}

int updater::compareVersions(const QString& a, const QString& b)
{
    // numbers only: "v0.80.12" -> [0, 80, 12]; "-dev" builds count as the base version
    QRegExp num("(\\d+)");
    QList<int> pa, pb;
    QString sa = a.section('-', 0, 0), sb = b.section('-', 0, 0);
    for(int pos = 0; (pos = num.indexIn(sa, pos)) != -1; pos += num.matchedLength()) pa << num.cap(1).toInt();
    for(int pos = 0; (pos = num.indexIn(sb, pos)) != -1; pos += num.matchedLength()) pb << num.cap(1).toInt();
    for(int i = 0; i < qMax(pa.size(), pb.size()); ++i) {
        int x = i < pa.size() ? pa[i] : 0;
        int y = i < pb.size() ? pb[i] : 0;
        if(x != y) return x < y ? -1 : 1;
    }
    return 0;
}

bool updater::parseRelease(const QByteArray& json, releaseInfo* out, QString* error)
{
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if(pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if(error) *error = "The update server sent an unexpected answer.";
        return false;
    }
    QJsonObject o = doc.object();
    QString tag = o["tag_name"].toString();
    if(tag.isEmpty()) {
        if(error) *error = o["message"].toString("No release found.");
        return false;
    }
    out->version = tag.startsWith('v') ? tag.mid(1) : tag;
    out->title = o["name"].toString(tag);
    out->notes = o["body"].toString();
    out->pageUrl = o["html_url"].toString();

#if defined(Q_OS_MAC)
    const QString suffix = "-macOS.zip";
#elif defined(Q_OS_WIN)
    const QString suffix = "-windows.zip";
#else
    const QString suffix = "-linux.zip";
#endif
    QJsonArray assets = o["assets"].toArray();
    for(int i = 0; i < assets.size(); ++i) {
        QJsonObject a = assets[i].toObject();
        if(a["name"].toString().endsWith(suffix, Qt::CaseInsensitive)) {
            out->assetName = a["name"].toString();
            out->assetUrl = a["browser_download_url"].toString();
            out->assetSize = (qint64)a["size"].toDouble();
            break;
        }
    }
    return true;
}

QString updater::apiUrl() const
{
    QByteArray env = qgetenv("LIQUIDFVD_UPDATE_API");
    return env.isEmpty() ? QString(kReleaseApi) : QString::fromLocal8Bit(env);
}

QString updater::curlProgram() const
{
#ifdef Q_OS_WIN
    return "curl.exe";
#else
    return "curl";
#endif
}

QString updater::macBundlePath() const
{
    // .../LiquidFVD.app/Contents/MacOS/FVD -> .../LiquidFVD.app
    QDir d(QCoreApplication::applicationDirPath());
    d.cdUp();
    d.cdUp();
    return d.canonicalPath();
}

// ---------------------------------------------------------------------------
//  Checking
// ---------------------------------------------------------------------------

void updater::checkNow()
{
    startCheck(true);
}

void updater::checkOnStartup()
{
    QSettings s("FVD++", "FVD");
    if(!s.value("updates/checkOnStart", true).toBool()) return;
    // once a day is plenty - unless a test server is configured
    QDateTime last = s.value("updates/lastCheck").toDateTime();
    if(qgetenv("LIQUIDFVD_UPDATE_API").isEmpty() && last.isValid() && last.secsTo(QDateTime::currentDateTime()) < 20*3600) return;
    startCheck(false);
}

void updater::startCheck(bool interactive)
{
    if(mCheck || mDownload) return;     // already busy
    mInteractive = interactive;
    mCheck = new QProcess(this);
    connect(mCheck, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(onCheckFinished()));
    QStringList args;
    args << "-sS" << "-L" << "--fail" << "--max-time" << "20"
         << "-H" << "Accept: application/vnd.github+json"
         << "-H" << QString("User-Agent: LiquidFVD-Updater/%1").arg(currentVersion())
         << apiUrl();
    qDebug().noquote() << "Update check:" << apiUrl();
    mCheck->start(curlProgram(), args);
    if(!mCheck->waitForStarted(3000)) {
        mCheck->deleteLater();
        mCheck = NULL;
        fail(tr("Could not look for updates: curl was not found on this computer."));
    }
}

void updater::onCheckFinished()
{
    QProcess* p = mCheck;
    mCheck = NULL;
    p->deleteLater();
    QByteArray out = p->readAllStandardOutput();
    QString err = QString::fromLocal8Bit(p->readAllStandardError()).trimmed();

    if(p->exitStatus() != QProcess::NormalExit || p->exitCode() != 0) {
        qDebug().noquote() << "Update check failed:" << p->exitCode() << err;
        if(err.contains("404")) fail(tr("There is no LiquidFVD release on GitHub yet."));
        else fail(tr("Could not reach GitHub to look for updates.\n\n%1").arg(err));
        emit checkFinished(false);
        return;
    }

    QSettings s("FVD++", "FVD");
    s.setValue("updates/lastCheck", QDateTime::currentDateTime());

    releaseInfo info;
    QString perr;
    if(!parseRelease(out, &info, &perr)) {
        fail(perr);
        emit checkFinished(false);
        return;
    }
    qDebug().noquote() << "Update check: latest" << info.version << "- this is" << currentVersion();

    if(compareVersions(info.version, currentVersion()) <= 0) {
        if(mInteractive) {
            QMessageBox::information(mWindow, tr("Check for Updates"),
                tr("LiquidFVD %1 is the newest version.").arg(currentVersion()));
        }
        emit checkFinished(false);
        return;
    }
    if(!mInteractive && s.value("updates/skipVersion").toString() == info.version) {
        emit checkFinished(false);
        return;
    }
    emit checkFinished(true);
    offer(info);
}

void updater::fail(const QString& message)
{
    qDebug().noquote() << "Updater:" << message;
    if(mInteractive) QMessageBox::warning(mWindow, tr("Check for Updates"), message);
}

// ---------------------------------------------------------------------------
//  Offering and installing
// ---------------------------------------------------------------------------

void updater::offer(const releaseInfo& info)
{
    if(qgetenv("LIQUIDFVD_UPDATE_ASSUME_YES") == "1") {
        install(info);
        return;
    }

#ifdef Q_OS_MAC
    const bool selfInstall = !info.assetUrl.isEmpty();
#else
    const bool selfInstall = false;
#endif

    QMessageBox box(mWindow);
    box.setWindowTitle(tr("Update available"));
    box.setIcon(QMessageBox::Information);
    box.setText(tr("<b>LiquidFVD %1 is available</b> - you have %2.").arg(info.version).arg(currentVersion()));
    box.setInformativeText(selfInstall
        ? tr("Install it now? LiquidFVD restarts by itself. Your project is saved to its autosave backup first - "
             "save it normally before if you want the project file itself up to date.")
        : tr("Open the download page?"));
    if(!info.notes.trimmed().isEmpty()) box.setDetailedText(info.notes);
    QPushButton* installBtn = box.addButton(selfInstall ? tr("Install and Restart") : tr("Open Download Page"), QMessageBox::AcceptRole);
    box.addButton(tr("Later"), QMessageBox::RejectRole);
    QPushButton* skipBtn = box.addButton(tr("Skip This Version"), QMessageBox::DestructiveRole);
    box.setDefaultButton(installBtn);
    box.exec();

    if(box.clickedButton() == installBtn) {
        install(info);
    } else if(box.clickedButton() == skipBtn) {
        QSettings s("FVD++", "FVD");
        s.setValue("updates/skipVersion", info.version);
    }
}

void updater::install(const releaseInfo& info)
{
#ifdef Q_OS_MAC
    if(info.assetUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(info.pageUrl));
        return;
    }
    QString bundle = macBundlePath();
    if(!bundle.endsWith(".app")) {
        // started outside an app bundle (developer build) - nothing to replace
        QDesktopServices::openUrl(QUrl(info.pageUrl));
        return;
    }
    if(!QFileInfo(QFileInfo(bundle).absolutePath()).isWritable()) {
        QMessageBox::warning(mWindow, tr("Update"),
            tr("LiquidFVD can't replace itself in\n%1\n(no write permission). The download page opens instead.")
            .arg(QFileInfo(bundle).absolutePath()));
        QDesktopServices::openUrl(QUrl(info.pageUrl));
        return;
    }

    mPending = info;
    QDir tmp(QDir::temp().filePath("LiquidFVD-update"));
    tmp.removeRecursively();
    QDir().mkpath(tmp.absolutePath());
    mDownloadFile = tmp.filePath(info.assetName.isEmpty() ? QString("update.zip") : info.assetName);

    mProgress = new QProgressDialog(tr("Downloading LiquidFVD %1...").arg(info.version), tr("Cancel"), 0,
                                    info.assetSize > 0 ? (int)(info.assetSize/1024) : 0, mWindow);
    mProgress->setWindowTitle(tr("Update"));
    mProgress->setWindowModality(Qt::WindowModal);
    mProgress->setMinimumDuration(0);
    mProgress->setValue(0);

    mDownload = new QProcess(this);
    connect(mDownload, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(onDownloadFinished()));
    connect(mProgress, SIGNAL(canceled()), mDownload, SLOT(kill()));
    mProgressTimer = new QTimer(this);
    connect(mProgressTimer, SIGNAL(timeout()), this, SLOT(onDownloadProgress()));
    mProgressTimer->start(200);

    QStringList args;
    args << "-sS" << "-L" << "--fail" << "--max-time" << "600"
         << "-H" << QString("User-Agent: LiquidFVD-Updater/%1").arg(currentVersion())
         << "-o" << mDownloadFile << info.assetUrl;
    qDebug().noquote() << "Update download:" << info.assetUrl;
    mDownload->start(curlProgram(), args);
#else
    QDesktopServices::openUrl(QUrl(info.pageUrl));
#endif
}

void updater::onDownloadProgress()
{
    if(mProgress && mProgress->maximum() > 0) {
        mProgress->setValue((int)(QFileInfo(mDownloadFile).size()/1024));
    }
}

void updater::onDownloadFinished()
{
    QProcess* p = mDownload;
    mDownload = NULL;
    p->deleteLater();
    if(mProgressTimer) { mProgressTimer->stop(); mProgressTimer->deleteLater(); mProgressTimer = NULL; }
    bool canceled = mProgress && mProgress->wasCanceled();
    if(mProgress) { mProgress->hide(); mProgress->deleteLater(); mProgress = NULL; }
    if(canceled) return;

    QString err = QString::fromLocal8Bit(p->readAllStandardError()).trimmed();
    if(p->exitStatus() != QProcess::NormalExit || p->exitCode() != 0 || QFileInfo(mDownloadFile).size() == 0) {
        mInteractive = true;
        fail(tr("The download failed.\n\n%1").arg(err));
        return;
    }
    QString error;
    if(!installMac(mDownloadFile, &error)) {
        mInteractive = true;
        fail(error);
    }
}

bool updater::installMac(const QString& zipFile, QString* error)
{
#ifdef Q_OS_MAC
    QString extractDir = QFileInfo(zipFile).absolutePath() + "/extracted";
    QDir(extractDir).removeRecursively();
    QDir().mkpath(extractDir);

    // ditto keeps the bundle's symlinks, permissions and signature intact
    QProcess unzip;
    unzip.start("/usr/bin/ditto", QStringList() << "-x" << "-k" << zipFile << extractDir);
    if(!unzip.waitForFinished(120000) || unzip.exitCode() != 0) {
        *error = tr("Could not unpack the update.\n\n%1").arg(QString::fromLocal8Bit(unzip.readAllStandardError()));
        return false;
    }
    QString newApp;
    QDirIterator it(extractDir, QStringList() << "*.app", QDir::Dirs);
    if(it.hasNext()) newApp = it.next();
    if(newApp.isEmpty() || !QFileInfo(newApp + "/Contents/MacOS").isDir()) {
        *error = tr("The update doesn't contain a LiquidFVD app.");
        return false;
    }

    // a small helper swaps the bundles after this process has quit, then starts the new version
    QString script = QFileInfo(zipFile).absolutePath() + "/install.sh";
    QFile f(script);
    if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = tr("Could not prepare the update.");
        return false;
    }
    f.write(
        "#!/bin/bash\n"
        "# LiquidFVD updater: $1 = pid to wait for, $2 = new bundle, $3 = installed bundle\n"
        "exec >> \"$(dirname \"$0\")/install.log\" 2>&1\n"
        "echo \"$(date) waiting for $1\"\n"
        "for i in $(seq 1 300); do kill -0 \"$1\" 2>/dev/null || break; sleep 0.2; done\n"
        "rm -rf \"$3.old\"\n"
        "if mv \"$3\" \"$3.old\" && mv \"$2\" \"$3\"; then\n"
        "  rm -rf \"$3.old\"\n"
        "  echo \"installed\"\n"
        "else\n"
        "  echo \"replacing failed - restoring\"\n"
        "  [ -d \"$3.old\" ] && [ ! -d \"$3\" ] && mv \"$3.old\" \"$3\"\n"
        "fi\n"
        "xattr -dr com.apple.quarantine \"$3\" 2>/dev/null\n"
        "open \"$3\"\n");
    f.close();
    f.setPermissions(f.permissions() | QFileDevice::ExeOwner);

    qint64 pid = QCoreApplication::applicationPid();
    if(!QProcess::startDetached("/bin/bash", QStringList() << script << QString::number(pid) << newApp << macBundlePath())) {
        *error = tr("Could not start the installer.");
        return false;
    }
    qDebug().noquote() << "Update: installing" << newApp << "over" << macBundlePath();
    emit aboutToInstall();      // the main window saves a backup and quits
    return true;
#else
    Q_UNUSED(zipFile);
    *error = tr("Automatic installation is only available on macOS.");
    return false;
#endif
}
