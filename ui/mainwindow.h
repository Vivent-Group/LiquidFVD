#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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

#include <QMainWindow>
#include "glviewwidget.h"
#include "trackhandler.h"
#include "sectionhandler.h"

#include <QtGlobal>

#include "track.h"
#include "saver.h"
#include "projectmedia.h"

#include <fstream>
#include <QStyledItemDelegate>


// some defines

// TYPES
#define STR_SEC 1
#define CUR_SEC 2
#define FRC_SEC 3
#define GEO_SEC 4
#define NODE 5
#define VECTOR 6
#define FLOAT 7
#define GLO_FUNC 8
#define BEZ_SEC 9

// PLOT FUNCS
#define PLOT_ALLFUNCS QString("")
#define PLOT_VERTFUNC QString("VerticalLine")
#define PLOT_ROLLFUNC QString("RollFunction")
#define PLOT_NORMFUNC QString("NormalForce")
#define PLOT_LATFUNC QString("LateralForce")
#define PLOT_FLEXFUNC QString("TrackFlexion")
#define PLOT_DIRFUNC QString("Direction")

// TREE ITEMS
#define TREE_ROLLFUNC QString("Roll Change")
#define TREE_NORMFUNC QString("Normal Force")
#define TREE_LATFUNC QString("Lateral Force")
#define TREE_FLEXFUNC QString("Pitch Change")
#define TREE_DIRFUNC QString("Yaw Change")


class exportUi;
class undoHandler;
class optionsMenu;
class conversionPanel;
class graphWidget;
class trackHandler;
class objectExporter;
class QTimer;
class QAction;
class updater;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void backupSave();
    void setUndoButtons();
    void updateBoxes();
    void displayStatusMessage(QString message);
    QString getGLVersionString();
    QString getCurrentFileName();
    int getPovPos();
    track* curTrack();
    QList<trackHandler*> getTrackList();
    void updateInfoPanel();
    void updateInfoPanel(mnode* lastnode);
    void openTab(trackHandler* _track);
    void renameTab(trackHandler* _track);
    void sectionChanged();
    void initProject();
    void hideAll();
    void showAll();
    void addProject(QString fileName);
    void loadProject(QString fileName);

    // re-reads dark mode + autosave interval from the options and applies them
    void applyAppearanceAndAutosave();

    // ---- music / play-along / bookmarks ----
    void togglePlayback();          // play from the play head, riding along in POV view
    void stopPlayback();            // stop, leave POV view, play head back to the start
    void seekPlayback(double trackTime);
    void loadMusicDialog();
    void addBookmarkAtPlayhead();
    void addBookmarkAt(double trackTime, bookmark::Kind kind);
    double playheadTime();

    void updateProjectWidget();

    void keyPressed(QEvent *event);
    void keyReleased(QEvent *event);


    subfunc* selectedFunc;
    QTreeWidgetItem* selectedSection;
    optionsMenu* mOptions;
    graphWidget* mGraphWidget;

    projectWidget* project;

    bool undoChanges;

signals:
    void emitMessage(QString msg, int msec = 5000);

public slots:
    void showCurInfoPanel();

    void on_actionNew_triggered();

#ifdef Q_OS_MAC
    void on_actionLoad_triggered(QString fileName = "");
#endif
#ifndef Q_OS_MAC
    void on_actionLoad_triggered();
#endif

    void on_actionSave_triggered();

    void on_actionSave_As_triggered();

    void on_actionQuit_triggered();

    void closeEvent(QCloseEvent *event);

    void on_actionUseShader0_triggered();

    void on_actionUseShader1_triggered();

    void on_actionUseShader2_triggered();

    void on_actionUseShader3_triggered();

    void on_actionUseShader4_triggered();

    void on_actionUseShader5_triggered();

    void on_actionUndo_triggered();

    void on_actionRedo_triggered();

    void on_actionExportAs_triggered();

    void on_actionOptions_triggered();

    void on_actionConversion_Panel_triggered();

    void on_tabChooser_currentChanged(int index);

    void on_tabChooser_tabCloseRequested(int index);

    void doAutoSave();

    void showMessage(QString msg, int msec = 5000);

    void toggleDarkMode(bool on);
    void onPlaybackStateChanged(bool playing);
    void quitForUpdate();           // backup + quit without the "are you sure" question
    void showAbout();

private slots:
    void on_actionExport_Model_As_triggered();

    void on_actionExport_triggered();

private:
    Ui::MainWindow *ui;
    void useShader(int shader);
    bool areYouSure();

    // asks for a file name and turns it into <dir>/<name>/<name>.fvd (creating the folder)
    bool askProjectLocation(const QString& title, QString* projectFile);
    // loads a project file and takes care of messages, backup restore and the initial backup
    void openProjectFile(const QString& fileName);
    // starts an empty, unsaved project without asking anything
    void resetToUnsavedProject();
    bool writeProject(const QString& fileName);
    void updateWindowTitle();

    QTimer*  autosaveTimer;
    QAction* darkModeAction;
    updater* mUpdater;
    bool     quitWithoutAsking;

    bool phantomChanges;
    QString     currentFileName;
    exportUi* exportScreen;
    conversionPanel* mConversion;
    objectExporter* mObjectExporter;
};


class NoEditDelegate : public QStyledItemDelegate
{
    public:
      NoEditDelegate(QObject* parent=0): QStyledItemDelegate(parent) {}
      virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex &index) const {
        Q_UNUSED(parent);
        Q_UNUSED(option);
        Q_UNUSED(index);
        return 0;
      }
    };

#endif // MAINWINDOW_H
