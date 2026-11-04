#ifndef PROJECTMEDIA_H
#define PROJECTMEDIA_H

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
#include <QVector>
#include <QList>
#include <QColor>
#include <QElapsedTimer>

class trackHandler;
struct musicBackend;

/*
 * A bookmark on the timeline.
 *  - Music bookmarks sit at a fixed time in the song, so they show which part of the
 *    ride currently hits that beat, wherever the layout is changed.
 *  - Track bookmarks sit at a fixed spot of the ride (section + time inside the section)
 *    and move along when earlier parts of the layout get longer or shorter.
 */
struct bookmark
{
    enum Kind { Music = 0, Track = 1 };

    QString name;
    Kind    kind;
    double  time;           // Music: seconds into the song / Track: seconds into the section
    int     trackIndex;     // Track bookmarks only
    int     sectionIndex;   // Track bookmarks only

    bookmark() : kind(Music), time(0.), trackIndex(0), sectionIndex(0) {}
    QColor color() const { return kind == Music ? QColor(230, 60, 160) : QColor(255, 165, 30); }
};

/*
 * Music, playback clock and bookmarks of the open project.
 * Stored next to the project file as <name>.timeline.json so the .fvd format stays untouched.
 */
class projectMedia : public QObject
{
    Q_OBJECT
public:
    static projectMedia* instance();

    // ---- music ----
    bool    loadMusic(const QString& file, QString* error);
    void    clearMusic();
    bool    hasMusic() const { return !mMusicFile.isEmpty(); }
    QString musicFile() const { return mMusicFile; }
    double  musicDuration() const { return mDuration; }
    // track time (seconds from the start of the ride) at which the song begins
    double  musicStart() const { return mMusicStart; }
    void    setMusicStart(double trackTime);
    // cue = position in the song at which the ride starts (= -musicStart)
    double  cue() const { return -mMusicStart; }
    void    setCue(double songTime) { setMusicStart(-songTime); }
    // pre-roll: Play from the start of the ride plays the song from its beginning, the train waits for the cue
    bool    preRoll() const { return mPreRoll; }
    void    setPreRoll(bool on);
    // song position right now (while playing) or at the given ride time
    double  songPositionAt(double trackTime) const { return trackTime - mMusicStart; }
    // normalized peak level (0..1), peaksPerSecond() entries per second of song
    const QVector<float>& peaks() const { return mPeaks; }
    static double peaksPerSecond() { return 200.; }
    float   peakBetween(double songFrom, double songTo) const;

    // ---- playback ----
    bool    startPlayback(double trackTime);   // false if nothing could be started
    void    stopPlayback();
    bool    isPlaying() const { return mPlaying; }
    double  playbackTrackTime();               // current track time while playing
    bool    audioDeviceAvailable();            // false: plays silently, clock still runs
    // where Play starts when not in POV mode
    double  cursorTime() const { return mCursor; }
    void    setCursorTime(double t);

    // ---- bookmarks ----
    QList<bookmark> bookmarks() const { return mBookmarks; }
    void    addBookmark(const bookmark& b);
    void    removeBookmark(int index);
    void    renameBookmark(int index, const QString& name);
    // creates a bookmark of the given kind at a track time of the given track
    bookmark makeBookmark(double trackTime, trackHandler* onTrack, bookmark::Kind kind, const QString& name);
    // track time of a bookmark on the given track, < 0 if it doesn't belong there
    double  bookmarkTrackTime(const bookmark& b, trackHandler* onTrack) const;
    QString nextBookmarkName() const;

    // ---- project file ----
    static QString sidecarFor(const QString& projectFile);
    bool    save(const QString& projectFile, QString* error = NULL);
    void    load(const QString& projectFile);   // silently resets if there is no sidecar
    void    reset();

signals:
    void musicChanged();
    void bookmarksChanged();
    void playbackStateChanged(bool playing);

private:
    projectMedia();
    ~projectMedia();
    void    startSoundAt(double songTime);
    bool    ensureEngine();
    bool    decodePeaks(const QString& file, QVector<float>* peaks, double* duration);
public:
    // converts formats miniaudio can't read (M4A / AAC / ALAC / CAF ...) to WAV with a system tool:
    // macOS afconvert, otherwise ffmpeg if installed. Cached. Empty string if not possible.
    static QString convertToWav(const QString& file, QString* error);
private:

    QString mMusicFile;         // what the user picked (saved in the project)
    QString mPlaybackFile;      // what is decoded / played (a converted copy for M4A, AAC, ...)
    double  mDuration;
    double  mMusicStart;
    bool    mPreRoll;
    QVector<float> mPeaks;
    double  mCursor;

    QList<bookmark> mBookmarks;

    // playback clock: wall clock anchored at mAnchorTrack, or the song position while it plays
    bool    mPlaying;
    bool    mSoundRunning;
    double  mAnchorTrack;
    QElapsedTimer mWall;

    musicBackend* mBackend;
};

#endif // PROJECTMEDIA_H
