#include "render/MediaPool.h"

#include <QAudioOutput>
#include <QDateTime>
#include <QMediaPlayer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

static constexpr int kMaxImageSide = 3840;
static constexpr int kMaxCachedImages = 48;
static constexpr qint64 kIdleMs = 4000;

MediaPool::MediaPool(QObject *parent) : QObject(parent)
{
    connect(&m_reaper, &QTimer::timeout, this, &MediaPool::reap);
    m_reaper.start(1000);
}

MediaPool::~MediaPool()
{
    for (Video *v : std::as_const(m_videos)) {
        v->player->stop();
        delete v->player;
        delete v->sink;
        delete v->audio;
        delete v;
    }
}

QImage MediaPool::frame(const Background &bg)
{
    switch (bg.type) {
    case Background::Type::Image: {
        auto it = m_images.constFind(bg.path);
        if (it != m_images.constEnd()) return it.value();
        QImage img(bg.path);
        if (img.width() > kMaxImageSide || img.height() > kMaxImageSide)
            img = img.scaled(kMaxImageSide, kMaxImageSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        if (m_images.size() >= kMaxCachedImages) m_images.clear();
        m_images.insert(bg.path, img);
        return img;
    }
    case Background::Type::Video: {
        Video *v = video(bg);
        v->lastUsed = QDateTime::currentMSecsSinceEpoch();
        return v->last;
    }
    default:
        return {};
    }
}

MediaPool::Video *MediaPool::video(const Background &bg)
{
    if (Video *v = m_videos.value(bg.path)) {
        if (bg.audio && !v->audio) {
            v->audio = new QAudioOutput;
            v->player->setAudioOutput(v->audio);
        }
        return v;
    }

    auto *v = new Video;
    v->player = new QMediaPlayer;
    v->sink = new QVideoSink;
    v->player->setVideoOutput(v->sink);
    if (bg.audio) {   // backgrounds stay silent; media cues may play sound
        v->audio = new QAudioOutput;
        v->player->setAudioOutput(v->audio);
    }
    v->player->setLoops(bg.loop ? QMediaPlayer::Infinite : 1);
    const QString path = bg.path;
    connect(v->sink, &QVideoSink::videoFrameChanged, this, [this, v, path](const QVideoFrame &f) {
        if (!f.isValid()) return;
        v->last = f.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
        if (!m_posters.contains(path)) {
            m_posters.insert(path, v->last.scaled(480, 270, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            emit posterReady(path);
        }
    });
    connect(v->player, &QMediaPlayer::mediaStatusChanged, this, [this, path](QMediaPlayer::MediaStatus s) {
        if (s == QMediaPlayer::EndOfMedia) emit videoFinished(path);
    });
    v->player->setSource(QUrl::fromLocalFile(bg.path));
    v->player->play();
    m_videos.insert(bg.path, v);
    return v;
}

void MediaPool::restart(const Background &bg)
{
    if (bg.type != Background::Type::Video) return;
    Video *v = video(bg);
    v->lastUsed = QDateTime::currentMSecsSinceEpoch();
    v->player->setLoops(bg.loop ? QMediaPlayer::Infinite : 1);
    v->player->setPosition(0);
    v->player->play();
}

void MediaPool::stop(const Background &bg)
{
    if (Video *v = m_videos.value(bg.path)) v->player->pause();
}

void MediaPool::reap()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_videos.begin(); it != m_videos.end();) {
        Video *v = it.value();
        if (now - v->lastUsed > kIdleMs) {
            v->sink->disconnect(this);
            v->player->stop();
            v->player->deleteLater();
            v->sink->deleteLater();
            if (v->audio) v->audio->deleteLater();
            delete v;
            it = m_videos.erase(it);
        } else {
            ++it;
        }
    }
}
