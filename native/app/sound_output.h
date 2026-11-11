#pragma once
#include <QByteArray>
#include <QObject>
#include <functional>
#include <memory>
namespace lordly {
class SoundOutput : public QObject {
  public:
    explicit SoundOutput(QObject *parent = nullptr);
    ~SoundOutput();
    void play(QByteArray pcm);
    void stop();
    qint64 positionMs() const;
    qsizetype frameCount() const;
    std::function<void()> finished;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace lordly
