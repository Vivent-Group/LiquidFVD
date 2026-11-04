#ifndef CURVEEDITOR_H
#define CURVEEDITOR_H

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
#include <QPoint>
#include <QByteArray>

class graphWidget;
class func;
class subfunc;
class section;
class QCPAxis;
class QCPGraph;

/*
 * Direct editing of the force / rotation graphs with the mouse ("like dough").
 *
 *  Points (ends of transitions of the selected function):
 *    drag up/down    = value of that point, the next point stays where it is
 *    drag left/right = timing; dragging the last point lengthens the section
 *    right-click     = remove the point (the two transitions are merged)
 *
 *  Anywhere on a line:
 *    Keep formula ON   the transition keeps its type: a hill gets higher / lower (and its
 *                      top moves sideways), a ramp bends - its ends stay put
 *    Keep formula OFF  a new point is created where you grab and stays where you drop it
 *
 *  Values snap to a grid that follows the zoom; hold Shift to move freely.
 *  Every drag is one undo step.
 */
class curveEditor : public QObject
{
    Q_OBJECT
public:
    explicit curveEditor(graphWidget* owner);

    static bool keepFormula();
    static void setKeepFormula(bool on);

    bool isDragging() const { return mMode != None; }
    // redraws the point markers (call after the plot was rebuilt)
    void refreshMarkers();

    // the editable functions shown in the plot and their value axis
    struct target { func* f; QCPAxis* axis; };
    QList<target> targets();

protected:
    bool eventFilter(QObject* obj, QEvent* event);

private:
    enum Mode { None, Vertex, Body };
    struct hit { Mode mode; func* f; int index; double arg; hit() : mode(None), f(0), index(-1), arg(0.) {} };

    hit    hitTest(const QPoint& pixel);
    func*  selectedFunc();
    QCPAxis* axisFor(func* f);
    double sectionStart();
    double minLength() const;
    double niceStep(QCPAxis* axis, double minStep);
    double snapValue(func* f, QCPAxis* axis, double v, bool free);
    double snapArg(double a, bool free);
    float  valueAt(func* f, double arg);
    bool   vertexMovesVertically(func* f, int vertex);

    void   beginDrag(const hit& h, const QPoint& pixel);
    void   dragTo(const QPoint& pixel, bool free);
    void   endDrag();
    void   dragVertex(const QPoint& target, bool moveX, bool moveY, bool free);
    void   dragBodyKeepFormula(const QPoint& target, bool moveX, bool moveY, bool free);
    bool   splitAt(func* f, int index, double arg);   // free mode: new point
    void   removePoint(func* f, int vertex);
    void   commitUndo(const QByteArray& before, func* f, int index);
    void   updateAfterEdit(subfunc* from);
    void   showTip(const QPoint& pixel, const QString& text);

    graphWidget* mOwner;
    QCPGraph* mMarkers;          // movable points
    QCPGraph* mHorizontalOnly;   // points that only move sideways (end of a hill)
    QCPGraph* mHover;            // ring where the line would be grabbed

    Mode   mMode;
    bool   mStarted;             // mouse moved far enough to really edit
    func*  mFunc;
    int    mIndex;               // vertex (Vertex) or transition (Body)
    double mPressArg;
    QPoint mPressPos;
    QPoint mGrabOffset;
    QByteArray mBefore;          // functions of the section at drag start (undo)
    // drag start state
    double mOrigArg, mOrigVal, mNextMax, mOrigCenter, mOrigSym, mOrigSectionEnd;
    QList<float> mOrigEnds;      // end argument of every function of the section
    qint64 mLastApply;
};

#endif // CURVEEDITOR_H
