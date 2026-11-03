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

#include "timelinestrip.h"
#include "graphwidget.h"
#include "projectmedia.h"
#include "mainwindow.h"
#include "trackhandler.h"
#include "track.h"
#include "theme.h"
#include "qcustomplot.h"
#include "glviewwidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QInputDialog>
#include <QApplication>
#include <QFileInfo>
#include <cmath>
#include <cstdlib>

extern MainWindow* gloParent;
extern glViewWidget* glView;

static QString formatTime(double t)
{
    bool neg = t < 0;
    if(neg) t = -t;
    int m = (int)(t / 60.);
    double s = t - m*60.;
    return QString("%1%2:%3").arg(neg ? "-" : "").arg(m).arg(s, 4, 'f', 1, QChar('0'));
}

// ---------------------------------------------------------------------------
//  timelineStrip
// ---------------------------------------------------------------------------

timelineStrip::timelineStrip(graphWidget* owner)
    : QWidget(owner), mOwner(owner)
{
    QVBoxLayout* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);

    QHBoxLayout* bar = new QHBoxLayout();
    bar->setContentsMargins(4, 0, 4, 0);
    bar->setSpacing(6);

    mPlay = new QToolButton(this);
    mPlay->setToolTip(tr("Play / pause - rides along in POV view (Ctrl+P)"));
    mPlay->setAutoRaise(true);
    mStop = new QToolButton(this);
    mStop->setText(QString::fromUtf8("■"));
    mStop->setToolTip(tr("Stop and leave POV view"));
    mStop->setAutoRaise(true);
    mTime = new QLabel(this);
    mTime->setMinimumWidth(110);

    mMusic = new QToolButton(this);
    mMusic->setPopupMode(QToolButton::InstantPopup);
    mMusic->setToolButtonStyle(Qt::ToolButtonTextOnly);
    QMenu* musicMenu = new QMenu(mMusic);
    musicMenu->addAction(tr("Load music (WAV, MP3, FLAC)..."), this, SLOT(onLoadMusic()));
    musicMenu->addAction(tr("Remove music"), this, SLOT(onRemoveMusic()));
    mMusic->setMenu(musicMenu);

    QLabel* startLabel = new QLabel(tr("Song starts at"), this);
    mStart = new QDoubleSpinBox(this);
    mStart->setRange(-3600., 3600.);
    mStart->setDecimals(2);
    mStart->setSingleStep(0.1);
    mStart->setSuffix(" s");
    mStart->setToolTip(tr("Ride time at which the song begins (negative: song starts before the ride).\nTip: Ctrl+drag the waveform."));

    mMarker = new QToolButton(this);
    mMarker->setText(tr("+ Bookmark"));
    mMarker->setToolTip(tr("Bookmark at the play head (Ctrl+M) - double-click the strip to place one anywhere"));
    mMarker->setAutoRaise(true);

    QLabel* hint = new QLabel(tr("Click: play head  ·  Double-click: bookmark  ·  Ctrl+drag: move song"), this);
    QFont f = hint->font();
    f.setPointSizeF(f.pointSizeF()*0.85);
    hint->setFont(f);
    hint->setEnabled(false);

    bar->addWidget(mPlay);
    bar->addWidget(mStop);
    bar->addWidget(mTime);
    bar->addSpacing(8);
    bar->addWidget(mMusic);
    bar->addWidget(startLabel);
    bar->addWidget(mStart);
    bar->addWidget(mMarker);
    bar->addStretch(1);
    bar->addWidget(hint);
    v->addLayout(bar);

    mWave = new waveView(this);
    v->addWidget(mWave);

    connect(mPlay, SIGNAL(clicked()), this, SLOT(onPlayClicked()));
    connect(mStop, SIGNAL(clicked()), this, SLOT(onStopClicked()));
    connect(mStart, SIGNAL(valueChanged(double)), this, SLOT(onStartChanged(double)));
    connect(mMarker, SIGNAL(clicked()), this, SLOT(onAddMarker()));

    projectMedia* media = projectMedia::instance();
    connect(media, SIGNAL(musicChanged()), this, SLOT(refreshControls()));
    connect(media, SIGNAL(bookmarksChanged()), this, SLOT(refresh()));
    connect(media, SIGNAL(playbackStateChanged(bool)), this, SLOT(onPlaybackState(bool)));

    refreshControls();
    onPlaybackState(media->isPlaying());
}

double timelineStrip::trackDuration() const
{
    if(!mOwner->selTrack) return 0.;
    return mOwner->selTrack->trackData->getNumPoints() / F_HZ;
}

double timelineStrip::playheadTime() const
{
    projectMedia* media = projectMedia::instance();
    if(media->isPlaying() || (glView && glView->povMode && gloParent->mGraphWidget == mOwner)) {
        return glView->povPos / F_HZ;
    }
    return media->cursorTime();
}

void timelineStrip::refresh()
{
    double dur = trackDuration();
    mTime->setText(QString("%1 / %2").arg(formatTime(playheadTime())).arg(formatTime(dur)));
    mWave->update();
}

void timelineStrip::refreshControls()
{
    projectMedia* media = projectMedia::instance();
    if(media->hasMusic()) {
        QString name = QFileInfo(media->musicFile()).fileName();
        if(name.length() > 28) name = name.left(25) + "...";
        mMusic->setText(QString::fromUtf8("♪ ") + name);
        mMusic->setToolTip(media->musicFile());
    } else {
        mMusic->setText(QString::fromUtf8("♪ ") + tr("No music"));
        mMusic->setToolTip(tr("Load a song to see it below the ride"));
    }
    mStart->blockSignals(true);
    mStart->setValue(media->musicStart());
    mStart->blockSignals(false);
    mStart->setEnabled(media->hasMusic());
    refresh();
}

void timelineStrip::onPlaybackState(bool playing)
{
    mPlay->setText(playing ? QString::fromUtf8("❚❚") : QString::fromUtf8("▶"));
    refresh();
}

void timelineStrip::onPlayClicked()   { gloParent->togglePlayback(); }
void timelineStrip::onStopClicked()   { gloParent->stopPlayback(); }
void timelineStrip::onLoadMusic()     { gloParent->loadMusicDialog(); }
void timelineStrip::onRemoveMusic()   { projectMedia::instance()->clearMusic(); }
void timelineStrip::onAddMarker()     { gloParent->addBookmarkAtPlayhead(); }

void timelineStrip::onStartChanged(double v)
{
    projectMedia::instance()->setMusicStart(v);
}

// ---------------------------------------------------------------------------
//  waveView
// ---------------------------------------------------------------------------

waveView::waveView(timelineStrip* strip)
    : QWidget(strip), mStrip(strip), mScrubbing(false), mShifting(false), mPressX(0), mPressStart(0.)
{
    setMinimumHeight(54);
    setMouseTracking(true);
    setCursor(Qt::IBeamCursor);
}

QSize waveView::sizeHint() const
{
    return QSize(400, 60);
}

double waveView::timeAtPixel(int x) const
{
    graphWidget* g = mStrip->owner();
    return g->coordToTime(g->plot()->xAxis->pixelToCoord(x));
}

int waveView::pixelAtTime(double t) const
{
    graphWidget* g = mStrip->owner();
    return (int)std::floor(g->plot()->xAxis->coordToPixel(g->timeToCoord(t)) + 0.5);
}

int waveView::bookmarkAt(int x) const
{
    projectMedia* media = projectMedia::instance();
    QList<bookmark> marks = media->bookmarks();
    int best = -1, bestDist = 6;
    for(int i = 0; i < marks.size(); ++i) {
        double t = media->bookmarkTrackTime(marks[i], mStrip->owner()->selTrack);
        if(t < 0) continue;
        int d = std::abs(pixelAtTime(t) - x);
        if(d <= bestDist) { bestDist = d; best = i; }
    }
    return best;
}

void waveView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const bool dark = theme::isDark();
    const int w = width(), h = height();
    graphWidget* g = mStrip->owner();
    projectMedia* media = projectMedia::instance();

    QColor bg      = dark ? QColor(26, 26, 29)  : QColor(250, 250, 252);
    QColor outside = dark ? QColor(38, 38, 42)  : QColor(232, 232, 236);
    QColor wave    = dark ? QColor(80, 160, 240) : QColor(40, 110, 200);
    QColor text    = dark ? QColor(170, 170, 176) : QColor(90, 90, 96);
    p.fillRect(rect(), outside);

    if(!g->selTrack) return;

    // only draw where the plot above has its data area, so both line up exactly
    QRect plotArea = g->plot()->axisRect()->rect();
    p.setClipRect(QRect(plotArea.left(), 0, plotArea.width(), h));

    // ride extent
    double dur = mStrip->trackDuration();
    int x0 = pixelAtTime(0.), x1 = pixelAtTime(dur);
    p.fillRect(QRect(QPoint(qMax(0, x0), 0), QPoint(qMin(w, x1), h)), bg);

    const int mid = h/2;
    if(media->hasMusic()) {
        // waveform: one vertical line per pixel, peak of the song time it covers
        double start = media->musicStart();
        p.setPen(wave);
        double tPrev = timeAtPixel(0);
        for(int x = 0; x < w; ++x) {
            double tNext = timeAtPixel(x+1);
            double a = qMin(tPrev, tNext) - start, b = qMax(tPrev, tNext) - start;
            tPrev = tNext;
            if(b < 0. || a > media->musicDuration()) continue;
            float peak = media->peakBetween(a, b);
            int amp = (int)(peak * (mid-3));
            p.drawLine(x, mid-amp, x, mid+amp);
        }
        // song start / end
        p.setPen(QPen(wave.darker(dark ? 60 : 130), 1, Qt::DotLine));
        int sx = pixelAtTime(start);
        int ex = pixelAtTime(start + media->musicDuration());
        p.drawLine(sx, 0, sx, h);
        p.drawLine(ex, 0, ex, h);
    } else {
        p.setPen(text);
        p.drawText(QRect(plotArea.left(), 0, plotArea.width(), h), Qt::AlignCenter, tr("No music loaded - use ♪ to add a WAV, MP3 or FLAC file"));
    }

    // bookmarks
    QList<bookmark> marks = media->bookmarks();
    QFontMetrics fm(font());
    for(int i = 0; i < marks.size(); ++i) {
        double t = media->bookmarkTrackTime(marks[i], g->selTrack);
        if(t < 0) continue;
        int x = pixelAtTime(t);
        if(x < -200 || x > w + 5) continue;
        QColor c = marks[i].color();
        p.setPen(QPen(c, 2));
        p.drawLine(x, 0, x, h);
        QString label = marks[i].name;
        #if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        int tw = fm.horizontalAdvance(label);
#else
        int tw = fm.width(label);
#endif
        QRect r(x+2, 1, tw+8, fm.height()+2);
        p.fillRect(r, c);
        p.setPen(Qt::white);
        p.drawText(r, Qt::AlignCenter, label);
    }

    // play head
    int px = pixelAtTime(mStrip->playheadTime());
    p.setPen(QPen(dark ? QColor(255, 255, 255) : QColor(220, 30, 30), 2));
    p.drawLine(px, 0, px, h);
}

void waveView::mousePressEvent(QMouseEvent* e)
{
    if(e->button() != Qt::LeftButton) return;
    mPressX = e->pos().x();
    if(e->modifiers() & Qt::ControlModifier && projectMedia::instance()->hasMusic()) {
        mShifting = true;
        mPressStart = projectMedia::instance()->musicStart();
        setCursor(Qt::SizeHorCursor);
        return;
    }
    mScrubbing = true;
    gloParent->seekPlayback(timeAtPixel(e->pos().x()));
}

void waveView::mouseMoveEvent(QMouseEvent* e)
{
    if(mShifting) {
        double dt = timeAtPixel(e->pos().x()) - timeAtPixel(mPressX);
        projectMedia::instance()->setMusicStart(std::floor((mPressStart + dt)*100. + 0.5)/100.);
        return;
    }
    if(mScrubbing) {
        gloParent->seekPlayback(timeAtPixel(e->pos().x()));
        return;
    }
    setCursor(bookmarkAt(e->pos().x()) >= 0 ? Qt::PointingHandCursor : Qt::IBeamCursor);
}

void waveView::mouseReleaseEvent(QMouseEvent*)
{
    mScrubbing = false;
    if(mShifting) {
        mShifting = false;
        setCursor(Qt::IBeamCursor);
    }
}

void waveView::mouseDoubleClickEvent(QMouseEvent* e)
{
    if(e->button() != Qt::LeftButton) return;
    double t = timeAtPixel(e->pos().x());
    projectMedia* media = projectMedia::instance();
    bool onSong = media->hasMusic() && t >= media->musicStart() && t <= media->musicStart() + media->musicDuration();
    gloParent->addBookmarkAt(t, onSong ? bookmark::Music : bookmark::Track);
}

void waveView::wheelEvent(QWheelEvent* e)
{
    // zoom together with the plot above
    QApplication::sendEvent(mStrip->owner()->plot(), e);
}

void waveView::contextMenuEvent(QContextMenuEvent* e)
{
    projectMedia* media = projectMedia::instance();
    double t = timeAtPixel(e->pos().x());
    int hit = bookmarkAt(e->pos().x());
    QMenu menu(this);

    if(hit >= 0) {
        bookmark b = media->bookmarks()[hit];
        QAction* rename = menu.addAction(tr("Rename \"%1\"...").arg(b.name));
        QAction* remove = menu.addAction(tr("Delete \"%1\"").arg(b.name));
        QAction* toggle = menu.addAction(b.kind == bookmark::Music ? tr("Pin to the ride instead of the song")
                                                                    : tr("Pin to the song instead of the ride"));
        toggle->setEnabled(b.kind == bookmark::Music || media->hasMusic());
        QAction* chosen = menu.exec(e->globalPos());
        if(chosen == rename) {
            bool ok = false;
            QString name = QInputDialog::getText(this, tr("Rename bookmark"), tr("Name:"), QLineEdit::Normal, b.name, &ok);
            if(ok && !name.trimmed().isEmpty()) media->renameBookmark(hit, name.trimmed());
        } else if(chosen == remove) {
            media->removeBookmark(hit);
        } else if(chosen == toggle) {
            double at = media->bookmarkTrackTime(b, mStrip->owner()->selTrack);
            bookmark nb = media->makeBookmark(at, mStrip->owner()->selTrack,
                                              b.kind == bookmark::Music ? bookmark::Track : bookmark::Music, b.name);
            media->removeBookmark(hit);
            media->addBookmark(nb);
        }
        return;
    }

    QAction* musicMark = menu.addAction(tr("Add song bookmark here"));
    musicMark->setEnabled(media->hasMusic());
    QAction* trackMark = menu.addAction(tr("Add ride bookmark here"));
    menu.addSeparator();
    QAction* songHere = menu.addAction(tr("Let the song start here"));
    songHere->setEnabled(media->hasMusic());
    menu.addSeparator();
    QAction* load = menu.addAction(tr("Load music..."));

    QAction* chosen = menu.exec(e->globalPos());
    if(chosen == musicMark) gloParent->addBookmarkAt(t, bookmark::Music);
    else if(chosen == trackMark) gloParent->addBookmarkAt(t, bookmark::Track);
    else if(chosen == songHere) media->setMusicStart(std::floor(t*100. + 0.5)/100.);
    else if(chosen == load) gloParent->loadMusicDialog();
}
