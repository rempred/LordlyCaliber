#include "app/sound_output.h"
#include "engine/rom.h"
#include <QTimer>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#endif
namespace lordly {
struct SoundOutput::Impl {
    QByteArray pcm;
    QTimer *timer = nullptr;
    qint64 finishedMs = 0;
#ifdef Q_OS_WIN
    HWAVEOUT device = nullptr;
    WAVEHDR header{};
#endif
};
SoundOutput::SoundOutput(QObject *parent) : QObject(parent), impl_(std::make_unique<Impl>()) {
    impl_->timer = new QTimer(this);
    impl_->timer->setInterval(50);
    connect(impl_->timer, &QTimer::timeout, this, [this] {
#ifdef Q_OS_WIN
        if (impl_->header.dwFlags & WHDR_DONE) {
            const auto duration = impl_->pcm.size() / 128;
            stop();
            impl_->finishedMs = duration;
            if (finished)
                finished();
        }
#endif
    });
}
SoundOutput::~SoundOutput() {
    stop();
}
qsizetype SoundOutput::frameCount() const { return impl_->pcm.size() / 4; }
void SoundOutput::stop() {
    impl_->timer->stop();
#ifdef Q_OS_WIN
    if (impl_->device) {
        waveOutReset(impl_->device);
        if (impl_->header.dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(impl_->device, &impl_->header, sizeof(WAVEHDR));
        waveOutClose(impl_->device);
        impl_->device = nullptr;
        impl_->header = {};
    }
#endif
    impl_->pcm.clear();
    impl_->finishedMs = 0;
}
qint64 SoundOutput::positionMs() const {
#ifdef Q_OS_WIN
    if (impl_->device) {
        MMTIME time{}; time.wType = TIME_SAMPLES;
        if (waveOutGetPosition(impl_->device, &time, sizeof(time)) == MMSYSERR_NOERROR) {
            if (time.wType == TIME_SAMPLES) return qint64(time.u.sample) * 1000 / 32000;
            if (time.wType == TIME_BYTES) return time.u.cb / 128;
            if (time.wType == TIME_MS) return time.u.ms;
        }
    }
#endif
    return impl_->finishedMs;
}
void SoundOutput::play(QByteArray pcm) {
    stop();
    if (pcm.isEmpty() || pcm.size() % 4)
        throw Error("Sound preview contains no complete audio frames");
#ifdef Q_OS_WIN
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = 32000;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 4;
    format.nAvgBytesPerSec = 128000;
    if (waveOutOpen(&impl_->device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        throw Error("Windows could not open an audio output device");
    impl_->pcm = std::move(pcm);
    impl_->header.lpData = impl_->pcm.data();
    impl_->header.dwBufferLength = DWORD(impl_->pcm.size());
    if (waveOutPrepareHeader(impl_->device, &impl_->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR ||
        waveOutWrite(impl_->device, &impl_->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
        stop();
        throw Error("Windows could not play this sound preview");
    }
    impl_->timer->start();
#else
    throw Error("Sound output is currently available in the Windows editor");
#endif
}
} // namespace lordly
