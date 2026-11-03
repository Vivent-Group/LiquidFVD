#ifndef GRAPHWIDGET_H
#define GRAPHWIDGET_H

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

#include <QWidget>
#include <QList>
#include "graphhandler.h"

class dragLabel;
class transitionWidget;
class sectionHandler;
class QCPAxis;
class QCPGraph;
class QCustomPlot;
class func;
class timelineStrip;
class section;

namespace Ui {
class graphWidget;
}

class graphWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit graphWidget(QWidget *parent, trackHandler* _track = 0);
    virtual ~graphWidget();
    void drawGraph(int index);
    bool drawPoVLine();
    void undrawGraph(int index);
    void curSectionChanged(sectionHandler* _section);
    void activateGraph(graphHandler* curGraph);
    void deactivateGraph(graphHandler* curGraph);
    void redrawGraphs(bool otherArgument = false);
    bool changeSelection(subfunc* _sel);
    void keyPressEvent(QKeyEvent* event);

    trackHandler* selTrack;
    subfunc* selFunc;
    transitionWidget* transitionHandler;

    QCPAxis* yAxes[4];

    QCustomPlot* plot();
    // converts between plot x coordinates and track time (seconds), honouring TIME/DISTANCE mode
    double coordToTime(double coord);
    double timeToCoord(double t);
    // rebuilds the bookmark lines drawn into the plot
    void updateBookmarkItems();
    void setPovLineVisible(bool on);
    timelineStrip* timeline;

protected:
    bool eventFilter(QObject* obj, QEvent* event);


public slots:
    void on_selTree_itemChanged(QTreeWidgetItem *item, int column);
    void MousePressedPlotter();
    void setPlotRanges();
    void MouseWheelPlotter();
    void selectionChanged();

    void on_plotter_customContextMenuRequested(const QPoint &pos);
    void setBezPoints();
    void onBookmarksChanged();

private:
    Ui::graphWidget *ui;
    QList<graphHandler*> pGraphList;
    QList<dragLabel*> bezPoints;
    bool phantomChanges;

    // ---- draggable transition end points ("handles") ----
    struct handleInfo { int vertex; double x; double y; };
    QList<handleInfo> currentHandles();     // vertices of the selected function, in plot coordinates
    func* handleFunc();                     // function the handles belong to (or NULL)
    QCPAxis* valueAxisFor(func* f);
    double sectionStartCoord();
    int  handleAt(const QPoint& pixel);     // vertex under the mouse, -1 if none
    void updateHandleGraph();
    void applyHandleDrag(const QPoint& pixel, bool freeMove);
    void finishHandleDrag();
    double niceStep(QCPAxis* axis, double minStep);

    QList<func*> sectionFuncs(section* sec);

    QCPGraph* handleGraph;
    int    dragVertex;                      // -1 = no drag in progress
    func*  dragFunc;
    QList<func*> dragFuncs;                 // all functions of the section (the last point moves them all)
    QList<QList<float> > dragOldLength;     // per function / transition, captured at drag start
    QList<QList<float> > dragOldSym;
    float  dragNextMax;                     // end of the following transition, kept fixed while dragging
    float  dragOrigMax;                     // dragged point at drag start
    float  dragOrigVal;
    float  dragOrigSectionEnd;
    QPoint dragPressPos;
    QPoint dragGrabOffset;                  // point centre - click position
    qint64 lastDragApply;
};

#endif // GRAPHWIDGET_H
