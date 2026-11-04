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

#include "graphwidget.h"
#include "theme.h"
#include "mainwindow.h"
#include "graphhandler.h"
#include "ui_graphwidget.h"
#include "optionsmenu.h"
#include "draglabel.h"
#include "lenassert.h"
#include "smoothui.h"
#include "timelinestrip.h"
#include "curveeditor.h"
#include "projectmedia.h"
#include "trackhandler.h"
#include "transitionwidget.h"
#include "undohandler.h"
#include "undoaction.h"
#include "section.h"
#include "function.h"
#include "subfunction.h"
#include <QMouseEvent>
#include <QToolTip>
#include <QDateTime>
#include <QVBoxLayout>
#include <cmath>
#include <cstdlib>

extern MainWindow* gloParent;
extern glViewWidget* glView;

graphWidget::graphWidget(QWidget *parent, trackHandler* _track) :
    QWidget(parent),
    ui(new Ui::graphWidget)
{
    phantomChanges = true;
    timeline = NULL;
    editor = NULL;
    ui->setupUi(this);

    transitionHandler = ui->transitionEditor;

    selTrack = _track;
    selFunc = NULL;

    this->setMaximumHeight(420);
    this->setMinimumHeight(280);

    ui->povLine->hide();

    ui->selTree->setColumnWidth(0, 180);
    ui->selTree->setColumnWidth(1, 50);

    ui->plotter->yAxis->grid()->setVisible(false);
    ui->plotter->yAxis2->setVisible(true);

    yAxes[0] = ui->plotter->yAxis;
    yAxes[1] = ui->plotter->yAxis2;
    yAxes[2] = ui->plotter->axisRect()->addAxis(QCPAxis::atRight);
    yAxes[3] = ui->plotter->axisRect()->addAxis(QCPAxis::atLeft);

    yAxes[0]->setLabel(QString("Banking"));
    yAxes[1]->setLabel(QString("Forces"));
    yAxes[2]->setLabel(QString("Pitch"));
    yAxes[3]->setLabel(QString("Yaw"));

    // background, axes and grid colors follow the light / dark theme
    theme::stylePlot(ui->plotter);

    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(1)->child(0)->child(0), banking, selTrack, yAxes[0], gloParent->mOptions->rollColor));
    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(1)->child(0)->child(1), rollSpeed, selTrack, yAxes[0], gloParent->mOptions->rollColor));
    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(1)->child(0)->child(2), rollAccel, selTrack, yAxes[0], gloParent->mOptions->rollColor));

    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(1)->child(1)->child(0), nForce, selTrack, yAxes[1], gloParent->mOptions->normColor));
    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(1)->child(1)->child(1), nForceChange, selTrack, yAxes[1], gloParent->mOptions->normColor));

    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(1)->child(2)->child(0), lForce, selTrack, yAxes[1], gloParent->mOptions->latColor));
    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(1)->child(2)->child(1), lForceChange, selTrack, yAxes[1], gloParent->mOptions->latColor));

    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(1)->child(3)->child(0), pitchChange, selTrack, yAxes[2], gloParent->mOptions->pitchColor));
    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(1)->child(3)->child(1), yawChange, selTrack, yAxes[3], gloParent->mOptions->yawColor));

    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(2)->child(0), secBoundaries, selTrack, ui->plotter->yAxis, NULL));
    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(2)->child(1), povPos, selTrack, NULL, NULL));

    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(1)->child(0)->child(1), smoothedRollSpeed, selTrack, yAxes[0], gloParent->mOptions->rollColor));
    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(1)->child(1)->child(0), smoothedNForce, selTrack, yAxes[1], gloParent->mOptions->normColor));
    pGraphList.append(new graphHandler(ui->selTree->topLevelItem(1)->child(2)->child(0), smoothedLForce, selTrack, yAxes[1], gloParent->mOptions->latColor));

    ui->plotter->setInteraction(QCP::iRangeDrag, true);
    ui->plotter->setInteraction(QCP::iRangeZoom, true);
    ui->plotter->setInteraction(QCP::iSelectPlottables, true);

    ui->plotter->axisRect()->setRangeDrag(Qt::Horizontal);
    ui->plotter->axisRect()->setRangeZoomAxes(ui->plotter->xAxis, NULL);

    connect(ui->plotter, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(MousePressedPlotter()));
    connect(ui->plotter, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(setPlotRanges()));
    connect(ui->plotter, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(MouseWheelPlotter()));

    ui->plotter->setNotAntialiasedElement((QCP::AntialiasedElement)(0xFFFF ^ 0x0020), true);
    ui->plotter->setPlottingHint(QCP::phForceRepaint, true);


    connect(ui->plotter, SIGNAL(selectionChangedByUser()), this, SLOT(selectionChanged()));
    ui->tabWidget->removeTab(1);

    if(selTrack) {
        pGraphList[secBoundaries]->treeItem->setCheckState(1, Qt::Checked);
        pGraphList[secBoundaries]->drawn = true;
        pGraphList[povPos]->treeItem->setCheckState(1, Qt::Unchecked);
    }

    ui->transitionEditor->mParent = this;

    // drag points and lines of the graph directly ("like dough")
    editor = new curveEditor(this);

    // music waveform, transport and bookmarks below the graph
    // the plot sits in a horizontal layout - stack plot + strip vertically in its place,
    // so both share exactly the same left edge and width (= the same x axis)
    timeline = new timelineStrip(this);
    {
        QBoxLayout* host = qobject_cast<QBoxLayout*>(ui->frame->layout());
        int idx = host ? host->indexOf(ui->plotter) : -1;
        QVBoxLayout* column = new QVBoxLayout();
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(2);
        if(host) host->removeWidget(ui->plotter);
        column->addWidget(ui->plotter, 1);
        column->addWidget(timeline, 0);
        if(host) host->insertLayout(idx < 0 ? 0 : idx, column, 1);
    }
    connect(ui->plotter->xAxis, SIGNAL(rangeChanged(QCPRange)), timeline, SLOT(refresh()));
    connect(projectMedia::instance(), SIGNAL(bookmarksChanged()), this, SLOT(onBookmarksChanged()));

    phantomChanges = false;
}

QCustomPlot* graphWidget::plot()
{
    return ui->plotter;
}

void graphWidget::setPovLineVisible(bool on)
{
    if(!selTrack) return;
    QTreeWidgetItem* item = pGraphList[povPos]->treeItem;
    if((item->checkState(1) == Qt::Checked) == on) {
        if(on) drawGraph(povPos);
        return;
    }
    item->setCheckState(1, on ? Qt::Checked : Qt::Unchecked);   // draws / hides via on_selTree_itemChanged
}

void graphWidget::onBookmarksChanged()
{
    updateBookmarkItems();
    ui->plotter->replot();
    timeline->refresh();
}

double graphWidget::coordToTime(double coord)
{
    if(!selTrack) return coord;
    track* t = selTrack->trackData;
    if(t->activeSection && t->activeSection->bArgument == DISTANCE) {
        return t->getIndexFromDist(coord)/F_HZ;
    }
    return coord;
}

double graphWidget::timeToCoord(double time)
{
    if(!selTrack) return time;
    track* t = selTrack->trackData;
    if(t->activeSection && t->activeSection->bArgument == DISTANCE) {
        int n = t->getNumPoints();
        int i = (int)(time*F_HZ + 0.5);
        if(i < 0) i = 0;
        if(i > n) i = n;
        mnode* node = t->getPoint(i);
        return node ? node->fTotalLength : 0.;
    }
    return time;
}

void graphWidget::updateBookmarkItems()
{
    // remove the old marker items
    for(int i = ui->plotter->itemCount()-1; i >= 0; --i) {
        QCPAbstractItem* it = ui->plotter->item(i);
        if(it->property("fvdBookmark").toBool()) ui->plotter->removeItem(it);
    }
    if(!selTrack) return;

    projectMedia* media = projectMedia::instance();
    QList<bookmark> marks = media->bookmarks();
    for(int i = 0; i < marks.size(); ++i) {
        double t = media->bookmarkTrackTime(marks[i], selTrack);
        if(t < 0) continue;
        double x = timeToCoord(t);
        QColor c = marks[i].color();

        QCPItemStraightLine* line = new QCPItemStraightLine(ui->plotter);
        ui->plotter->addItem(line);
        line->setProperty("fvdBookmark", true);
        line->setSelectable(false);
        // two points with the same x = an infinite vertical line, independent of the y range
        line->point1->setType(QCPItemPosition::ptPlotCoords);
        line->point2->setType(QCPItemPosition::ptPlotCoords);
        line->point1->setCoords(x, 0.0);
        line->point2->setCoords(x, 1.0);
        line->setPen(QPen(c, 1.5, Qt::DashLine));

        QCPItemText* label = new QCPItemText(ui->plotter);
        ui->plotter->addItem(label);
        label->setProperty("fvdBookmark", true);
        label->setSelectable(false);
        label->position->setType(QCPItemPosition::ptPlotCoords);
        label->position->setCoords(x, ui->plotter->yAxis->range().upper);
        label->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
        label->setPadding(QMargins(3, 1, 3, 1));
        label->setText(marks[i].name);
        label->setColor(Qt::white);
        label->setBrush(QBrush(c));
        label->setClipToAxisRect(true);
    }
}

QList<QPair<func*, QCPAxis*> > graphWidget::drawnEditableFunctions()
{
    QList<QPair<func*, QCPAxis*> > list;
    if(!selTrack) return list;
    section* sec = selTrack->trackData->activeSection;
    if(!sec || (sec->type != forced && sec->type != geometric)) return list;
    struct { int graph; func* f; int axis; } map[] = {
        { rollSpeed,   sec->rollFunc,  0 },
        { nForce,      sec->normForce, 1 },
        { lForce,      sec->latForce,  1 },
        { pitchChange, sec->normForce, 2 },
        { yawChange,   sec->latForce,  3 },
    };
    for(unsigned i = 0; i < sizeof(map)/sizeof(map[0]); ++i) {
        graphHandler* gh = pGraphList[map[i].graph];
        if(map[i].f && gh->active && gh->drawn) list.append(qMakePair(map[i].f, yAxes[map[i].axis]));
    }
    return list;
}

void graphWidget::selectTransition(subfunc* s)
{
    if(!s || s == selFunc) return;
    changeSelection(s);
    ui->tabWidget->addTab(ui->transitionEditor, "Transition Editor");
    ui->tabWidget->setCurrentWidget(ui->transitionEditor);
    selTrack->trackData->hasChanged = true;
    redrawGraphs();
}

graphWidget::~graphWidget()
{
    for(int i = 0; i < pGraphList.size(); ++i) {
        delete pGraphList[i];
    }
    delete ui;
}

void graphWidget::on_selTree_itemChanged(QTreeWidgetItem *item, int column)
{
    if(column != 1 || phantomChanges) {
        return;
    }

    int i;
    for(i = 0; i < pGraphList.size(); ++i) {
        if(pGraphList[i]->treeItem == item) break;
    }

    lenAssert(i < pGraphList.size());
    if(i == pGraphList.size()) {
        return;
    }

    if(item->checkState(1) == Qt::Checked) {
        if(i%2 && (i-1)/2 >= 0 && (i-1)/2 < 3 && selTrack->trackData->smoother && selTrack->trackData->smoother->active()) {
            drawGraph(11+(i-1)/2);
        }
        drawGraph(i);
    } else {
        if(i%2 && (i-1)/2 >= 0 && (i-1)/2 < 3 && selTrack->trackData->smoother && selTrack->trackData->smoother->active()) {
            undrawGraph(11+(i-1)/2);
        }
        undrawGraph(i);
    }

    if(item == pGraphList[secBoundaries]->treeItem) {
        redrawGraphs();
    }

    ui->plotter->replot();
}

void graphWidget::setPlotRanges()
{
    // keep the axes still while a handle is dragged, otherwise the point would run away from the mouse
    if(editor && editor->isDragging()) return;

    for(int i = 0; i < bezPoints.size(); ++i) {
        if(bezPoints[i]->isDragged) {
            return;
        }
    }

    double lowerBound, upperBound;
    double maxKey = 1;
    double minKey = std::numeric_limits<double>::max();
    double edge;
    bool valid;
    for(int a = 0; a < 4; ++a) {
        yAxes[a]->setVisible(false);
        upperBound = -100000;
        lowerBound = 100000;
        for(int i = 0; i < 9; ++i) {
            graphHandler* curGraph = pGraphList[i];
            if(curGraph->drawn && curGraph->usedAxis == yAxes[a]) {
                curGraph->usedAxis->setVisible(true);
                if(curGraph->graphList.size()) {
                    maxKey = maxKey > curGraph->graphList.last()->getKeyRange(valid).upper ? maxKey :curGraph->graphList.last()->getKeyRange(valid).upper;
                    minKey = minKey < curGraph->graphList.first()->getKeyRange(valid).lower ? minKey : curGraph->graphList.first()->getKeyRange(valid).lower;
                }
                for(int j = 0; j < curGraph->graphList.size(); ++j) {
                    if(curGraph->graphList[j]->getKeyRange(valid).upper > ui->plotter->xAxis->range().lower && curGraph->graphList[j]->getKeyRange(valid).lower < ui->plotter->xAxis->range().upper) {
                        lowerBound = lowerBound < curGraph->graphList[j]->getValueRange(valid).lower ? lowerBound : curGraph->graphList[j]->getValueRange(valid).lower;
                        upperBound = upperBound > curGraph->graphList[j]->getValueRange(valid).upper ? upperBound : curGraph->graphList[j]->getValueRange(valid).upper;
                    }
                }
            }
        }
        edge = (upperBound-lowerBound)/(6-a);
        edge = edge > fabs(upperBound) ? edge : fabs(upperBound)/(6-a);
        edge = edge > fabs(lowerBound) ? edge : fabs(lowerBound)/(6-a);
        upperBound += edge;
        lowerBound -= edge;
        if(edge < 0 || (upperBound <= 0.1f+0.02f*a && lowerBound >= -0.1f)) {
            yAxes[a]->setRange(-0.1f , +0.1f+0.02f*a);
        } else if(upperBound > 0.1f+0.02f*a && lowerBound >= -0.1f) {
            yAxes[a]->setRange(-0.1f , upperBound);
        } else if(upperBound <= 0.1f+0.02f*a && lowerBound < -0.1f) {
            yAxes[a]->setRange(lowerBound , 0.1f+0.02f*a);
        } else {
            yAxes[a]->setRange(lowerBound, upperBound);
        }
    }

    if(minKey > maxKey) {
        minKey = 0;
        maxKey = 1;
    }
    edge = (ui->plotter->xAxis->range().upper-ui->plotter->xAxis->range().lower)/3.;
    ui->plotter->axisRect()->xLowerBoundary = minKey-edge;
    ui->plotter->axisRect()->xUpperBoundary = maxKey+edge;
}

void graphWidget::MousePressedPlotter()
{
    //ui->plotter->setRangeDrag(ui->plotter->xAxis->orientation());
}

void graphWidget::MouseWheelPlotter()
{
    /*if(ui->plotter->yAxis->selected().testFlag(QCPAxis::spAxis))
    {
        ui->plotter->setRangeZoom(ui->plotter->yAxis->orientation());
    }
    else
    {
        ui->plotter->setRangeZoom(ui->plotter->xAxis->orientation());
    }*/
    setPlotRanges();
}

void graphWidget::drawGraph(int index)
{
    if(pGraphList[index]->mType == povPos) {
        if(pGraphList[index]->treeItem->checkState(1) == Qt::Unchecked || !glView->povMode) {
            ui->povLine->hide();
            return;
        }
        if(drawPoVLine()) {
            ui->plotter->replot();
            setPlotRanges();
        }
        pGraphList[index]->drawn = true;
        return;
    }

    pGraphList[index]->drawn = true;

    if(selTrack->trackData->activeSection) {
        pGraphList[index]->fillGraphList(ui->plotter->xAxis, selTrack->trackData->activeSection->bArgument, selTrack->trackData->activeSection->bOrientation, pGraphList[secBoundaries]->drawn);
    } else {
        pGraphList[index]->fillGraphList(ui->plotter->xAxis, TIME, QUATERNION, pGraphList[secBoundaries]->drawn);
    }

    for(int i = 0; i < pGraphList[index]->graphList.size(); ++i) {
        ui->plotter->addPlottable(pGraphList[index]->graphList[i]);
        subfunc* temp = (subfunc*)pGraphList[index]->graphList[i]->property("p").value<void*>();
        if(temp && temp == ui->transitionEditor->getSelectedFunc()) {
            pGraphList[index]->graphList[i]->setSelected(true);
        }
    }
}

bool graphWidget::drawPoVLine()
{
    if(ui->povLine->isHidden()) ui->povLine->show();
    bool changed = false;
    double coord;
    if(selTrack->trackData->activeSection && selTrack->trackData->activeSection->bArgument == DISTANCE) {
         mnode* temp = selTrack->trackData->getPoint(gloParent->getPovPos());
         if(temp) {
            coord = temp->fTotalLength;
         } else {
             return false;
         }
    } else {
        coord = gloParent->getPovPos()/F_HZ;
    }
    double diff = coord - ui->plotter->xAxis->range().lower*0.8 - ui->plotter->xAxis->range().upper*0.2;
    if(diff < 0 && coord > 0) {
        ui->plotter->xAxis->setRange(ui->plotter->xAxis->range().lower+diff, ui->plotter->xAxis->range().upper+diff);
        changed = true;
    }
    diff = ui->plotter->xAxis->range().upper*0.8 + ui->plotter->xAxis->range().lower*0.2 - coord;
    if(diff < 0) {
        ui->plotter->xAxis->setRange(ui->plotter->xAxis->range().lower-diff, ui->plotter->xAxis->range().upper-diff);
        changed = true;
    }
    int x = ui->plotter->xAxis->coordToPixel(coord);
    ui->povLine->setGeometry(x, 0, 4, ui->plotter->height());
    return changed;
}

void graphWidget::undrawGraph(int index)
{
    pGraphList[index]->drawn = false;
    if(pGraphList[index]->mType == povPos) {
        ui->povLine->hide();
        return;
    }
    for(int i = 0; i < pGraphList[index]->graphList.size(); ++i) {
        ui->plotter->removeGraph(pGraphList[index]->graphList[i]);
    }
    while(pGraphList[index]->graphList.size()) pGraphList[index]->graphList.removeAt(0);
}

void graphWidget::curSectionChanged(sectionHandler *_section)
{
    deactivateGraph(pGraphList[rollSpeed]);
    deactivateGraph(pGraphList[nForce]);
    deactivateGraph(pGraphList[lForce]);
    deactivateGraph(pGraphList[pitchChange]);
    deactivateGraph(pGraphList[yawChange]);

    switch(_section->type) {
    case anchor:
    case bezier:
    case nolimitscsv:
        return;
    case curved:
    case straight:
        activateGraph(pGraphList[rollSpeed]);
        break;
    case forced:
        activateGraph(pGraphList[rollSpeed]);
        activateGraph(pGraphList[nForce]);
        activateGraph(pGraphList[lForce]);
        break;
    case geometric:
        activateGraph(pGraphList[rollSpeed]);
        activateGraph(pGraphList[pitchChange]);
        activateGraph(pGraphList[yawChange]);
        break;
    default:
        lenAssert(0 && "unknown section type");
        break;
    }
    ui->transitionEditor->adjustLengthSteps(_section->sectionData->type, _section->sectionData->bArgument);
    ui->tabWidget->removeTab(1);
    ui->tabWidget->setCurrentWidget(ui->graphChooser);
    ui->transitionEditor->changeSubfunc(NULL);
    selFunc = NULL;
}

void graphWidget::activateGraph(graphHandler* curGraph)
{
    if(curGraph->active) {
        return;
    }

    curGraph->treeItem->parent()->removeChild(curGraph->treeItem);
    ui->selTree->topLevelItem(0)->addChild(curGraph->treeItem);
    curGraph->active = true;
    curGraph->prevCheckState = curGraph->treeItem->checkState(1);
    curGraph->treeItem->setCheckState(1, Qt::Checked);
}

void graphWidget::deactivateGraph(graphHandler *curGraph)
{
    if(!curGraph->active) {
        return;
    }

    curGraph->treeItem->parent()->removeChild(curGraph->treeItem);
    int a = 0, b = 0;

    switch(curGraph->mType) {
    case rollSpeed:
        a = 0;
        b = 1;
        break;
    case nForce:
        a = 1;
        b = 0;
        break;
    case lForce:
        a = 2;
        b = 0;
        break;
    case pitchChange:
        a = 3;
        b = 0;
        break;
    case yawChange:
        a = 3;
        b = 1;
        break;
    default:
        lenAssert(0 && "unhandeled graph type");
        break;
    }
    ui->selTree->topLevelItem(1)->child(a)->insertChild(b, curGraph->treeItem);

    curGraph->active = false;
    curGraph->treeItem->setCheckState(1, curGraph->prevCheckState);
}

void graphWidget::selectionChanged()
{
    QList<QCPGraph*> selected = ui->plotter->selectedGraphs();
    for(int i = 0; i<4; ++i) {
        yAxes[i]->grid()->setVisible(false);
    }

    if(selected.size()) {
        subfunc* temp = (subfunc*)selected[0]->property("p").value<void*>();
        ui->transitionEditor->changeSubfunc(temp);
        selFunc = temp;
        ui->tabWidget->addTab(ui->transitionEditor, "Transition Editor");
        ui->tabWidget->setCurrentWidget(ui->transitionEditor);
        selected[0]->valueAxis()->grid()->setVisible(true);
    } else {
        ui->tabWidget->removeTab(1);
        ui->tabWidget->setCurrentWidget(ui->graphChooser);
        ui->transitionEditor->changeSubfunc(NULL);
        selFunc = NULL;
    }
    this->selTrack->trackData->hasChanged = true;
    redrawGraphs();
}

bool graphWidget::changeSelection(subfunc* _sel)
{
    ui->transitionEditor->changeSubfunc(_sel);
    selFunc = _sel;
    ui->plotter->deselectAll();
    for(int i = 0; i < ui->plotter->graphCount(); ++i) {
        if((subfunc*)ui->plotter->graph(i)->property("p").value<void*>() == _sel) {
            ui->plotter->graph(i)->setSelected(true);
            return true;
        }
    }
    return false;
}

void graphWidget::redrawGraphs(bool otherArgument)
{
    for(int i = 0; i < pGraphList.size(); ++i) {
        if(pGraphList[i]->drawn) {
            if(i%2 && (i-1)/2 >= 0 && (i-1)/2 < 3) {
                undrawGraph(11+(i-1)/2);
                if(selTrack->trackData->smoother && selTrack->trackData->smoother->active()) {
                    drawGraph(11+(i-1)/2);
                }
            }
            undrawGraph(i);
            drawGraph(i);
        }
    }
    if(otherArgument) {
        double rLower = ui->plotter->xAxis->range().lower;
        double rUpper = ui->plotter->xAxis->range().upper;

        track* curTrack = selTrack->trackData;

        int maxPoints = curTrack->getNumPoints();

        if(curTrack->activeSection->bArgument == TIME) {
            rLower = curTrack->getIndexFromDist(rLower)/1000.;
            rUpper = curTrack->getIndexFromDist(rUpper)/1000.;

            double edge = (rUpper-rLower)/3.;
            rUpper += edge;
            rLower -= edge;

            ui->plotter->xAxis->setRange(rLower, rUpper);
        } else { // DISTANCE
            rLower *= 1000.;
            rUpper *= 1000.;

            lenAssert(rLower < maxPoints);

            if(rLower <= 0.) {
                rUpper = curTrack->getPoint(maxPoints <= rUpper ? maxPoints : rUpper)->fTotalLength;
                rLower = 0.;
            } else {
                rUpper = curTrack->getPoint(maxPoints <= rUpper ? maxPoints : rUpper)->fTotalLength;
                rLower = curTrack->getPoint(rLower)->fTotalLength;
            }

            double edge = (rUpper-rLower)/3.;
            rUpper += edge;
            rLower -= edge;

            ui->plotter->xAxis->setRange(rLower, rUpper);
        }
    }
    setPlotRanges();
    if(editor) editor->refreshMarkers();
    updateBookmarkItems();
    ui->plotter->replot();
    if(timeline) timeline->refresh();
}

void graphWidget::on_plotter_customContextMenuRequested(const QPoint &pos)
{
    subfunc* selFunc = ui->transitionEditor->getSelectedFunc();
    if(selFunc == NULL) return;

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction("Append Transition", ui->transitionEditor, SLOT(on_appendButton_released()));
    menu->addAction("Prepend Transition", ui->transitionEditor, SLOT(on_prependButton_released()));
    if(selFunc->parent->funcList.size() > 1) {
        menu->addAction("Remove Transition", ui->transitionEditor, SLOT(on_removeButton_released()));
    }

    menu->popup(ui->plotter->mapToGlobal(pos));
}

void graphWidget::keyPressEvent(QKeyEvent* event) {
    subfunc* oldFunc = selFunc;
    int i;
    bool ok = false;
    switch(event->key()) {
    case Qt::Key_Delete:
        if(selFunc && selFunc->parent->funcList.size() > 1) {
            ui->transitionEditor->on_removeButton_released();
        }
        break;
    case Qt::Key_Right:
        if(selFunc) {
            if((i = selFunc->parent->getSubfuncNumber(selFunc)) == selFunc->parent->funcList.size()-1) {
                ui->transitionEditor->on_appendButton_released();
            } else {
                changeSelection(selFunc->parent->funcList[i+1]);
                redrawGraphs();
            }
        }
        break;
    case Qt::Key_Left:
        if(selFunc) {
            if((i = selFunc->parent->getSubfuncNumber(selFunc)) == 0) {
                ui->transitionEditor->on_prependButton_released();
            } else {
                changeSelection(selFunc->parent->funcList[i-1]);
                redrawGraphs();
            }
        }
        break;
    case Qt::Key_Up:
        if(selFunc) {
            if(selFunc->parent->secParent->type == forced) {
                i = selFunc->parent->getSubfuncNumber(selFunc);
                if(selFunc->parent->type == funcRoll) {
                    ok = changeSelection(i < selFunc->parent->secParent->normForce->funcList.size() ? selFunc->parent->secParent->normForce->funcList[i] : selFunc->parent->secParent->normForce->funcList.last());
                    if(!ok) ok = changeSelection(i < selFunc->parent->secParent->latForce->funcList.size() ? selFunc->parent->secParent->latForce->funcList[i] : selFunc->parent->secParent->latForce->funcList.last());
                } else if(selFunc->parent->type == funcNormal) {
                    ok = changeSelection(i < selFunc->parent->secParent->latForce->funcList.size() ? selFunc->parent->secParent->latForce->funcList[i] : selFunc->parent->secParent->latForce->funcList.last());
                    if(!ok) ok = changeSelection(i < selFunc->parent->secParent->rollFunc->funcList.size() ? selFunc->parent->secParent->rollFunc->funcList[i] : selFunc->parent->secParent->rollFunc->funcList.last());
                } else {
                    ok = changeSelection(i < selFunc->parent->secParent->rollFunc->funcList.size() ? selFunc->parent->secParent->rollFunc->funcList[i] : selFunc->parent->secParent->rollFunc->funcList.last());
                    if(!ok) ok = changeSelection(i < selFunc->parent->secParent->normForce->funcList.size() ? selFunc->parent->secParent->normForce->funcList[i] : selFunc->parent->secParent->normForce->funcList.last());
                }
                if(!ok) changeSelection(oldFunc);
                redrawGraphs();
            } else if(selFunc->parent->secParent->type == geometric) {
                i = selFunc->parent->getSubfuncNumber(selFunc);
                if(selFunc->parent->type == funcRoll) {
                    ok = changeSelection(i < selFunc->parent->secParent->normForce->funcList.size() ? selFunc->parent->secParent->normForce->funcList[i] : selFunc->parent->secParent->normForce->funcList.last());
                    if(!ok) ok = changeSelection(i < selFunc->parent->secParent->latForce->funcList.size() ? selFunc->parent->secParent->latForce->funcList[i] : selFunc->parent->secParent->latForce->funcList.last());
                } else if(selFunc->parent->type == funcPitch) {
                    ok = changeSelection(i < selFunc->parent->secParent->latForce->funcList.size() ? selFunc->parent->secParent->latForce->funcList[i] : selFunc->parent->secParent->latForce->funcList.last());
                    if(!ok) ok = changeSelection(i < selFunc->parent->secParent->rollFunc->funcList.size() ? selFunc->parent->secParent->rollFunc->funcList[i] : selFunc->parent->secParent->rollFunc->funcList.last());
                } else {
                    ok = changeSelection(i < selFunc->parent->secParent->rollFunc->funcList.size() ? selFunc->parent->secParent->rollFunc->funcList[i] : selFunc->parent->secParent->rollFunc->funcList.last());
                    if(!ok) ok = changeSelection(i < selFunc->parent->secParent->normForce->funcList.size() ? selFunc->parent->secParent->normForce->funcList[i] : selFunc->parent->secParent->normForce->funcList.last());
                }
                if(!ok) changeSelection(oldFunc);
                redrawGraphs();
            }
        }
        break;
    case Qt::Key_Down:
        if(selFunc)
        {
            if(selFunc->parent->secParent->type == forced) {
                i = selFunc->parent->getSubfuncNumber(selFunc);
                if(selFunc->parent->type == funcRoll) {
                    ok = changeSelection(i < selFunc->parent->secParent->latForce->funcList.size() ? selFunc->parent->secParent->latForce->funcList[i] : selFunc->parent->secParent->latForce->funcList.last());
                    if(!ok) ok = changeSelection(i < selFunc->parent->secParent->normForce->funcList.size() ? selFunc->parent->secParent->normForce->funcList[i] : selFunc->parent->secParent->normForce->funcList.last());
                } else if(selFunc->parent->type == funcNormal) {
                    ok = changeSelection(i < selFunc->parent->secParent->rollFunc->funcList.size() ? selFunc->parent->secParent->rollFunc->funcList[i] : selFunc->parent->secParent->rollFunc->funcList.last());
                    if(!ok) ok = changeSelection(i < selFunc->parent->secParent->latForce->funcList.size() ? selFunc->parent->secParent->latForce->funcList[i] : selFunc->parent->secParent->latForce->funcList.last());
                } else {
                    ok = changeSelection(i < selFunc->parent->secParent->normForce->funcList.size() ? selFunc->parent->secParent->normForce->funcList[i] : selFunc->parent->secParent->normForce->funcList.last());
                    if(!ok) ok = changeSelection(i < selFunc->parent->secParent->rollFunc->funcList.size() ? selFunc->parent->secParent->rollFunc->funcList[i] : selFunc->parent->secParent->rollFunc->funcList.last());
                }
                if(!ok) changeSelection(oldFunc);
                redrawGraphs();
            } else if(selFunc->parent->secParent->type == geometric) {
                i = selFunc->parent->getSubfuncNumber(selFunc);
                if(selFunc->parent->type == funcRoll) {
                    ok = changeSelection(i < selFunc->parent->secParent->latForce->funcList.size() ? selFunc->parent->secParent->latForce->funcList[i] : selFunc->parent->secParent->latForce->funcList.last());
                    if(!ok) ok = changeSelection(i < selFunc->parent->secParent->normForce->funcList.size() ? selFunc->parent->secParent->normForce->funcList[i] : selFunc->parent->secParent->normForce->funcList.last());
                } else if(selFunc->parent->type == funcPitch) {
                    ok = changeSelection(i < selFunc->parent->secParent->rollFunc->funcList.size() ? selFunc->parent->secParent->rollFunc->funcList[i] : selFunc->parent->secParent->rollFunc->funcList.last());
                    if(!ok) ok = changeSelection(i < selFunc->parent->secParent->latForce->funcList.size() ? selFunc->parent->secParent->latForce->funcList[i] : selFunc->parent->secParent->latForce->funcList.last());
                } else {
                    ok = changeSelection(i < selFunc->parent->secParent->normForce->funcList.size() ? selFunc->parent->secParent->normForce->funcList[i] : selFunc->parent->secParent->normForce->funcList.last());
                    if(!ok) ok = changeSelection(i < selFunc->parent->secParent->rollFunc->funcList.size() ? selFunc->parent->secParent->rollFunc->funcList[i] : selFunc->parent->secParent->rollFunc->funcList.last());
                }
                if(!ok) changeSelection(oldFunc);
                redrawGraphs();
            }
        }
        break;
    default:
        event->ignore();
        break;
    }
    event->accept();
}

void graphWidget::setBezPoints()
{
    if(!selFunc || selFunc->degree != freeform) {
        if(bezPoints.size()) {
            for(int i = 0; i < bezPoints.size(); ++i) {
                delete bezPoints[i];
            }
            bezPoints.clear();
        }
        return;
    }
    if(selFunc && selFunc->degree == freeform) {
        QCPAxis* yAxis;
        switch(selFunc->parent->type) {
        case funcRoll:
            yAxis = yAxes[0];
            break;
        case funcNormal:
        case funcLateral:
            yAxis = yAxes[1];
            break;
        case funcPitch:
            yAxis = yAxes[2];
            break;
        case funcYaw:
            yAxis = yAxes[3];
            break;
        }

        float until;
        if(selFunc->parent->secParent->bArgument == TIME) {
            until = selTrack->trackData->getNumPoints(selFunc->parent->secParent)/F_HZ;
        } else {
			until = selFunc->parent->secParent->lNodes.first().fTotalHeartLength;
        }
        int x1 = ui->plotter->xAxis->coordToPixel(selFunc->minArgument+until);
        int x2 = ui->plotter->xAxis->coordToPixel(selFunc->maxArgument+until);
        int y1 = yAxis->coordToPixel(selFunc->startValue);
        int y2 = yAxis->coordToPixel(selFunc->startValue+selFunc->symArg);
        if(selFunc->pointList.size() != bezPoints.size()) {
            for(int i = 0; i < bezPoints.size(); ++i) {
                delete bezPoints[i];
            }
            bezPoints.clear();
            for(int i = 0; i < selFunc->pointList.size(); ++i) {
                bezPoints.append(new dragLabel(ui->plotter));
                bezPoints[i]->setText("x");
                bezPoints[i]->setGeometry(QRect(0, 0, 12, 12));
                bezPoints[i]->setAlignment(Qt::AlignCenter);
                bezPoints[i]->show();
            }
        }
        for(int i = 0; i < selFunc->pointList.size(); ++i) {
            if(bezPoints[i]->isDragged) {
                selFunc->pointList[i].x = (ui->plotter->xAxis->pixelToCoord(bezPoints[i]->pos().x()+6)-selFunc->minArgument-until)/(selFunc->maxArgument-selFunc->minArgument);
                selFunc->pointList[i].y = (yAxis->pixelToCoord(bezPoints[i]->pos().y()+6)-selFunc->startValue)/(selFunc->symArg);
                selFunc->updateBez();
                selTrack->trackData->updateTrack(selTrack->trackData->activeSection, (int)(selFunc->minArgument*F_HZ-1.5f));
            } else {
                int x = x1*(1-selFunc->pointList[i].x) + x2*selFunc->pointList[i].x-6;
                int y = y1*(1-selFunc->pointList[i].y) + y2*selFunc->pointList[i].y-6;
                bezPoints[i]->move(x, y);
            }
        }
        redrawGraphs();
    }
    return;
}
