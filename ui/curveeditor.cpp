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

#include "curveeditor.h"
#include "graphwidget.h"
#include "mainwindow.h"
#include "trackhandler.h"
#include "track.h"
#include "section.h"
#include "function.h"
#include "subfunction.h"
#include "mnode.h"
#include "theme.h"
#include "optionsmenu.h"
#include "qcustomplot.h"
#include "undohandler.h"
#include "undoaction.h"
#include "funcsnapshot.h"
#include "transitionwidget.h"

#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QToolTip>
#include <QSettings>
#include <QDateTime>
#include <cmath>
#include <cstdlib>

extern MainWindow* gloParent;

// ---------------------------------------------------------------------------
//  small helpers
// ---------------------------------------------------------------------------

static int gKeepFormula = -1;   // -1 = not loaded yet

bool curveEditor::keepFormula()
{
    if(gKeepFormula < 0) gKeepFormula = QSettings("FVD++", "FVD").value("editor/keepFormula", false).toBool() ? 1 : 0;
    return gKeepFormula == 1;
}

void curveEditor::setKeepFormula(bool on)
{
    gKeepFormula = on ? 1 : 0;
    QSettings("FVD++", "FVD").setValue("editor/keepFormula", on);
}

// value of the transition's shape at arg with start 0 and amplitude 1
static float unitShape(subfunc* a, double arg)
{
    float sym = a->symArg, start = a->startValue;
    a->symArg = 1.f;
    a->startValue = 0.f;
    float v = a->getValue((float)arg);
    a->symArg = sym;
    a->startValue = start;
    return v;
}

// position (0..1) of the top of a hill for a given center argument, and the inverse
static double peakFromCenter(double c)
{
    if(c > 0.) return std::pow(0.5, 1./std::pow(2., c/2.));
    if(c < 0.) return 1. - std::pow(0.5, 1./std::pow(2., -c/2.));
    return 0.5;
}

static double centerFromPeak(double u)
{
    if(u > 0.5) return 2.*std::log(std::log(0.5)/std::log(u))/std::log(2.);
    if(u < 0.5) return -2.*std::log(std::log(0.5)/std::log(1.-u))/std::log(2.);
    return 0.;
}

static bool isUnit(func* f, bool force) { return force == (f->type == funcNormal || f->type == funcLateral); }

// ---------------------------------------------------------------------------

curveEditor::curveEditor(graphWidget* owner)
    : QObject(owner), mOwner(owner), mMarkers(NULL), mHorizontalOnly(NULL), mHover(NULL),
      mMode(None), mStarted(false), mFunc(NULL), mIndex(-1), mPressArg(0.),
      mOrigArg(0.), mOrigVal(0.), mNextMax(0.), mOrigCenter(0.), mOrigSym(0.), mOrigSectionEnd(0.), mLastApply(0)
{
    mOwner->plot()->setMouseTracking(true);
    mOwner->plot()->installEventFilter(this);
}

QList<curveEditor::target> curveEditor::targets()
{
    QList<target> list;
    QList<QPair<func*, QCPAxis*> > drawn = mOwner->drawnEditableFunctions();
    for(int i = 0; i < drawn.size(); ++i) {
        target t;
        t.f = drawn[i].first;
        t.axis = drawn[i].second;
        list.append(t);
    }
    return list;
}

func* curveEditor::selectedFunc()
{
    subfunc* s = mOwner->selFunc;
    if(!s || !mOwner->selTrack) return NULL;
    section* sec = mOwner->selTrack->trackData->activeSection;
    if(!sec || s->parent->secParent != sec) return NULL;
    if(sec->type != forced && sec->type != geometric) return NULL;
    return s->parent;
}

QCPAxis* curveEditor::axisFor(func* f)
{
    switch(f->type) {
    case funcRoll:    return mOwner->yAxes[0];
    case funcNormal:
    case funcLateral: return mOwner->yAxes[1];
    case funcPitch:   return mOwner->yAxes[2];
    case funcYaw:     return mOwner->yAxes[3];
    }
    return mOwner->yAxes[1];
}

double curveEditor::sectionStart()
{
    track* t = mOwner->selTrack->trackData;
    if(t->activeSection->bArgument == DISTANCE) return t->activeSection->lNodes[0].fTotalLength;
    return t->getNumPoints(t->activeSection)/F_HZ;
}

double curveEditor::minLength() const
{
    return mOwner->selTrack->trackData->activeSection->bArgument == DISTANCE ? 0.5 : 0.05;
}

double curveEditor::niceStep(QCPAxis* axis, double minStep)
{
    // smallest "nice" step that is at least 6 px wide at the current zoom, but never below minStep
    static const double steps[] = { 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.25, 0.5, 1, 2, 5, 10, 20, 50, 100 };
    double perPixel = std::fabs(axis->pixelToCoord(1) - axis->pixelToCoord(0));
    for(unsigned i = 0; i < sizeof(steps)/sizeof(steps[0]); ++i) {
        if(steps[i] >= minStep - 1e-9 && steps[i] >= 6*perPixel) return steps[i];
    }
    return 100;
}

double curveEditor::snapValue(func* f, QCPAxis* axis, double v, bool free)
{
    if(free) return v;
    double step = niceStep(axis, isUnit(f, true) ? 0.05 : 0.5);
    return std::floor(v/step + 0.5)*step;
}

double curveEditor::snapArg(double a, bool free)
{
    if(free) return a;
    bool byDistance = mOwner->selTrack->trackData->activeSection->bArgument == DISTANCE;
    double step = niceStep(mOwner->plot()->xAxis, byDistance ? 0.5 : 0.05);
    return std::floor(a/step + 0.5)*step;
}

float curveEditor::valueAt(func* f, double arg)
{
    if(f->funcList.isEmpty()) return 0.f;
    if(arg < 0.) arg = 0.;
    for(int i = 0; i < f->funcList.size(); ++i) {
        subfunc* s = f->funcList[i];
        if(s->locked) s->getValue(-1.f);
        if(arg <= s->maxArgument || i == f->funcList.size()-1) {
            double a = arg < s->minArgument ? s->minArgument : (arg > s->maxArgument ? s->maxArgument : arg);
            return s->getValue((float)a);
        }
    }
    return 0.f;
}

bool curveEditor::vertexMovesVertically(func* f, int vertex)
{
    subfunc* a = f->funcList[vertex-1];
    // a hill ends where it started, "to zero" always ends at 0 - their end can only move sideways
    return !a->isSymmetric() && a->degree != tozero;
}

// ---------------------------------------------------------------------------
//  hit test
// ---------------------------------------------------------------------------

curveEditor::hit curveEditor::hitTest(const QPoint& pixel)
{
    hit h;
    if(!mOwner->selTrack) return h;
    section* sec = mOwner->selTrack->trackData->activeSection;
    if(!sec || (sec->type != forced && sec->type != geometric)) return h;
    QCustomPlot* plot = mOwner->plot();
    const double n0 = sectionStart();

    // 1) points of the selected function
    func* sf = selectedFunc();
    if(sf) {
        QCPAxis* ax = axisFor(sf);
        double best = 9.;
        for(int k = 1; k <= sf->funcList.size(); ++k) {
            subfunc* a = sf->funcList[k-1];
            if(a->locked) a->getValue(-1.f);
            double px = plot->xAxis->coordToPixel(n0 + a->maxArgument);
            double py = ax->coordToPixel(a->endValue());
            double d = std::sqrt((px-pixel.x())*(px-pixel.x()) + (py-pixel.y())*(py-pixel.y()));
            if(d < best) { best = d; h.mode = Vertex; h.f = sf; h.index = k; h.arg = a->maxArgument; }
        }
        if(h.mode == Vertex) return h;
    }

    // 2) anywhere on an editable line
    double arg = plot->xAxis->pixelToCoord(pixel.x()) - n0;
    QList<target> ts = targets();
    double best = 7.;
    for(int t = 0; t < ts.size(); ++t) {
        func* f = ts[t].f;
        if(arg < 0. || arg > f->getMaxArgument()) continue;
        double py = ts[t].axis->coordToPixel(valueAt(f, arg));
        double d = std::fabs(py - pixel.y());
        if(d < best) {
            best = d;
            int i = 0;
            while(i < f->funcList.size()-1 && arg > f->funcList[i]->maxArgument) ++i;
            h.mode = Body; h.f = f; h.index = i; h.arg = arg;
        }
    }
    return h;
}

// ---------------------------------------------------------------------------
//  markers
// ---------------------------------------------------------------------------

void curveEditor::refreshMarkers()
{
    QCustomPlot* plot = mOwner->plot();
    if(mMarkers) { plot->removeGraph(mMarkers); mMarkers = NULL; }
    if(mHorizontalOnly) { plot->removeGraph(mHorizontalOnly); mHorizontalOnly = NULL; }
    if(mHover) { plot->removeGraph(mHover); mHover = NULL; }

    func* f = selectedFunc();
    QColor border = theme::isDark() ? QColor(240, 240, 240) : QColor(30, 30, 30);
    QCPAxis* ax = f ? axisFor(f) : mOwner->yAxes[1];

    mHover = new QCPGraph(plot->xAxis, ax);
    plot->addPlottable(mHover);
    mHover->setLineStyle(QCPGraph::lsNone);
    mHover->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(border, 2), QBrush(Qt::NoBrush), 13));
    mHover->setSelectable(false);

    if(!f) return;
    QColor fill = gloParent->mOptions->rollColor[2];
    switch(f->type) {
    case funcNormal:  fill = gloParent->mOptions->normColor[2]; break;
    case funcLateral: fill = gloParent->mOptions->latColor[2]; break;
    case funcPitch:   fill = gloParent->mOptions->pitchColor[2]; break;
    case funcYaw:     fill = gloParent->mOptions->yawColor[2]; break;
    default: break;
    }
    fill.setAlpha(255);

    QVector<double> x, y, hx, hy;
    const double n0 = sectionStart();
    for(int k = 1; k <= f->funcList.size(); ++k) {
        subfunc* a = f->funcList[k-1];
        if(a->locked) a->getValue(-1.f);
        if(vertexMovesVertically(f, k)) { x.append(n0 + a->maxArgument); y.append(a->endValue()); }
        else { hx.append(n0 + a->maxArgument); hy.append(a->endValue()); }
    }
    mMarkers = new QCPGraph(plot->xAxis, ax);
    plot->addPlottable(mMarkers);
    mMarkers->setLineStyle(QCPGraph::lsNone);
    mMarkers->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(border, 1.5), QBrush(fill), 10));
    mMarkers->setSelectable(false);
    mMarkers->setData(x, y);

    mHorizontalOnly = new QCPGraph(plot->xAxis, ax);
    plot->addPlottable(mHorizontalOnly);
    mHorizontalOnly->setLineStyle(QCPGraph::lsNone);
    mHorizontalOnly->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDiamond, QPen(border, 1.5), QBrush(fill.lighter(140)), 10));
    mHorizontalOnly->setSelectable(false);
    mHorizontalOnly->setData(hx, hy);

    // keep the hover ring on the selected function's axis
    mHover->setValueAxis(ax);
}

// ---------------------------------------------------------------------------
//  dragging
// ---------------------------------------------------------------------------

void curveEditor::beginDrag(const hit& h, const QPoint& pixel)
{
    section* sec = mOwner->selTrack->trackData->activeSection;
    mMode = h.mode;
    mStarted = false;
    mFunc = h.f;
    mIndex = h.index;
    mPressArg = h.arg;
    mPressPos = pixel;
    mLastApply = 0;
    mBefore = funcsnapshot::save(sec);
    mOrigSectionEnd = sec->getMaxArgument();
    mOrigEnds.clear();
    func* fs[3] = { sec->rollFunc, sec->normForce, sec->latForce };
    for(int k = 0; k < 3; ++k) mOrigEnds.append(fs[k] ? fs[k]->getMaxArgument() : 0.f);

    QCustomPlot* plot = mOwner->plot();
    QCPAxis* ax = axisFor(h.f);
    const double n0 = sectionStart();
    subfunc* s = h.mode == Vertex ? h.f->funcList[h.index-1] : h.f->funcList[h.index];
    mOwner->selectTransition(s);

    if(h.mode == Vertex) {
        mOrigArg = s->maxArgument;
        mOrigVal = s->endValue();
        mNextMax = h.index < h.f->funcList.size() ? h.f->funcList[h.index]->maxArgument : 0.;
        QPoint centre((int)std::floor(plot->xAxis->coordToPixel(n0 + mOrigArg) + 0.5),
                      (int)std::floor(ax->coordToPixel(mOrigVal) + 0.5));
        mGrabOffset = centre - pixel;
    } else {
        mOrigArg = h.arg;
        mOrigVal = valueAt(h.f, h.arg);
        mOrigCenter = s->centerArg;
        mOrigSym = s->symArg;
        mGrabOffset = QPoint(0, (int)std::floor(ax->coordToPixel(mOrigVal) + 0.5) - pixel.y());
    }
    plot->setCursor(Qt::ClosedHandCursor);
}

bool curveEditor::splitAt(func* f, int i, double arg)
{
    subfunc* a = f->funcList[i];
    const double minLen = minLength();
    if(a->degree == tozero) return false;
    if(arg - a->minArgument < minLen || a->maxArgument - arg < minLen) return false;

    const float y0 = a->getValue((float)arg);
    const float start = a->startValue;
    const float endV = a->endValue();
    const bool sym = a->isSymmetric();
    const eDegree d = sym ? cubic : a->degree;
    const float arg1 = a->arg1;
    const float len1 = (float)(arg - a->minArgument), len2 = (float)(a->maxArgument - arg);
    const bool wasLocked = a->locked;

    f->appendSubFunction(len2, i);      // new transition after a, following ones move back by len2
    f->changeLength(len1, i);           // a ends at the new point, following ones move forward again
    subfunc* a1 = f->funcList[i];
    subfunc* a2 = f->funcList[i+1];

    a1->changeDegree(d);
    a2->changeDegree(d);
    if(!sym) { a1->arg1 = arg1; a2->arg1 = arg1; }
    a1->centerArg = a1->tensionArg = 0.f;
    a2->centerArg = a2->tensionArg = 0.f;
    a1->locked = false;
    a2->locked = wasLocked;
    a1->symArg = y0 - start;
    a2->symArg = endV - y0;
    f->translateValues(a1);
    return true;
}

void curveEditor::dragTo(const QPoint& pixel, bool free)
{
    if(!mStarted) {
        if(std::abs(pixel.x() - mPressPos.x()) + std::abs(pixel.y() - mPressPos.y()) < 3) return;
        mStarted = true;
        if(mMode == Body && !keepFormula()) {
            // free mode: a new point where the line was grabbed, then it's dragged like any point
            if(!splitAt(mFunc, mIndex, mPressArg)) {
                showTip(pixel, tr("Too close to a point - drag the point instead"));
                mMode = None;
                return;
            }
            mIndex = mIndex + 1;
            subfunc* a = mFunc->funcList[mIndex-1];
            mOrigArg = a->maxArgument;
            mOrigVal = a->endValue();
            mNextMax = mFunc->funcList[mIndex]->maxArgument;
            mGrabOffset = QPoint(0, mGrabOffset.y());
            mMode = Vertex;
            mOwner->changeSelection(a);
        }
    }

    const QPoint target = pixel + mGrabOffset;
    const bool moveX = std::abs(pixel.x() - mPressPos.x()) >= 3;
    const bool moveY = std::abs(pixel.y() - mPressPos.y()) >= 3;
    if(mMode == Vertex) dragVertex(target, moveX, moveY, free);
    else if(mMode == Body) dragBodyKeepFormula(target, moveX, moveY, free);
}

void curveEditor::dragVertex(const QPoint& target, bool moveX, bool moveY, bool free)
{
    func* f = mFunc;
    if(!f || mIndex < 1 || mIndex > f->funcList.size()) return;
    track* t = mOwner->selTrack->trackData;
    section* sec = t->activeSection;
    QCustomPlot* plot = mOwner->plot();
    QCPAxis* ax = axisFor(f);
    const int i = mIndex - 1;
    subfunc* a = f->funcList[i];
    subfunc* b = (i+1 < f->funcList.size()) ? f->funcList[i+1] : NULL;
    const bool lastPoint = (b == NULL);
    const bool vertical = vertexMovesVertically(f, mIndex);
    const double n0 = sectionStart();
    const double minLen = minLength();

    double arg = mOrigArg, val = mOrigVal;
    if(moveX) arg = snapArg(plot->xAxis->pixelToCoord(target.x()) - n0, free);
    if(moveY && vertical) val = snapValue(f, ax, ax->pixelToCoord(target.y()), free);

    // ---- sideways: move the boundary, the following point keeps its time ----
    if(!a->locked) {
        double lo = a->minArgument + minLen;
        double hi = (b && !b->locked) ? mNextMax - minLen : 1e9;
        QList<subfunc*> followers;
        if(lastPoint) {
            // the last point is the end of the section: functions that ended with it follow
            func* fs[3] = { sec->rollFunc, sec->normForce, sec->latForce };
            for(int k = 0; k < 3; ++k) {
                func* o = fs[k];
                if(!o || o == f || o->funcList.isEmpty()) continue;
                subfunc* last = o->funcList.last();
                if(last->locked) continue;
                if(std::fabs(mOrigEnds[k] - mOrigSectionEnd) > 1e-3) continue;
                followers.append(last);
                if(last->minArgument + minLen > lo) lo = last->minArgument + minLen;
            }
        }
        if(arg < lo) arg = lo;
        if(arg > hi) arg = hi;
        if(std::fabs(arg - a->maxArgument) > 1e-6) {
            f->changeLength((float)(arg - a->minArgument), i);
            if(b && !b->locked) f->changeLength((float)(mNextMax - b->minArgument), i+1);
            for(int k = 0; k < followers.size(); ++k) {
                subfunc* last = followers[k];
                last->parent->changeLength((float)(arg - last->minArgument), last->parent->funcList.size()-1);
            }
        }
    }

    // ---- up / down: the point's value; the next point (end of the next ramp) stays put ----
    float dE = (float)(val - a->endValue());
    if(vertical && std::fabs(dE) > 1e-7) {
        a->symArg += dE;
        for(int j = i+1; j < f->funcList.size(); ++j) {
            subfunc* c = f->funcList[j];
            if(c->degree == tozero) break;                 // always ends at 0 by itself
            if(!c->isSymmetric()) { c->symArg -= dE; break; }
            // hills in between ride along on the new level
        }
        f->translateValues(a);
    }

    updateAfterEdit(a);

    QString unit = isUnit(f, true) ? "g" : QString::fromUtf8("°/s");
    bool byDistance = sec->bArgument == DISTANCE;
    QString xs = byDistance ? QString("%1 m").arg(a->maxArgument - a->minArgument, 0, 'f', 1)
                            : QString("%1 s").arg(a->maxArgument - a->minArgument, 0, 'f', 2);
    QString tip = QString("Length %1   Value %2 %3").arg(xs).arg(a->endValue(), 0, 'f', 2).arg(unit);
    if(!vertical) tip += tr("   (end of a hill: moves sideways only)");
    if(lastPoint) tip += tr("   (section end)");
    if(!free) tip += tr("   - Shift: free");
    showTip(target, tip);
}

void curveEditor::dragBodyKeepFormula(const QPoint& target, bool moveX, bool moveY, bool free)
{
    func* f = mFunc;
    subfunc* a = f->funcList[mIndex];
    QCustomPlot* plot = mOwner->plot();
    QCPAxis* ax = axisFor(f);
    if(a->degree == tozero) {
        showTip(target, tr("\"To zero\" transitions follow automatically"));
        return;
    }
    const double n0 = sectionStart();
    const double len = a->maxArgument - a->minArgument;
    double argM = plot->xAxis->pixelToCoord(target.x()) - n0;
    if(argM < a->minArgument + 0.02*len) argM = a->minArgument + 0.02*len;
    if(argM > a->maxArgument - 0.02*len) argM = a->maxArgument - 0.02*len;
    const double y = snapValue(f, ax, ax->pixelToCoord(target.y()), free);
    QString unit = isUnit(f, true) ? "g" : QString::fromUtf8("°/s");
    QString tip;

    if(a->isSymmetric()) {
        // hill: sideways moves the top, up/down sets the height so the line goes through the mouse
        if(moveX) {
            double u = peakFromCenter(mOrigCenter) + (argM - mPressArg)/len;
            if(u < 0.08) u = 0.08;
            if(u > 0.92) u = 0.92;
            a->centerArg = (float)centerFromPeak(u);
        }
        if(moveY) {
            float s = unitShape(a, moveX ? argM : mPressArg);
            if(std::fabs(s) > 0.08f) a->symArg = (float)((y - a->startValue)/s);
        }
        tip = tr("Hill height %1 %2   top at %3 %").arg(a->symArg, 0, 'f', 2).arg(unit)
                .arg(peakFromCenter(a->centerArg)*100., 0, 'f', 0);
    } else {
        // ramp: the ends stay, the line bends through the mouse (same formula, different centre)
        float total = a->symArg;
        if(std::fabs(total) < 1e-4f) {
            showTip(target, tr("This transition is flat - switch off \"Keep formula\" to pull it"));
            return;
        }
        if(moveX || moveY) {
            double tFrac = (y - a->startValue)/total;
            if(tFrac < 0.02) tFrac = 0.02;
            if(tFrac > 0.98) tFrac = 0.98;
            double lo = -14., hi = 14.;
            for(int it = 0; it < 40; ++it) {
                double mid = 0.5*(lo + hi);
                a->centerArg = (float)mid;
                if(unitShape(a, argM) > tFrac) lo = mid; else hi = mid;
            }
            a->centerArg = (float)(0.5*(lo + hi));
        }
        tip = tr("Bend %1 (ends stay)").arg(a->centerArg, 0, 'f', 2);
    }
    updateAfterEdit(a);
    if(!free) tip += tr("   - Shift: free");
    showTip(target, tip + tr("   [Keep formula]"));
}

void curveEditor::updateAfterEdit(subfunc* from)
{
    track* t = mOwner->selTrack->trackData;
    t->updateTrack(t->activeSection, (int)(from->minArgument*F_HZ-1.5f));
    mOwner->redrawGraphs();
    gloParent->updateInfoPanel();
}

void curveEditor::showTip(const QPoint& pixel, const QString& text)
{
    QToolTip::showText(mOwner->plot()->mapToGlobal(pixel + QPoint(14, 14)), text, mOwner->plot());
}

void curveEditor::commitUndo(const QByteArray& before, func* f, int index)
{
    trackHandler* th = mOwner->selTrack;
    section* sec = th->trackData->activeSection;
    QByteArray after = funcsnapshot::save(sec);
    if(after == before) return;
    if(index < 0) index = 0;
    if(index >= f->funcList.size()) index = f->funcList.size()-1;
    mOwner->changeSelection(f->funcList[index]);       // the undo step remembers the selected transition
    undoAction* u = new undoAction(th, restoreSectionFunctions);
    u->funcsBefore = before;
    u->funcsAfter = after;
    th->mUndoHandler->addAction(u);
    gloParent->setUndoButtons();
}

void curveEditor::endDrag()
{
    if(mMode == None && !mStarted) return;
    func* f = mFunc;
    int idx = mMode == Vertex ? mIndex-1 : mIndex;
    bool started = mStarted;
    mMode = None;
    mStarted = false;
    mFunc = NULL;
    QToolTip::hideText();
    mOwner->plot()->setCursor(Qt::ArrowCursor);
    if(started && f) commitUndo(mBefore, f, idx);
    mOwner->transitionHandler->changeSubfunc(mOwner->selFunc);   // spin boxes show the new values
    mOwner->redrawGraphs();
}

void curveEditor::removePoint(func* f, int k)
{
    if(k < 1 || k >= f->funcList.size()) return;
    section* sec = mOwner->selTrack->trackData->activeSection;
    QByteArray before = funcsnapshot::save(sec);
    subfunc* a = f->funcList[k-1];
    subfunc* b = f->funcList[k];
    const float start = a->startValue;
    const float endB = b->endValue();
    const float la = a->maxArgument - a->minArgument, lb = b->maxArgument - b->minArgument;
    const bool bLocked = b->locked;
    const bool toZero = b->degree == tozero;
    eDegree d = toZero ? tozero : ((!a->isSymmetric() && a->degree != tozero) ? a->degree : cubic);
    float arg1 = a->arg1;

    f->removeSubFunction(k);
    mOwner->changeSelection(a);          // b is gone - never leave a dangling selection
    f->changeLength(la + lb, k-1);
    if(d != a->degree) a->changeDegree(d); else a->arg1 = arg1;
    a->centerArg = a->tensionArg = 0.f;
    a->locked = bLocked;
    if(d != tozero) a->symArg = endB - start;
    f->translateValues(a);

    updateAfterEdit(a);
    commitUndo(before, f, k-1);
    mOwner->transitionHandler->changeSubfunc(mOwner->selFunc);
    mOwner->redrawGraphs();
}

// ---------------------------------------------------------------------------
//  mouse handling
// ---------------------------------------------------------------------------

bool curveEditor::eventFilter(QObject* obj, QEvent* event)
{
    if(obj != mOwner->plot() || !mOwner->selTrack) return QObject::eventFilter(obj, event);
    QCustomPlot* plot = mOwner->plot();

    switch(event->type()) {
    case QEvent::MouseButtonPress: {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if(me->button() != Qt::LeftButton) return false;
        hit h = hitTest(me->pos());
        if(h.mode == None) return false;          // plot pans / deselects as usual
        beginDrag(h, me->pos());
        return true;
    }
    case QEvent::MouseMove: {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if(mMode != None) {
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if(now - mLastApply >= 25) {          // keeps the track recalculation responsive
                mLastApply = now;
                dragTo(me->pos(), me->modifiers() & Qt::ShiftModifier);
            }
            return true;
        }
        if(me->buttons() != Qt::NoButton) return false;
        hit h = hitTest(me->pos());
        if(h.mode == Vertex) plot->setCursor(vertexMovesVertically(h.f, h.index) ? Qt::SizeAllCursor : Qt::SizeHorCursor);
        else if(h.mode == Body) plot->setCursor(Qt::OpenHandCursor);
        else plot->setCursor(Qt::ArrowCursor);
        // ring where the line would be grabbed
        if(mHover) {
            QVector<double> x, y;
            if(h.mode == Body) {
                x.append(sectionStart() + h.arg);
                y.append(valueAt(h.f, h.arg));
                mHover->setValueAxis(axisFor(h.f));
            }
            bool was = mHover->data()->size() > 0;
            mHover->setData(x, y);
            if(was || !x.isEmpty()) plot->replot();
        }
        return false;
    }
    case QEvent::MouseButtonRelease: {
        if(mMode == None) return false;
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        dragTo(me->pos(), me->modifiers() & Qt::ShiftModifier);
        endDrag();
        return true;
    }
    case QEvent::Leave:
        if(mHover && mHover->data()->size()) {
            mHover->clearData();
            plot->replot();
        }
        return false;
    case QEvent::ContextMenu: {
        QContextMenuEvent* ce = static_cast<QContextMenuEvent*>(event);
        hit h = hitTest(ce->pos());
        if(h.mode == None) return false;          // the plot's own menu
        QMenu menu;
        QAction* remove = NULL;
        QAction* add = NULL;
        if(h.mode == Vertex && h.index < h.f->funcList.size()) remove = menu.addAction(tr("Remove point"));
        if(h.mode == Body) add = menu.addAction(tr("Add point here"));
        menu.addSeparator();
        QAction* keep = menu.addAction(tr("Keep formula when dragging the line"));
        keep->setCheckable(true);
        keep->setChecked(keepFormula());
        QAction* chosen = menu.exec(ce->globalPos());
        if(chosen && chosen == remove) {
            removePoint(h.f, h.index);
        } else if(chosen && chosen == add) {
            section* sec = mOwner->selTrack->trackData->activeSection;
            QByteArray before = funcsnapshot::save(sec);
            mOwner->selectTransition(h.f->funcList[h.index]);
            if(splitAt(h.f, h.index, h.arg)) {
                updateAfterEdit(h.f->funcList[h.index]);
                commitUndo(before, h.f, h.index+1);
                mOwner->transitionHandler->changeSubfunc(mOwner->selFunc);
                mOwner->redrawGraphs();
            } else {
                showTip(ce->pos(), tr("Too close to a point"));
            }
        } else if(chosen == keep) {
            setKeepFormula(keep->isChecked());
            emit mOwner->keepFormulaChanged();
        }
        return true;
    }
    default:
        break;
    }
    return false;
}
