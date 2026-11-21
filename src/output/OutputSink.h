#pragma once

#include <QImage>
#include <QString>

// Anything a rendered screen can be sent to. pushFrame() is called on the UI
// thread at the screen's frame rate and must not block.
class OutputSink {
public:
    virtual ~OutputSink() = default;
    virtual void pushFrame(const QImage &frame) = 0;
    virtual QString status() const = 0;
};
