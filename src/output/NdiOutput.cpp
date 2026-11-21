#include "output/NdiOutput.h"

#include <QDir>
#include <QLibrary>
#include <QMutex>
#include <cstdint>
#include <limits>

// Minimal declarations of the public NDI C ABI (see the NDI SDK's
// Processing.NDI.*.h). Only the handful of entry points used here.
namespace ndi {

struct SendCreate {
    const char *name;
    const char *groups;
    bool clockVideo;
    bool clockAudio;
};

struct VideoFrameV2 {
    int xres, yres;
    uint32_t fourCC;
    int frameRateN, frameRateD;
    float aspect;
    int frameFormat;          // 1 = progressive
    int64_t timecode;
    uint8_t *data;
    int lineStride;
    const char *metadata;
    int64_t timestamp;
};

constexpr uint32_t fourcc(char a, char b, char c, char d)
{
    return uint32_t(uint8_t(a)) | uint32_t(uint8_t(b)) << 8 | uint32_t(uint8_t(c)) << 16 | uint32_t(uint8_t(d)) << 24;
}

using InitializeFn = bool (*)();
using SendCreateFn = void *(*)(const SendCreate *);
using SendDestroyFn = void (*)(void *);
using SendVideoFn = void (*)(void *, const VideoFrameV2 *);

struct Api {
    bool loaded = false;
    QString error;
    SendCreateFn sendCreate = nullptr;
    SendDestroyFn sendDestroy = nullptr;
    SendVideoFn sendVideo = nullptr;
};

static Api &api()
{
    static QMutex mutex;
    static Api a;
    static bool tried = false;
    QMutexLocker lock(&mutex);
    if (tried) return a;
    tried = true;

    QStringList candidates;
#if defined(Q_OS_WIN)
    for (const char *var : {"NDI_RUNTIME_DIR_V6", "NDI_RUNTIME_DIR_V5"}) {
        const QString dir = qEnvironmentVariable(var);
        if (!dir.isEmpty()) candidates << QDir(dir).filePath("Processing.NDI.Lib.x64.dll");
    }
    candidates << "Processing.NDI.Lib.x64.dll";
#elif defined(Q_OS_MACOS)
    candidates << "/usr/local/lib/libndi.dylib" << "libndi.dylib";
#else
    candidates << "libndi.so.6" << "libndi.so.5" << "libndi.so";
#endif

    static QLibrary lib;
    for (const QString &c : candidates) {
        lib.setFileName(c);
        if (lib.load()) break;
    }
    if (!lib.isLoaded()) {
        a.error = QObject::tr("NDI runtime not installed (get NDI Tools / Runtime from ndi.video)");
        return a;
    }
    auto init = reinterpret_cast<InitializeFn>(lib.resolve("NDIlib_initialize"));
    a.sendCreate = reinterpret_cast<SendCreateFn>(lib.resolve("NDIlib_send_create"));
    a.sendDestroy = reinterpret_cast<SendDestroyFn>(lib.resolve("NDIlib_send_destroy"));
    a.sendVideo = reinterpret_cast<SendVideoFn>(lib.resolve("NDIlib_send_send_video_v2"));
    if (!init || !a.sendCreate || !a.sendDestroy || !a.sendVideo) {
        a.error = QObject::tr("NDI runtime is missing expected functions");
        return a;
    }
    if (!init()) {
        a.error = QObject::tr("NDI could not initialise (unsupported CPU?)");
        return a;
    }
    a.loaded = true;
    return a;
}

}

NdiOutput::NdiOutput(const QString &sourceName, double fps, bool alpha)
    : m_name(sourceName), m_alpha(alpha)
{
    // 29.97 / 59.94 style rates need the 1001 denominator.
    const double rounded = qRound(fps);
    if (qAbs(fps - rounded) > 0.01) {
        m_rateN = int(rounded * 1000);
        m_rateD = 1001;
    } else {
        m_rateN = int(rounded) * 1000;
        m_rateD = 1000;
    }
    setStatus(QObject::tr("NDI \"%1\" starting").arg(m_name));
    start();
}

NdiOutput::~NdiOutput()
{
    stopWorker();
}

bool NdiOutput::runtimeAvailable(QString *error)
{
    const ndi::Api &a = ndi::api();
    if (error) *error = a.error;
    return a.loaded;
}

bool NdiOutput::open()
{
    const ndi::Api &a = ndi::api();
    if (!a.loaded) {
        setStatus(a.error);
        return false;
    }
    const QByteArray name = m_name.toUtf8();
    ndi::SendCreate desc{name.constData(), nullptr, false, false};
    m_sender = a.sendCreate(&desc);
    if (!m_sender) {
        setStatus(QObject::tr("NDI \"%1\" could not be created").arg(m_name));
        return false;
    }
    setStatus(QObject::tr("NDI \"%1\" live").arg(m_name));
    return true;
}

void NdiOutput::sendFrame(const QImage &frame)
{
    // NDI wants straight (non-premultiplied) BGRA; QImage ARGB32 is BGRA in
    // memory on little-endian machines.
    const QImage img = m_alpha ? frame.convertToFormat(QImage::Format_ARGB32)
                               : frame.convertToFormat(QImage::Format_RGB32);
    ndi::VideoFrameV2 f{};
    f.xres = img.width();
    f.yres = img.height();
    f.fourCC = m_alpha ? ndi::fourcc('B', 'G', 'R', 'A') : ndi::fourcc('B', 'G', 'R', 'X');
    f.frameRateN = m_rateN;
    f.frameRateD = m_rateD;
    f.aspect = float(img.width()) / img.height();
    f.frameFormat = 1;
    f.timecode = std::numeric_limits<int64_t>::max(); // let NDI synthesise
    f.data = const_cast<uint8_t *>(img.constBits());
    f.lineStride = int(img.bytesPerLine());
    ndi::api().sendVideo(m_sender, &f);
}

void NdiOutput::close()
{
    if (m_sender) ndi::api().sendDestroy(m_sender);
    m_sender = nullptr;
}
