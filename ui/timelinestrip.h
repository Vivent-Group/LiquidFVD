#ifndef TIMELINESTRIP_H
#define TIMELINESTRIP_H

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

class graphWidget;
class QToolButton;
class QLabel;
class QDoubleSpinBox;
class waveView;
class QCheckBox;

/*
 * Lives directly below the graph plot and shares its x axis:
 * transport (play / stop / time), the music waveform, bookmarks and the play head.
 */
class timelineStrip : public QWidget
{
    Q_OBJECT
public:
    explicit timelineStrip(graphWidget* owner);
    graphWidget* owner() const { return mOwner; }
    double playheadTime() const;
    double trackDuration() const;

public slots:
    void refresh();          // repaint waveform / play head and update the time readout
    void refreshControls();

private slots:
    void onPlayClicked();
    void onStopClicked();
    void onLoadMusic();
    void onRemoveMusic();
    void onStartChanged(double v);
    void onAddMarker();
    void onPlaybackState(bool playing);

private:
    graphWidget*   mOwner;
    QToolButton*   mPlay;
    QToolButton*   mStop;
    QLabel*        mTime;
    QToolButton*   mMusic;
    QDoubleSpinBox* mStart;
    QToolButton*   mMarker;
    QCheckBox*     mKeepFormula;
    QToolButton*   mSetCue;
    QCheckBox*     mPreRoll;
    waveView*      mWave;
    friend class waveView;
};

class waveView : public QWidget
{
    Q_OBJECT
public:
    explicit waveView(timelineStrip* strip);
    QSize sizeHint() const;

protected:
    void paintEvent(QPaintEvent*);
    void mousePressEvent(QMouseEvent*);
    void mouseMoveEvent(QMouseEvent*);
    void mouseReleaseEvent(QMouseEvent*);
    void mouseDoubleClickEvent(QMouseEvent*);
    void wheelEvent(QWheelEvent*);
    void contextMenuEvent(QContextMenuEvent*);

private:
    double timeAtPixel(int x) const;
    int    pixelAtTime(double t) const;
    int    bookmarkAt(int x) const;

    timelineStrip* mStrip;
    bool   mScrubbing;
    bool   mShifting;       // Ctrl+drag moves the song against the ride
    int    mPressX;
    double mPressStart;
};

#endif // TIMELINESTRIP_H
