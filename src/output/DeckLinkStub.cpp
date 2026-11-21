#include "output/DeckLinkOutput.h"
#include "output/OutputSink.h"

#include <QObject>

namespace {
class UnavailableSink : public OutputSink {
public:
    void pushFrame(const QImage &) override {}
    QString status() const override
    {
        return QObject::tr("DeckLink: this build has no DeckLink support (rebuild with DECKLINK_SDK_DIR)");
    }
};
}

QStringList deckLinkDevices(QString *error)
{
    if (error) *error = QObject::tr("Built without the DeckLink SDK");
    return {};
}

OutputSink *createDeckLinkOutput(int, QSize, double, bool)
{
    return new UnavailableSink;
}
