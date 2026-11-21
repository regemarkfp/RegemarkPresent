#pragma once

#include "core/Types.h"

#include <QHash>
#include <QImage>
#include <QObject>
#include <QTimer>

class QAudioOutput;
class QMediaPlayer;
class QVideoSink;

// Shared source of background pixels. One decoder per video file no matter
// how many screens show it, so Audience and Stream stay frame-locked.
// Players nobody has asked for in a few seconds are stopped.
class MediaPool : public QObject {
    Q_OBJECT
public:
    explicit MediaPool(QObject *parent = nullptr);
    ~MediaPool() override;

    QImage frame(const Background &bg);

    // Start a video from the top (a media cue being taken), with or without sound.
    void restart(const Background &bg);
    // Stop a cue's playback (and its sound) as soon as it leaves the screen.
    void stop(const Background &bg);

    // Single representative frame for thumbnails (first decoded frame of a video).
    QImage poster(const QString &path) const { return m_posters.value(path); }

signals:
    void posterReady(const QString &path);
    void videoFinished(const QString &path);   // a play-once video reached its end

private:
    struct Video {
        QMediaPlayer *player = nullptr;
        QVideoSink *sink = nullptr;
        QAudioOutput *audio = nullptr;
        QImage last;
        qint64 lastUsed = 0;
    };
    Video *video(const Background &bg);
    void reap();

    QHash<QString, Video *> m_videos;
    QHash<QString, QImage> m_images;
    QHash<QString, QImage> m_posters;
    QTimer m_reaper;
};
