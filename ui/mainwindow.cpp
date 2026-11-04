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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QVector>

#include "mnode.h"
#include "exportui.h"
#include "optionsmenu.h"
#include "conversionpanel.h"
#include "graphwidget.h"
#include <sstream>
#include "trackwidget.h"
#include <QTimer>
#include "undohandler.h"
#include "undoaction.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include "objectexporter.h"
#include "backupmanager.h"
#include "theme.h"
#include "timelinestrip.h"
#include "updater.h"
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QAction>
#include <QMenu>
#include <cmath>

MainWindow* gloParent;
glViewWidget* glView;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    gloParent = this;
    project = NULL;

    // load options
    mOptions = new optionsMenu(this);
    mOptions->setWindowFlags(mOptions->windowFlags() | Qt::CustomizeWindowHint);
    mOptions->setWindowFlags(mOptions->windowFlags() & (~Qt::WindowMinimizeButtonHint));
    mOptions->setWindowFlags(mOptions->windowFlags() & (~Qt::WindowContextHelpButtonHint));

#ifdef Q_OS_MAC
    mOptions->setWindowModality(Qt::WindowModal);
    mOptions->setWindowFlags((windowFlags() & ~Qt::WindowType_Mask) | Qt::Sheet);
#endif

    ui->setupUi(this);

    glView = new glViewWidget(ui->splitter);
    glView->setObjectName(QStringLiteral("GraphicsView"));
    glView->setMinimumSize(QSize(0, 0));
    glView->setMouseTracking(true);
    glView->setFocusPolicy(Qt::StrongFocus);
    ui->splitter->addWidget(glView);

    // set up all sub widgets etc
    project = ui->projectTab;
    currentFileName.clear();
    this->setWindowTitle(QString("LiquidFVD - unsaved Work"));


    // set up GL frame
#ifndef Q_OS_MAC
    if(mOptions->glPolicy == 1) {
        glView->legacyMode = true;
    } else
#endif
    if(mOptions->glPolicy == 2) {
        glView->legacyMode = false;
    }
    else if(glView->format().majorVersion() < 3 || (glView->format().majorVersion() == 3 && glView->format().minorVersion() < 1)) {
        glView->legacyMode = true;
    } else {
        glView->legacyMode = false;
    }
    if(glView->legacyMode) {
        delete ui->menuView;
    }

    phantomChanges = false;

    // init conversion panel
    mConversion = new conversionPanel(this);
    mConversion->setWindowFlags(mConversion->windowFlags() | Qt::CustomizeWindowHint);
    mConversion->setWindowFlags(mConversion->windowFlags() & (~Qt::WindowMinimizeButtonHint));
    mConversion->setWindowFlags(mConversion->windowFlags() & (~Qt::WindowContextHelpButtonHint));

    //ui->customPlot->hide();
    mGraphWidget = NULL;

    delete ui->customPlot;
    ui->customPlot = NULL;


    exportScreen = new exportUi(this, ui->projectTab);
    exportScreen->setWindowFlags(exportScreen->windowFlags() | Qt::CustomizeWindowHint);
    exportScreen->setWindowFlags(exportScreen->windowFlags() & (~Qt::WindowMinimizeButtonHint));
    exportScreen->setWindowFlags(exportScreen->windowFlags() & (~Qt::WindowContextHelpButtonHint));

    mObjectExporter = new objectExporter(this);
    mObjectExporter->setWindowFlags(exportScreen->windowFlags());

    setUndoButtons();
    undoChanges = false;

    QTabBar *tabBar = ui->tabChooser->findChild<QTabBar*>();
    #ifndef Q_OS_MAC // on Win / Unix
        tabBar->tabButton(0, QTabBar::RightSide)->resize(0, 0); // hide close button on project tab
    #endif
    #ifdef Q_OS_MAC
        tabBar->tabButton(0, QTabBar::LeftSide)->resize(0, 0); // hide close button on project tab
    #endif

    selectedFunc = NULL;


    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), glView, SLOT(updateGL()));
    connect(timer, SIGNAL(timeout()), this, SLOT(showCurInfoPanel()));
	timer->start(10);

    // rotating backups, interval comes from the options (default 5 min)
    autosaveTimer = new QTimer(this);
    connect(autosaveTimer, SIGNAL(timeout()), this, SLOT(doAutoSave()));

    darkModeAction = new QAction(tr("Dark Mode"), this);
    darkModeAction->setCheckable(true);
    darkModeAction->setShortcut(QKeySequence(tr("Ctrl+Shift+D")));
    ui->menuEdit->addSeparator();
    ui->menuEdit->addAction(darkModeAction);
    connect(darkModeAction, SIGNAL(toggled(bool)), this, SLOT(toggleDarkMode(bool)));

    theme::init();
    applyAppearanceAndAutosave();

    // music / play-along / bookmarks
    QMenu* timelineMenu = new QMenu(tr("Timeline"), this);
    ui->menuBar->insertMenu(ui->menuHelp->menuAction(), timelineMenu);
    QAction* a;
    a = timelineMenu->addAction(tr("Play / Pause"));
    a->setShortcut(QKeySequence(tr("Ctrl+P")));
    a->setShortcutContext(Qt::ApplicationShortcut);
    connect(a, &QAction::triggered, this, &MainWindow::togglePlayback);
    a = timelineMenu->addAction(tr("Stop"));
    a->setShortcut(QKeySequence(tr("Ctrl+Shift+P")));
    a->setShortcutContext(Qt::ApplicationShortcut);
    connect(a, &QAction::triggered, this, &MainWindow::stopPlayback);
    timelineMenu->addSeparator();
    a = timelineMenu->addAction(tr("Set Cue - the Ride Starts Here"));
    a->setShortcut(QKeySequence(tr("Ctrl+K")));
    a->setShortcutContext(Qt::ApplicationShortcut);
    connect(a, &QAction::triggered, this, &MainWindow::setCueNow);
    a = timelineMenu->addAction(tr("Add Bookmark at Play Head"));
    a->setShortcut(QKeySequence(tr("Ctrl+M")));
    a->setShortcutContext(Qt::ApplicationShortcut);
    connect(a, &QAction::triggered, this, &MainWindow::addBookmarkAtPlayhead);
    timelineMenu->addSeparator();
    a = timelineMenu->addAction(tr("Load Music..."));
    connect(a, &QAction::triggered, this, &MainWindow::loadMusicDialog);
    a = timelineMenu->addAction(tr("Remove Music"));
    connect(a, &QAction::triggered, projectMedia::instance(), &projectMedia::clearMusic);

    connect(projectMedia::instance(), SIGNAL(playbackStateChanged(bool)), this, SLOT(onPlaybackStateChanged(bool)));

    // updates from GitHub
    quitWithoutAsking = false;
    mUpdater = new updater(this);
    connect(mUpdater, SIGNAL(aboutToInstall()), this, SLOT(quitForUpdate()));
    ui->menuHelp->addSeparator();
    a = ui->menuHelp->addAction(tr("Check for Updates..."));
    a->setMenuRole(QAction::ApplicationSpecificRole);
    connect(a, &QAction::triggered, mUpdater, &updater::checkNow);
    a = ui->menuHelp->addAction(tr("About LiquidFVD"));
    a->setMenuRole(QAction::AboutRole);
    connect(a, &QAction::triggered, this, &MainWindow::showAbout);
    QTimer::singleShot(4000, mUpdater, SLOT(checkOnStartup()));   // quiet check once the window is up

    connect(this, SIGNAL(emitMessage(QString,int)), ui->statusBar, SLOT(showMessage(QString,int)));
}

MainWindow::~MainWindow()
{
    delete ui;
    exit(0);
}

void MainWindow::initProject()
{
    ui->projectTab->init();
}


track* MainWindow::curTrack()
{
    if(ui->tabChooser->currentIndex() == 0) {
        return NULL;
    } else {
        trackWidget* curWidget = (trackWidget*)ui->tabChooser->currentWidget();
        return curWidget->inTrack->trackData;
    }
}

QList<trackHandler*> MainWindow::getTrackList()
{
    return ui->projectTab->trackList;
}

void MainWindow::on_actionExportAs_triggered()
{
    exportScreen->updateBoxes();
    exportScreen->show();
}

void MainWindow::updateWindowTitle()
{
    if(currentFileName.isEmpty()) {
        this->setWindowTitle(QString("LiquidFVD - unsaved Work"));
    } else {
        this->setWindowTitle(QString("LiquidFVD - " + currentFileName));
    }
}

void MainWindow::resetToUnsavedProject()
{
    stopPlayback();
    currentFileName.clear();
    updateWindowTitle();
    ui->projectTab->init();
    projectMedia::instance()->reset();
}

void MainWindow::on_actionNew_triggered()
{
    // keep a snapshot of whatever was open before it gets replaced
    backupSave();

    resetToUnsavedProject();

    // a new project gets its own folder right away: <dir>/<name>/<name>.fvd
    // cancelling keeps working on an unsaved project (still autosaved)
    QString file;
    if(askProjectLocation(tr("Create New Project"), &file)) {
        if(writeProject(file)) {
            currentFileName = file;
        }
    }
    updateWindowTitle();
    autosaveTimer->start();
}

void MainWindow::addProject(QString fileName) {
    ui->projectTab->importFromProject(fileName);
}

bool MainWindow::askProjectLocation(const QString& title, QString* projectFile)
{
    QSettings settings("FVD++", "FVD");
    QString startDir = settings.value("paths/lastProjectParent", QDir::homePath()).toString();
    if(!currentFileName.isEmpty()) {
        // suggest the folder that contains the current project folder
        QDir d = QFileInfo(currentFileName).absoluteDir();
        if(d.dirName() == backupManager::baseNameFor(currentFileName)) d.cdUp();
        startDir = d.absolutePath();
    }

    QString chosen = QFileDialog::getSaveFileName(this, title, startDir, tr("FVD Data (*.fvd)"));
    if(chosen.isEmpty()) return false;
    if(!chosen.endsWith(".fvd", Qt::CaseInsensitive)) chosen.append(".fvd");

    QString error;
    QString target = backupManager::makeProjectFolder(chosen, &error);
    if(target.isEmpty()) {
        QMessageBox::warning(this, tr("Application"), error);
        return false;
    }
    if(target != QFileInfo(chosen).absoluteFilePath() && QFileInfo(target).exists()) {
        if(QMessageBox::question(this, tr("Application"),
                                 tr("%1 already exists.\nDo you want to replace it?").arg(target),
                                 QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes) {
            return false;
        }
    }

    QDir parent = QFileInfo(target).absoluteDir();
    parent.cdUp();
    settings.setValue("paths/lastProjectParent", parent.absolutePath());

    *projectFile = target;
    return true;
}

bool MainWindow::writeProject(const QString& fileName)
{
    saver* gott = new saver(fileName, ui->projectTab, this);
    QString output = gott->doSave();
    delete gott;
    if(!output.contains("Error")) {
        QString err;
        if(!projectMedia::instance()->save(fileName, &err)) output = "Error: " + err;
    }
    showMessage(output.contains("Error") ? QString("Could not save %1").arg(fileName) : output);
    return !output.contains("Error");
}

void MainWindow::openProjectFile(const QString& fileName)
{
    // snapshot the project that is about to be closed
    stopPlayback();
    backupSave();

    currentFileName.clear();

    saver* gott = new saver(fileName, ui->projectTab, this);
    QString output = gott->doLoad();
    delete gott;

    bool loaded = true;
    if(output.contains("Warning:") || output.contains("Error:")) {
        QMessageBox mb(this);
        mb.setWindowTitle(tr("Application"));
        mb.setText(output);
        mb.setIcon(QMessageBox::Warning);
        mb.setDefaultButton(QMessageBox::Ok);
        mb.exec();

        if(output.contains("Error:")) {
            loaded = false;
            resetToUnsavedProject();
        }
    } else {
        ui->statusBar->showMessage(output, 5000);
    }

    glView->paintMode = true;

    if(loaded) {
        if(backupManager::isBackupFile(fileName)) {
            // opening a backup = restoring it into its project; never save over the backup itself
            currentFileName = backupManager::originalForBackup(fileName);
            QMessageBox::information(this, tr("Backup restored"),
                tr("You opened an autosave backup.\n\nIt will be saved to the project file\n%1\nwhen you press Save.")
                .arg(currentFileName));
        } else {
            currentFileName = fileName;
        }
        // music + bookmarks of the project (the backup case reads the original's)
        projectMedia::instance()->load(currentFileName);
        // first backup right after opening, then every few minutes
        backupSave();
    }
    autosaveTimer->start();
    updateWindowTitle();
}

void MainWindow::loadProject(QString fileName)
{
    openProjectFile(fileName);
}

#ifdef Q_OS_MAC
void MainWindow::on_actionLoad_triggered(QString fileName)
{
#endif
#ifndef Q_OS_MAC
void MainWindow::on_actionLoad_triggered()
{
    QString fileName;
#endif
    // on macOS a file double-clicked in the Finder arrives here already - open it without asking
    if(!fileName.isEmpty()) {
        openProjectFile(fileName);
        return;
    }
    glView->paintMode = false;
    QSettings settings("FVD++", "FVD");
    fileName = QFileDialog::getOpenFileName(this, "open FVD Data",
                                            settings.value("paths/lastProjectParent", QString()).toString(),
                                            "FVD Data(*.fvd);;Backed Up FVD Data(*.bak)", 0, 0);

    if(fileName.isEmpty()) {
        glView->paintMode = true;
        return;
    }

    openProjectFile(fileName);
}

void MainWindow::on_actionSave_triggered()
{
    if(glView->moveMode) {
        glView->cameraMov.z = 1.f;
        return;
    }

    if(currentFileName.isEmpty()) {
        on_actionSave_As_triggered();
        return;
    }
    writeProject(currentFileName);
    updateWindowTitle();
}

void MainWindow::backupSave()
{
    QString written;
    backupManager::Result r = backupManager::writeBackup(ui->projectTab, currentFileName, mOptions->backupCount, &written);
    if(r == backupManager::Written) {
        showMessage(QString("Autosave: %1").arg(QDir::toNativeSeparators(written)), 4000);
    } else if(r == backupManager::Failed) {
        showMessage(QString("Autosave failed - could not write to %1")
                    .arg(QDir::toNativeSeparators(backupManager::backupDirFor(currentFileName))), 8000);
    }
}

void MainWindow::on_actionSave_As_triggered()
{
    if(glView->moveMode) {
        return;
    }
    QString fileName;
    if(!askProjectLocation(tr("Save Project As"), &fileName)) {
        return;
    }
    if(writeProject(fileName)) {
        currentFileName = fileName;
        backupSave();
    }
    updateWindowTitle();
}

void MainWindow::applyAppearanceAndAutosave()
{
    theme::apply(mOptions->darkMode);

    darkModeAction->blockSignals(true);
    darkModeAction->setChecked(mOptions->darkMode);
    darkModeAction->blockSignals(false);

    autosaveTimer->setInterval(mOptions->autosaveMinutes * 60 * 1000);
    if(!autosaveTimer->isActive()) autosaveTimer->start();
}

void MainWindow::toggleDarkMode(bool on)
{
    mOptions->darkMode = on;
    mOptions->saveExtraSettings();
    applyAppearanceAndAutosave();
}

void MainWindow::on_actionQuit_triggered()
{
    this->close();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (quitWithoutAsking || areYouSure()) {
        backupSave();
        event->accept();
    } else {
        event->ignore();
    }
}

bool MainWindow::areYouSure()
{
    QMessageBox mb(this);
    mb.setIcon(QMessageBox::Warning);
    mb.setText(tr("Are you sure you want to quit?"));
    mb.setWindowTitle(tr("Application"));
    mb.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    int ret = mb.exec();

    if (ret == QMessageBox::Yes) {
        return true;
    } else {
        return false;
    }
}

void MainWindow::on_actionUndo_triggered()
{
    trackWidget* mWidget;
    if(ui->tabChooser->currentIndex() == 0) {
        if(ui->projectTab->selTrack) {
            mWidget = ui->projectTab->selTrack->trackWidgetItem;
        } else {
            ui->actionRedo->setEnabled(false);
            ui->actionUndo->setEnabled(false);
            return;
        }
    } else {
        mWidget = (trackWidget*)ui->tabChooser->currentWidget();
    }
    mWidget->inTrack->mUndoHandler->doUndo();
    setUndoButtons();
}

void MainWindow::on_actionRedo_triggered()
{
    trackWidget* mWidget;
    if(ui->tabChooser->currentIndex() == 0) {
        if(ui->projectTab->selTrack) {
            mWidget = ui->projectTab->selTrack->trackWidgetItem;
        } else {
            ui->actionRedo->setEnabled(false);
            ui->actionUndo->setEnabled(false);
            return;
        }
    } else {
        mWidget = (trackWidget*)ui->tabChooser->currentWidget();
    }
    mWidget->inTrack->mUndoHandler->doRedo();
    setUndoButtons();
}

void MainWindow::on_actionUseShader0_triggered()
{
    useShader(0);
}

void MainWindow::on_actionUseShader1_triggered()
{
    useShader(1);
}

void MainWindow::on_actionUseShader2_triggered()
{
    useShader(2);
}

void MainWindow::on_actionUseShader3_triggered()
{
    useShader(3);
}

void MainWindow::on_actionUseShader4_triggered()
{
    useShader(4);
}

void MainWindow::on_actionUseShader5_triggered()
{
    useShader(5);
}

void MainWindow::useShader(int shader)
{
    glView->curTrackShader = shader;
    glView->hasChanged = true;
    switch(shader) {
    case 0:
        ui->actionUseShader0->setChecked(true);
        ui->actionUseShader1->setChecked(false);
        ui->actionUseShader2->setChecked(false);
        ui->actionUseShader3->setChecked(false);
        ui->actionUseShader4->setChecked(false);
        ui->actionUseShader5->setChecked(false);
        break;
    case 1:
        ui->actionUseShader0->setChecked(false);
        ui->actionUseShader1->setChecked(true);
        ui->actionUseShader2->setChecked(false);
        ui->actionUseShader3->setChecked(false);
        ui->actionUseShader4->setChecked(false);
        ui->actionUseShader5->setChecked(false);
        break;
    case 2:
        ui->actionUseShader0->setChecked(false);
        ui->actionUseShader1->setChecked(false);
        ui->actionUseShader2->setChecked(true);
        ui->actionUseShader3->setChecked(false);
        ui->actionUseShader4->setChecked(false);
        ui->actionUseShader5->setChecked(false);
        break;
    case 3:
        ui->actionUseShader0->setChecked(false);
        ui->actionUseShader1->setChecked(false);
        ui->actionUseShader2->setChecked(false);
        ui->actionUseShader3->setChecked(true);
        ui->actionUseShader4->setChecked(false);
        ui->actionUseShader5->setChecked(false);
        break;
    case 4:
        ui->actionUseShader0->setChecked(false);
        ui->actionUseShader1->setChecked(false);
        ui->actionUseShader2->setChecked(false);
        ui->actionUseShader3->setChecked(false);
        ui->actionUseShader4->setChecked(true);
        ui->actionUseShader5->setChecked(false);
        break;
    case 5:
        ui->actionUseShader0->setChecked(false);
        ui->actionUseShader1->setChecked(false);
        ui->actionUseShader2->setChecked(false);
        ui->actionUseShader3->setChecked(false);
        ui->actionUseShader4->setChecked(false);
        ui->actionUseShader5->setChecked(true);
        break;
    }
}

void MainWindow::showCurInfoPanel()
{
    if(!glView->povMode) {
        return;
    }
    updateInfoPanel(glView->povNode);
}

void MainWindow::updateInfoPanel()
{
    if(curTrack() == NULL) {
        return;
    }

    trackWidget* temp = (trackWidget*)ui->tabChooser->currentWidget();
    temp->updateSectionFrame();
    mnode* lastnode;
    if(curTrack()->lSections.size() != 0 && curTrack()->activeSection != NULL) {
		lastnode = &curTrack()->activeSection->lNodes[curTrack()->activeSection->lNodes.size()-1];
	} else {
        lastnode = curTrack()->anchorNode;
    }
    updateInfoPanel(lastnode);
}

void MainWindow::updateInfoPanel(mnode* lastnode)
{
    if(curTrack() == NULL) {
        return;
    }

    float heart = curTrack()->fHeart;
    glm::mat4 anchorBase = glm::translate(curTrack()->startPos) * glm::rotate(TO_RAD(curTrack()->startYaw-90.f), glm::vec3(0.f, 1.f, 0.f));

    glm::vec3 worldPos = glm::vec3(anchorBase * glm::vec4(lastnode->vPosHeart(heart), 1.f));

    QString str1, str2, str3;

    str1 = QString::number(worldPos.x*mOptions->getLengthFactor(), 'f', 3);
    str2 = QString::number(worldPos.y*mOptions->getLengthFactor(), 'f', 3);
    str3 = QString::number(worldPos.z*mOptions->getLengthFactor(), 'f', 3);
    if(worldPos.x >= 0.f) str1.prepend('+');
    if(worldPos.y >= 0.f) str2.prepend('+');
    if(worldPos.z >= 0.f) str3.prepend('+');
    ui->infoPosLabel->setText(QString("X: ").append(str1).append(mOptions->getLengthString()).append(QString("  Y: ")).append(str2).append(mOptions->getLengthString()).append(QString("  Z: ")).append(str3).append(mOptions->getLengthString()));

    str1 = QString::number(lastnode->fVel*mOptions->getSpeedFactor(), 'f', 3);
    if(lastnode->fVel >= 0.f) str1.prepend('+');
    ui->infoSpeedLabel->setText(QString("Speed: ").append(str1).append(mOptions->getSpeedString()));

    str1 = QString::number(lastnode->getDirection()+curTrack()->startYaw, 'f', 3);
    str2 = QString::number(lastnode->getYawChange(), 'f', 3);
    if(!str1.startsWith('-')) str1.prepend('+');
    if(!str2.startsWith('-')) str2.prepend('+');
    ui->infoDirLabel->setText(QString("Yaw: ").append(str1).append(QString("%1 (").arg(QChar(0xb0))).append(str2).append(QString("%1/s)").arg(QChar(0xb0))));

    str1 = QString::number(lastnode->getPitch(), 'f', 3);
    str2 = QString::number(lastnode->getPitchChange(), 'f', 3);
    if(!str1.startsWith('-')) str1.prepend('+');
    if(!str2.startsWith('-')) str2.prepend('+');
    ui->infoPitchLabel->setText(QString("Pitch: ").append(str1).append(QString("%1 (").arg(QChar(0xb0))).append(str2).append(QString("%1/s)").arg(QChar(0xb0))));

    float temp = (lastnode->fRollSpeed + lastnode->fSmoothSpeed);
    str1 = QString::number(lastnode->fRoll, 'f', 3);
    str2 = QString::number(temp, 'f', 3);
    if(!str1.startsWith('-')) str1.prepend('+');
    if(!str2.startsWith('-')) str2.prepend('+');
    ui->infoRollLabel->setText(QString("Roll: ").append(str1).append(QString("%1 (").arg(QChar(0xb0))).append(str2).append(QString("%1/s)").arg(QChar(0xb0))));


    str1 = QString::number(lastnode->forceNormal + lastnode->smoothNormal, 'f', 3);
    str2 = QString::number(lastnode->forceLateral + lastnode->smoothLateral, 'f', 3);
    if(!str1.startsWith('-')) str1.prepend('+');
    if(!str2.startsWith('-')) str2.prepend('+');
    ui->infoNormalLabel->setText(QString("y-Accel: ").append(str1).append(QString("g")));
    ui->infoLateralLabel->setText(QString("x-Accel: ").append(str2).append(QString("g")));
}

void MainWindow::setUndoButtons()
{
    trackWidget* mWidget;
    if(ui->tabChooser->currentIndex() == 0) {
        if(ui->projectTab->selTrack) {
            mWidget = ui->projectTab->selTrack->trackWidgetItem;
        } else {
            ui->actionRedo->setEnabled(false);
            ui->actionUndo->setEnabled(false);
            return;
        }
    } else {
        mWidget = (trackWidget*)ui->tabChooser->currentWidget();
    }

    if(mWidget == NULL) {
        ui->actionRedo->setEnabled(false);
        ui->actionUndo->setEnabled(false);
        return;
    }

    if(mWidget->inTrack->mUndoHandler->stackIndex > 0) {
        ui->actionRedo->setEnabled(true);
    } else {
        ui->actionRedo->setEnabled(false);
    }

    if(mWidget->inTrack->mUndoHandler->stackIndex != -1 && mWidget->inTrack->mUndoHandler->stackIndex != mWidget->inTrack->mUndoHandler->lActions.size()) {
        ui->actionUndo->setEnabled(true);
    } else {
        ui->actionUndo->setEnabled(false);
    }
}

void MainWindow::displayStatusMessage(QString message)
{
    ui->statusBar->showMessage(message, 5000);
}

void MainWindow::on_actionOptions_triggered()
{
    mOptions->setGLVersionString(glView->getGLVersionString());
    mOptions->syncExtraOptions();
    mOptions->show();
}

void MainWindow::on_actionConversion_Panel_triggered()
{
    mConversion->show();
}

int MainWindow::getPovPos()
{
    return glView->povPos;
}

QString MainWindow::getCurrentFileName() {
    return currentFileName;
}

void MainWindow::openTab(trackHandler* _track)
{
    this->setUpdatesEnabled(false);
    _track->trackWidgetItem->on_sectionListWidget_itemSelectionChanged();
    if(_track->tabId == -1) {
        _track->tabId = ui->tabChooser->addTab(_track->trackWidgetItem, _track->trackData->name);
    }
    ui->tabChooser->setCurrentWidget(_track->trackWidgetItem);
    this->setUpdatesEnabled(true);
}

void MainWindow::renameTab(trackHandler* _track)
{
    ui->tabChooser->setTabText(_track->tabId, _track->trackData->name);
}

void MainWindow::sectionChanged()
{

}

void MainWindow::on_tabChooser_currentChanged(int index)
{
    if(phantomChanges) return;
    if(projectMedia::instance()->isPlaying() || glView->povMode) stopPlayback();

    phantomChanges = true;

    if(mGraphWidget) {
        mGraphWidget->setParent(NULL);
        mGraphWidget = NULL;
    }
    if(index) {
        //this->updatesEnabled(false);
        trackWidget* widget = (trackWidget*)ui->tabChooser->widget(index);
        mGraphWidget = widget->inTrack->graphWidgetItem;
        ui->vertSplitter->insertWidget(1, mGraphWidget);
        mGraphWidget->show();
        mGraphWidget->update();
        ui->tabChooser->setCurrentIndex(0); // go to index 0 to apply size changes
        for(int i = 0; i < ui->tabChooser->count(); ++i) {
            ui->tabChooser->widget(i)->setGeometry(0, 0, ui->tabChooser->widget(i)->width(), ui->tabChooser->widget(i)->height()-150);
        }
        ui->tabChooser->setGeometry(0, 0, ui->tabChooser->width(), ui->tabChooser->height()-150);
        ui->tabChooser->setCurrentIndex(index);
    }
    setUndoButtons();
    phantomChanges = false;
}

void MainWindow::on_tabChooser_tabCloseRequested(int index)
{
    if(!index) {
        return;
    }

    trackWidget* widget = (trackWidget*)ui->tabChooser->widget(index);
    widget->clearSelection();
    widget->inTrack->tabId = -1;

    ui->tabChooser->removeTab(index);
}

void MainWindow::keyPressed(QEvent *event)
{
    keyPressEvent((QKeyEvent*)event);
}

void MainWindow::keyReleased(QEvent *event)
{
    keyReleaseEvent((QKeyEvent*)event);
}

void MainWindow::updateProjectWidget()
{
    ui->projectTab->on_trackListWidget_itemSelectionChanged();
}

void MainWindow::showMessage(QString msg, int msec)
{
    emit emitMessage(msg, msec);
}

void MainWindow::doAutoSave()
{
    backupSave();
}

void MainWindow::hideAll()
{
    ui->centralWidget->layout()->setContentsMargins(0, 0, 0, 0);
    ui->menuBar->hide();
    ui->infoFrame->hide();
    if(mGraphWidget) mGraphWidget->hide();
    ui->tabFrame->hide();
    ui->statusBar->hide();
}

void MainWindow::showAll()
{
    ui->centralWidget->layout()->setContentsMargins(9, 9, 9, 9);
    ui->menuBar->show();
    ui->infoFrame->show();
    if(mGraphWidget) mGraphWidget->show();
    ui->tabFrame->show();
    ui->statusBar->show();
}

void MainWindow::on_actionExport_Model_As_triggered()
{
    mObjectExporter->update();
    mObjectExporter->show();
}

void MainWindow::on_actionExport_triggered()
{
    if(exportScreen->updateBoxes()) {
        exportScreen->doFastExport();
    } else {
        on_actionExportAs_triggered();
    }
}

// ---------------------------------------------------------------------------
//  Music, play-along in POV view, bookmarks
// ---------------------------------------------------------------------------

static trackHandler* handlerOf(track* t)
{
    QList<trackHandler*> list = gloParent->getTrackList();
    for(int i = 0; i < list.size(); ++i) if(list[i]->trackData == t) return list[i];
    return NULL;
}

double MainWindow::playheadTime()
{
    projectMedia* media = projectMedia::instance();
    if(media->isPlaying() || glView->povMode) return glView->povPos / F_HZ;
    return media->cursorTime();
}

void MainWindow::togglePlayback()
{
    projectMedia* media = projectMedia::instance();
    if(media->isPlaying()) {
        // pause: stay in POV view at the current spot
        media->stopPlayback();
        media->setCursorTime(glView->povPos / F_HZ);
        return;
    }

    track* t = curTrack();
    if(!t || !mGraphWidget) {
        showMessage(tr("Open a track (double-click it in the project tab) to play it"));
        return;
    }
    const int n = t->getNumPoints();
    if(n < 2) {
        showMessage(tr("The track has no sections to ride yet"));
        return;
    }

    double start = playheadTime();
    if(start*F_HZ >= n - 10 || start < 0.) start = 0.;   // at the end: start over

    // pre-roll: from the start of the ride, the song plays from its beginning and the train
    // waits in the station until the cue
    double clock = start;
    if(start < 0.05 && media->hasMusic() && media->preRoll() && media->cue() > 0.) clock = -media->cue();

    glView->povPos = (int)((clock > 0. ? clock : 0.) * F_HZ);
    glView->povNode = t->getPoint(glView->povPos);
    glView->povMode = true;
    mGraphWidget->setPovLineVisible(true);

    media->startPlayback(clock);
    if(media->hasMusic() && !media->audioDeviceAvailable()) {
        showMessage(tr("No audio output found - playing without sound"), 6000);
    }
}

void MainWindow::stopPlayback()
{
    projectMedia* media = projectMedia::instance();
    media->stopPlayback();
    media->setCursorTime(0.);
    if(glView->povMode) {
        glView->povMode = false;
        glView->povPos = 0;
        if(mGraphWidget) mGraphWidget->drawGraph(10); // 10 = povPos, hides the line
    }
    if(mGraphWidget && mGraphWidget->timeline) mGraphWidget->timeline->refresh();
}

void MainWindow::seekPlayback(double trackTime)
{
    track* t = curTrack();
    if(!t) return;
    double dur = t->getNumPoints() / F_HZ;
    if(trackTime < 0.) trackTime = 0.;
    if(trackTime > dur) trackTime = dur;

    projectMedia* media = projectMedia::instance();
    media->setCursorTime(trackTime);
    if(glView->povMode || media->isPlaying()) {
        glView->povPos = (int)(trackTime * F_HZ);
        glView->povNode = t->getPoint(glView->povPos);
        if(media->isPlaying()) media->startPlayback(trackTime);   // re-sync the song
        if(mGraphWidget) mGraphWidget->drawGraph(10);
    }
    if(mGraphWidget && mGraphWidget->timeline) mGraphWidget->timeline->refresh();
}

void MainWindow::onPlaybackStateChanged(bool playing)
{
    Q_UNUSED(playing);
    if(mGraphWidget && mGraphWidget->timeline) mGraphWidget->timeline->refresh();
}

void MainWindow::loadMusicDialog()
{
    QSettings settings("FVD++", "FVD");
    QString startDir = settings.value("paths/lastMusicDir",
                                      currentFileName.isEmpty() ? QDir::homePath() : QFileInfo(currentFileName).absolutePath()).toString();
    glView->paintMode = false;
    QString file = QFileDialog::getOpenFileName(this, tr("Load Music"), startDir,
                                                tr("Audio (*.wav *.mp3 *.flac *.m4a *.aac *.aif *.aiff *.caf *.ogg);;All Files (*)"));
    glView->paintMode = true;
    if(file.isEmpty()) return;
    settings.setValue("paths/lastMusicDir", QFileInfo(file).absolutePath());

    QString err;
    if(!projectMedia::instance()->loadMusic(file, &err)) {
        QMessageBox::warning(this, tr("Load Music"), err);
        return;
    }
    showMessage(tr("Music loaded: %1 (%2 s)").arg(QFileInfo(file).fileName())
                .arg(projectMedia::instance()->musicDuration(), 0, 'f', 1));
}

void MainWindow::addBookmarkAtPlayhead()
{
    projectMedia* media = projectMedia::instance();
    double t = playheadTime();
    bool onSong = media->hasMusic() && t >= media->musicStart() && t <= media->musicStart() + media->musicDuration();
    addBookmarkAt(t, onSong ? bookmark::Music : bookmark::Track);
}

void MainWindow::addBookmarkAt(double trackTime, bookmark::Kind kind)
{
    projectMedia* media = projectMedia::instance();
    trackHandler* th = handlerOf(curTrack());
    if(kind == bookmark::Track && !th) {
        showMessage(tr("Open a track to place a ride bookmark"));
        return;
    }
    if(kind == bookmark::Music && !media->hasMusic()) kind = bookmark::Track;
    if(kind == bookmark::Track && !th) return;
    bookmark b = media->makeBookmark(trackTime, th, kind, QString());
    media->addBookmark(b);
    showMessage(tr("%1 set at %2 s (%3)").arg(b.name).arg(trackTime, 0, 'f', 2)
                .arg(kind == bookmark::Music ? tr("pinned to the song") : tr("pinned to the ride")));
}

// ---------------------------------------------------------------------------
//  Updates / about
// ---------------------------------------------------------------------------

void MainWindow::quitForUpdate()
{
    stopPlayback();
    backupSave();                 // the open project is safe in its autosave ring
    quitWithoutAsking = true;
    close();
    QTimer::singleShot(3000, [](){ exit(0); });   // in case something keeps the event loop alive
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About LiquidFVD"),
        tr("<h3>LiquidFVD %1</h3>"
           "<p>Force Vector Design for roller coasters, based on FVD++ 0.79 by Stephan \"Lenny\" Alt, "
           "extended by Vivent Group.</p>"
           "<p>New: autosave backups, dark mode, graph handles, music timeline with play-along POV, "
           "bookmarks and automatic updates.</p>"
           "<p><a href=\"https://github.com/Vivent-Group/LiquidFVD\">github.com/Vivent-Group/LiquidFVD</a><br>"
           "License: GNU GPL v3. Audio: miniaudio (public domain / MIT-0).</p>").arg(updater::currentVersion()));
}

void MainWindow::setCueNow()
{
    projectMedia* media = projectMedia::instance();
    if(!media->hasMusic()) {
        showMessage(tr("Load a song first (Timeline > Load Music)"));
        return;
    }
    double song;
    if(media->isPlaying()) {
        // live: the ride starts right now, at what you hear
        song = media->songPositionAt(media->playbackTrackTime());
    } else {
        song = media->songPositionAt(playheadTime());
    }
    if(song < 0.) song = 0.;
    if(song > media->musicDuration()) song = media->musicDuration();
    song = std::floor(song*100. + 0.5)/100.;
    media->setCue(song);
    if(!media->isPlaying()) seekPlayback(0.);
    int m = (int)(song/60.);
    showMessage(tr("Cue set: the ride starts at %1:%2 in the song").arg(m).arg(song - m*60., 5, 'f', 2, QChar('0')));
}
