#include "output/DeckLinkOutput.h"

#include "output/FrameWorker.h"

#include <QObject>
#include <cstring>

#if defined(_WIN32)
#  include <windows.h>
#  include "DeckLinkAPI_h.h"
#else
#  include "DeckLinkAPI.h"
#endif
#include "DeckLinkAPIVersion.h"

namespace {

struct ComScope {
#if defined(_WIN32)
    ComScope() { CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
    ~ComScope() { CoUninitialize(); }
#endif
};

IDeckLinkIterator *createIterator()
{
    IDeckLinkIterator *it = nullptr;
#if defined(_WIN32)
    if (CoCreateInstance(CLSID_CDeckLinkIterator, nullptr, CLSCTX_ALL, IID_IDeckLinkIterator,
                         reinterpret_cast<void **>(&it)) != S_OK)
        return nullptr;
#else
    it = CreateDeckLinkIteratorInstance();
#endif
    return it;
}

QString displayName(IDeckLink *dl)
{
#if defined(_WIN32)
    BSTR name = nullptr;
    if (dl->GetDisplayName(&name) != S_OK) return {};
    const QString s = QString::fromWCharArray(name);
    SysFreeString(name);
    return s;
#else
    const char *name = nullptr;
    if (dl->GetDisplayName(&name) != S_OK) return {};
    const QString s = QString::fromUtf8(name);
    free(const_cast<char *>(name));
    return s;
#endif
}

IDeckLink *deviceAt(int index)
{
    IDeckLinkIterator *it = createIterator();
    if (!it) return nullptr;
    IDeckLink *dl = nullptr;
    for (int i = 0; it->Next(&dl) == S_OK; ++i) {
        if (i == index) break;
        dl->Release();
        dl = nullptr;
    }
    it->Release();
    return dl;
}

class DeckLinkSink : public FrameWorker {
public:
    DeckLinkSink(int device, QSize res, double fps, bool key)
        : m_device(device), m_res(res), m_fps(fps), m_key(key)
    {
        setStatus(QObject::tr("DeckLink #%1 starting").arg(device + 1));
        start();
    }
    ~DeckLinkSink() override { stopWorker(); }

protected:
    bool open() override
    {
        m_com = new ComScope;
        m_deckLink = deviceAt(m_device);
        if (!m_deckLink) return fail(QObject::tr("DeckLink #%1 not found (is Desktop Video installed?)").arg(m_device + 1));
        const QString name = displayName(m_deckLink);

        if (m_deckLink->QueryInterface(IID_IDeckLinkOutput, reinterpret_cast<void **>(&m_output)) != S_OK)
            return fail(QObject::tr("%1 has no output").arg(name));

        BMDDisplayMode mode = findMode();
        if (mode == bmdModeUnknown)
            return fail(QObject::tr("%1: no %2x%3 @ %4 mode").arg(name).arg(m_res.width()).arg(m_res.height()).arg(m_fps));

        if (m_output->EnableVideoOutput(mode, bmdVideoOutputFlagDefault) != S_OK)
            return fail(QObject::tr("%1 is busy or does not support this mode").arg(name));

        if (m_key && m_deckLink->QueryInterface(IID_IDeckLinkKeyer, reinterpret_cast<void **>(&m_keyer)) == S_OK) {
            m_keyer->Enable(true);   // external: fill + key on separate connectors
            m_keyer->SetLevel(255);
        }

        if (m_output->CreateVideoFrame(m_res.width(), m_res.height(), m_res.width() * 4, bmdFormat8BitBGRA,
                                       bmdFrameFlagDefault, &m_frame) != S_OK)
            return fail(QObject::tr("%1: could not allocate frame").arg(name));

        setStatus(QObject::tr("DeckLink %1 live%2").arg(name, m_keyer ? QObject::tr(" (fill+key)") : QString()));
        return true;
    }

    void sendFrame(const QImage &frame) override
    {
        QImage img = frame.size() == m_res ? frame : frame.scaled(m_res, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        void *bytes = nullptr;
#if BLACKMAGIC_DECKLINK_API_VERSION >= 0x0e030000
        // SDK 14.3+ moved pixel access to IDeckLinkVideoBuffer.
        IDeckLinkVideoBuffer *buf = nullptr;
        if (m_frame->QueryInterface(IID_IDeckLinkVideoBuffer, reinterpret_cast<void **>(&buf)) != S_OK) return;
        buf->StartAccess(bmdBufferAccessWrite);
        buf->GetBytes(&bytes);
#else
        m_frame->GetBytes(&bytes);
#endif
        const int rowBytes = m_res.width() * 4;
        for (int y = 0; y < m_res.height(); ++y)
            std::memcpy(static_cast<char *>(bytes) + y * rowBytes, img.constScanLine(y), rowBytes);
#if BLACKMAGIC_DECKLINK_API_VERSION >= 0x0e030000
        buf->EndAccess(bmdBufferAccessWrite);
        buf->Release();
#endif
        m_output->DisplayVideoFrameSync(m_frame);
    }

    void close() override
    {
        if (m_keyer) { m_keyer->Disable(); m_keyer->Release(); m_keyer = nullptr; }
        if (m_frame) { m_frame->Release(); m_frame = nullptr; }
        if (m_output) { m_output->DisableVideoOutput(); m_output->Release(); m_output = nullptr; }
        if (m_deckLink) { m_deckLink->Release(); m_deckLink = nullptr; }
        delete m_com;
        m_com = nullptr;
    }

private:
    bool fail(const QString &msg)
    {
        setStatus(msg);
        close();
        return false;
    }

    BMDDisplayMode findMode()
    {
        IDeckLinkDisplayModeIterator *it = nullptr;
        if (m_output->GetDisplayModeIterator(&it) != S_OK) return bmdModeUnknown;
        BMDDisplayMode best = bmdModeUnknown;
        double bestDiff = 0.05;
        IDeckLinkDisplayMode *mode = nullptr;
        while (it->Next(&mode) == S_OK) {
            BMDTimeValue duration;
            BMDTimeScale scale;
            mode->GetFrameRate(&duration, &scale);
            const double fps = double(scale) / double(duration);
            if (mode->GetWidth() == m_res.width() && mode->GetHeight() == m_res.height()
                && mode->GetFieldDominance() == bmdProgressiveFrame && qAbs(fps - m_fps) < bestDiff) {
                best = mode->GetDisplayMode();
                bestDiff = qAbs(fps - m_fps);
            }
            mode->Release();
        }
        it->Release();
        return best;
    }

    int m_device;
    QSize m_res;
    double m_fps;
    bool m_key;
    ComScope *m_com = nullptr;
    IDeckLink *m_deckLink = nullptr;
    IDeckLinkOutput *m_output = nullptr;
    IDeckLinkKeyer *m_keyer = nullptr;
    IDeckLinkMutableVideoFrame *m_frame = nullptr;
};

}

QStringList deckLinkDevices(QString *error)
{
    ComScope com;
    QStringList names;
    IDeckLinkIterator *it = createIterator();
    if (!it) {
        if (error) *error = QObject::tr("Blackmagic Desktop Video drivers not installed");
        return names;
    }
    IDeckLink *dl = nullptr;
    while (it->Next(&dl) == S_OK) {
        names << displayName(dl);
        dl->Release();
    }
    it->Release();
    return names;
}

OutputSink *createDeckLinkOutput(int device, QSize resolution, double fps, bool key)
{
    return new DeckLinkSink(device, resolution, fps, key);
}
