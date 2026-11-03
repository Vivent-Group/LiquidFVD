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

#include "projectmedia.h"
#include "mainwindow.h"
#include "trackhandler.h"
#include "track.h"
#include "section.h"
#include "mnode.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtDebug>
#include <cmath>

#include "miniaudio.h"

extern MainWindow* gloParent;

struct musicBackend
{
    ma_engine engine;
    bool      engineTried;
    bool      engineOk;
    ma_sound  sound;
    bool      soundOk;

    musicBackend() : engineTried(false), engineOk(false), soundOk(false) {}
};

projectMedia* projectMedia::instance()
{
    static projectMedia* inst = new projectMedia();
    return inst;
}

projectMedia::projectMedia()
    : mDuration(0.), mMusicStart(0.), mCursor(0.),
      mPlaying(false), mSoundRunning(false), mAnchorTrack(0.),
      mBackend(new musicBackend())
{
}

projectMedia::~projectMedia()
{
    if(mBackend->soundOk) ma_sound_uninit(&mBackend->sound);
    if(mBackend->engineOk) ma_engine_uninit(&mBackend->engine);
    delete mBackend;
}

bool projectMedia::ensureEngine()
{
    if(!mBackend->engineTried) {
        mBackend->engineTried = true;
        mBackend->engineOk = ma_engine_init(NULL, &mBackend->engine) == MA_SUCCESS;
        if(!mBackend->engineOk) qWarning("No audio output device - music plays silently.");
    }
    return mBackend->engineOk;
}

bool projectMedia::audioDeviceAvailable()
{
    return ensureEngine();
}

// ---------------------------------------------------------------------------
//  Music
// ---------------------------------------------------------------------------

bool projectMedia::loadMusic(const QString& file, QString* error)
{
    // 1) decode once to get the length and the waveform (mono, native sample rate)
    ma_decoder decoder;
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 1, 0);
    QByteArray path = QFile::encodeName(file);
    if(ma_decoder_init_file(path.constData(), &cfg, &decoder) != MA_SUCCESS) {
        if(error) *error = QString("Could not read %1 (supported: WAV, MP3, FLAC)").arg(QFileInfo(file).fileName());
        return false;
    }
    ma_uint32 rate = decoder.outputSampleRate;
    if(rate == 0) rate = 44100;
    const ma_uint64 framesPerPeak = (ma_uint64)(rate / peaksPerSecond());

    QVector<float> peaks;
    QVector<float> buffer(4096);
    float curPeak = 0.f;
    ma_uint64 inBucket = 0, total = 0;
    for(;;) {
        ma_uint64 read = 0;
        ma_result r = ma_decoder_read_pcm_frames(&decoder, buffer.data(), buffer.size(), &read);
        for(ma_uint64 i = 0; i < read; ++i) {
            float v = std::fabs(buffer[(int)i]);
            if(v > curPeak) curPeak = v;
            if(++inBucket >= framesPerPeak) {
                peaks.append(curPeak);
                curPeak = 0.f;
                inBucket = 0;
            }
        }
        total += read;
        if(r != MA_SUCCESS || read == 0) break;
    }
    if(inBucket) peaks.append(curPeak);
    ma_decoder_uninit(&decoder);

    if(total == 0) {
        if(error) *error = QString("%1 contains no audio").arg(QFileInfo(file).fileName());
        return false;
    }

    // normalize so quiet songs still show a readable waveform
    float maxPeak = 0.f;
    for(int i = 0; i < peaks.size(); ++i) maxPeak = peaks[i] > maxPeak ? peaks[i] : maxPeak;
    if(maxPeak > 0.f) for(int i = 0; i < peaks.size(); ++i) peaks[i] /= maxPeak;

    // 2) prepare playback (streamed from disk)
    stopPlayback();
    if(mBackend->soundOk) {
        ma_sound_uninit(&mBackend->sound);
        mBackend->soundOk = false;
    }
    if(ensureEngine()) {
        mBackend->soundOk = ma_sound_init_from_file(&mBackend->engine, path.constData(),
                                                    MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                                    NULL, NULL, &mBackend->sound) == MA_SUCCESS;
    }

    mMusicFile = QFileInfo(file).absoluteFilePath();
    mDuration = total / (double)rate;
    mPeaks = peaks;
    emit musicChanged();
    emit bookmarksChanged();    // music bookmarks become visible
    return true;
}

void projectMedia::clearMusic()
{
    stopPlayback();
    if(mBackend->soundOk) {
        ma_sound_uninit(&mBackend->sound);
        mBackend->soundOk = false;
    }
    mMusicFile.clear();
    mDuration = 0.;
    mPeaks.clear();
    emit musicChanged();
    emit bookmarksChanged();
}

void projectMedia::setMusicStart(double trackTime)
{
    if(trackTime == mMusicStart) return;
    mMusicStart = trackTime;
    emit musicChanged();
    emit bookmarksChanged();    // music bookmarks move along with the song
}

float projectMedia::peakBetween(double songFrom, double songTo) const
{
    if(mPeaks.isEmpty()) return 0.f;
    int a = (int)std::floor(songFrom * peaksPerSecond());
    int b = (int)std::ceil(songTo * peaksPerSecond());
    if(b <= a) b = a + 1;
    if(b <= 0 || a >= mPeaks.size()) return 0.f;
    if(a < 0) a = 0;
    if(b > mPeaks.size()) b = mPeaks.size();
    float m = 0.f;
    for(int i = a; i < b; ++i) m = mPeaks[i] > m ? mPeaks[i] : m;
    return m;
}

// ---------------------------------------------------------------------------
//  Playback clock
// ---------------------------------------------------------------------------

void projectMedia::startSoundAt(double songTime)
{
    if(!mBackend->soundOk) return;
    ma_sound_seek_to_second(&mBackend->sound, (float)songTime);
    ma_sound_start(&mBackend->sound);
    mSoundRunning = true;
}

bool projectMedia::startPlayback(double trackTime)
{
    stopPlayback();
    mPlaying = true;
    mSoundRunning = false;
    mAnchorTrack = trackTime;
    mWall.start();

    double song = trackTime - mMusicStart;
    if(hasMusic() && song >= 0. && song < mDuration) startSoundAt(song);
    emit playbackStateChanged(true);
    return true;
}

void projectMedia::stopPlayback()
{
    if(mBackend->soundOk && mSoundRunning) ma_sound_stop(&mBackend->sound);
    mSoundRunning = false;
    if(mPlaying) {
        mPlaying = false;
        emit playbackStateChanged(false);
    }
}

double projectMedia::playbackTrackTime()
{
    if(!mPlaying) return mCursor;

    if(mSoundRunning) {
        if(ma_sound_is_playing(&mBackend->sound)) {
            // the song is the master clock, so picture and sound can't drift apart
            float cursor = 0.f;
            ma_sound_get_cursor_in_seconds(&mBackend->sound, &cursor);
            double t = mMusicStart + cursor;
            mAnchorTrack = t;
            mWall.restart();
            return t;
        }
        // song finished - continue on the wall clock from where it ended
        mSoundRunning = false;
    }

    const double anchor = mAnchorTrack;
    double t = anchor + mWall.nsecsElapsed() / 1e9;
    // ride started before the song: start the music once its start is reached
    double song = t - mMusicStart;
    if(hasMusic() && mBackend->soundOk && anchor < mMusicStart && song >= 0. && song < mDuration - 0.05) {
        startSoundAt(song);
        mAnchorTrack = t;
        mWall.restart();
    }
    return t;
}

void projectMedia::setCursorTime(double t)
{
    mCursor = t < 0. ? 0. : t;
}

// ---------------------------------------------------------------------------
//  Bookmarks
// ---------------------------------------------------------------------------

void projectMedia::addBookmark(const bookmark& b)
{
    mBookmarks.append(b);
    emit bookmarksChanged();
}

void projectMedia::removeBookmark(int index)
{
    if(index < 0 || index >= mBookmarks.size()) return;
    mBookmarks.removeAt(index);
    emit bookmarksChanged();
}

void projectMedia::renameBookmark(int index, const QString& name)
{
    if(index < 0 || index >= mBookmarks.size()) return;
    mBookmarks[index].name = name;
    emit bookmarksChanged();
}

QString projectMedia::nextBookmarkName() const
{
    return QString("Marker %1").arg(mBookmarks.size() + 1);
}

bookmark projectMedia::makeBookmark(double trackTime, trackHandler* onTrack, bookmark::Kind kind, const QString& name)
{
    bookmark b;
    b.name = name.isEmpty() ? nextBookmarkName() : name;
    b.kind = kind;
    if(kind == bookmark::Music || !onTrack) {
        b.kind = bookmark::Music;
        b.time = trackTime - mMusicStart;
        return b;
    }
    track* t = onTrack->trackData;
    b.trackIndex = gloParent->getTrackList().indexOf(onTrack);
    int node = 0, sec = 0;
    int index = (int)(trackTime * F_HZ + 0.5);
    if(index < 0) index = 0;
    if(index > t->getNumPoints()) index = t->getNumPoints();
    t->getSecNode(index, &node, &sec);
    if(sec < 0 || sec >= t->lSections.size()) sec = 0;
    b.sectionIndex = sec;
    b.time = trackTime - t->getNumPoints(t->lSections.isEmpty() ? NULL : t->lSections[sec]) / F_HZ;
    return b;
}

double projectMedia::bookmarkTrackTime(const bookmark& b, trackHandler* onTrack) const
{
    if(b.kind == bookmark::Music) {
        return mMusicStart + b.time;
    }
    if(!onTrack || gloParent->getTrackList().indexOf(onTrack) != b.trackIndex) return -1.;
    track* t = onTrack->trackData;
    if(b.sectionIndex < 0 || b.sectionIndex >= t->lSections.size()) return -1.;
    return t->getNumPoints(t->lSections[b.sectionIndex]) / F_HZ + b.time;
}

// ---------------------------------------------------------------------------
//  Persistence
// ---------------------------------------------------------------------------

QString projectMedia::sidecarFor(const QString& projectFile)
{
    QFileInfo fi(projectFile);
    QString base = fi.fileName();
    if(base.endsWith(".fvd", Qt::CaseInsensitive)) base.chop(4);
    return fi.absoluteDir().filePath(base + ".timeline.json");
}

bool projectMedia::save(const QString& projectFile, QString* error)
{
    QString side = sidecarFor(projectFile);
    if(!hasMusic() && mBookmarks.isEmpty()) {
        // nothing to keep - don't leave an outdated sidecar behind
        if(QFile::exists(side)) QFile::remove(side);
        return true;
    }

    QJsonObject root;
    root["format"] = QString("FVD++ timeline");
    root["version"] = 1;

    QJsonObject music;
    if(hasMusic()) {
        QDir projectDir = QFileInfo(projectFile).absoluteDir();
        music["file"] = projectDir.relativeFilePath(mMusicFile);
        music["absolute"] = mMusicFile;
    }
    music["start"] = mMusicStart;
    root["music"] = music;

    QJsonArray marks;
    for(int i = 0; i < mBookmarks.size(); ++i) {
        const bookmark& b = mBookmarks[i];
        QJsonObject o;
        o["name"] = b.name;
        o["kind"] = b.kind == bookmark::Music ? QString("music") : QString("track");
        o["time"] = b.time;
        if(b.kind == bookmark::Track) {
            o["track"] = b.trackIndex;
            o["section"] = b.sectionIndex;
        }
        marks.append(o);
    }
    root["bookmarks"] = marks;

    QFile f(side);
    if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if(error) *error = QString("Could not write %1").arg(side);
        return false;
    }
    f.write(QJsonDocument(root).toJson());
    return true;
}

void projectMedia::reset()
{
    clearMusic();
    mMusicStart = 0.;
    mCursor = 0.;
    mBookmarks.clear();
    emit musicChanged();
    emit bookmarksChanged();
}

void projectMedia::load(const QString& projectFile)
{
    reset();
    QFile f(sidecarFor(projectFile));
    if(!f.open(QIODevice::ReadOnly)) return;
    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    if(root["format"].toString() != "FVD++ timeline") return;

    QJsonObject music = root["music"].toObject();
    mMusicStart = music["start"].toDouble(0.);
    QString rel = music["file"].toString();
    QString abs = music["absolute"].toString();
    if(!rel.isEmpty() || !abs.isEmpty()) {
        QString candidate = QFileInfo(projectFile).absoluteDir().filePath(rel);
        if(rel.isEmpty() || !QFile::exists(candidate)) candidate = abs;
        QString err;
        if(!loadMusic(candidate, &err)) qWarning() << "Music of the project not found:" << candidate << err;
    }

    QJsonArray marks = root["bookmarks"].toArray();
    for(int i = 0; i < marks.size(); ++i) {
        QJsonObject o = marks[i].toObject();
        bookmark b;
        b.name = o["name"].toString();
        b.kind = o["kind"].toString() == "track" ? bookmark::Track : bookmark::Music;
        b.time = o["time"].toDouble();
        b.trackIndex = o["track"].toInt(0);
        b.sectionIndex = o["section"].toInt(0);
        mBookmarks.append(b);
    }
    emit musicChanged();
    emit bookmarksChanged();
}
