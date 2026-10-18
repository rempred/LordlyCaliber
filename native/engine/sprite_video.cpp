#include "engine/sprite_video.h"
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QPainter>
#include <QSaveFile>
#include <QTemporaryDir>
#include <algorithm>
#include <cstring>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <windows.h>
#include <wrl/client.h>
#endif
namespace lordly {
SpriteVideoPlan spriteVideoPlan(const SpriteAsset &asset, const SpriteVideoOptions &options) {
    asset.validate();
    const bool alpha = options.format == SpriteVideoFormat::AnimatedPng || options.format == SpriteVideoFormat::Webm;
    if ((!alpha && options.format != SpriteVideoFormat::Mp4) || options.scale < 1 ||
        options.scale > 16 || options.repeats < (options.format == SpriteVideoFormat::AnimatedPng ? 0 : 1) || options.repeats > 100 ||
        (!alpha && (!options.background.isValid() || options.background.alpha() != 255)))
        throw Error("Invalid animation export scale, play count, format or background");
    SpriteVideoPlan plan;
    // Use a small compatible video canvas, retaining source scale. The Windows
    // pipeline rejects tiny frames; larger odd dimensions need chroma padding.
    plan.size = {std::max(64, (asset.size.width() * options.scale + 1) & ~1),
                 std::max(64, (asset.size.height() * options.scale + 1) & ~1)};
    if (alpha)
        plan.size = asset.size * options.scale;
    if (plan.size.width() > 4096 || plan.size.height() > 4096)
        throw Error("Video dimensions must not exceed 4096 pixels per side");
    for (const auto &frame : asset.frames)
        plan.ticks += std::max(1, frame.ticks);
    plan.ticks *= std::max(1, options.repeats);
    if (plan.ticks > 30 * 60 * 10)
        throw Error("Sprite video exports are limited to ten minutes");
    return plan;
}
#ifdef Q_OS_WIN
namespace {
using Microsoft::WRL::ComPtr;
void checked(HRESULT hr, const char *operation) {
    if (FAILED(hr))
        throw Error(QString("Video export: %1 failed (0x%2)")
                        .arg(QString::fromLatin1(operation))
                        .arg(quint32(hr), 8, 16, QChar('0')));
}
QString systemLibrary(const wchar_t *name) {
    wchar_t folder[MAX_PATH + 1];
    const UINT length = GetSystemDirectoryW(folder, MAX_PATH + 1);
    if (!length || length > MAX_PATH)
        throw Error("Cannot locate Windows video components");
    return QString::fromWCharArray(folder) + '/' + QString::fromWCharArray(name);
}
template <class T> T entry(QLibrary &library, const char *name) {
    const auto pointer = library.resolve(name);
    if (!pointer)
        throw Error("Windows video components are unavailable: " + library.errorString());
    return reinterpret_cast<T>(pointer);
}
// Resolve video entry points only when exporting. Missing optional Windows media
// components must not prevent the editor or its other workspaces from starting.
struct MediaApi {
    QLibrary platform{systemLibrary(L"mfplat.dll")}, readWrite{systemLibrary(L"mfreadwrite.dll")};
    decltype(&MFStartup) startup = entry<decltype(&MFStartup)>(platform, "MFStartup");
    decltype(&MFShutdown) shutdown = entry<decltype(&MFShutdown)>(platform, "MFShutdown");
    decltype(&MFCreateMediaType) mediaType =
        entry<decltype(&MFCreateMediaType)>(platform, "MFCreateMediaType");
    decltype(&MFCreateAttributes) attributes =
        entry<decltype(&MFCreateAttributes)>(platform, "MFCreateAttributes");
    decltype(&MFCreateMemoryBuffer) buffer =
        entry<decltype(&MFCreateMemoryBuffer)>(platform, "MFCreateMemoryBuffer");
    decltype(&MFCreateSample) sample = entry<decltype(&MFCreateSample)>(platform, "MFCreateSample");
    decltype(&MFCreateSinkWriterFromURL) writer =
        entry<decltype(&MFCreateSinkWriterFromURL)>(readWrite, "MFCreateSinkWriterFromURL");
};
struct ComSession {
    bool owned = false;
    ComSession() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr != RPC_E_CHANGED_MODE)
            checked(hr, "initialize COM");
        owned = SUCCEEDED(hr);
    }
    ~ComSession() {
        if (owned)
            CoUninitialize();
    }
};
struct MediaSession {
    MediaApi &api;
    explicit MediaSession(MediaApi &value) : api(value) {
        checked(api.startup(MF_VERSION, MFSTARTUP_FULL), "start media services");
    }
    ~MediaSession() {
        api.shutdown();
    }
};
void format(IMFMediaType *type, REFGUID encoding, QSize size) {
    checked(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "set media type");
    checked(type->SetGUID(MF_MT_SUBTYPE, encoding), "set video encoding");
    checked(type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive),
            "set progressive video");
    checked(MFSetAttributeSize(type, MF_MT_FRAME_SIZE, UINT32(size.width()), UINT32(size.height())),
            "set video size");
    checked(MFSetAttributeRatio(type, MF_MT_FRAME_RATE, 30, 1), "set video rate");
    checked(MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "set square pixels");
}
} // namespace
#endif
bool exportSpriteVideo(const SpriteAsset &asset, const QString &path,
                       const SpriteVideoOptions &options,
                       const std::function<bool(int, int)> &progress) {
    const auto plan = spriteVideoPlan(asset, options);
    if (path.isEmpty())
        throw Error("Choose a video destination");
    if (progress && !progress(0, plan.ticks))
        return false;
    if (options.format == SpriteVideoFormat::AnimatedPng)
        return exportSpriteAnimatedPng(asset, path, options, progress);
    if (options.format == SpriteVideoFormat::Webm)
        return exportSpriteWebm(asset, path, options, progress);
#ifdef Q_OS_WIN
    ComSession com;
    MediaApi api;
    MediaSession media(api);
    QTemporaryDir temporary(QFileInfo(path).absoluteDir().filePath(".lordly-video-XXXXXX"));
    if (!temporary.isValid())
        throw Error("Cannot create temporary video directory: " + temporary.errorString());
    const auto intermediate = temporary.filePath("animation.mp4");
    ComPtr<IMFAttributes> attributes;
    checked(api.attributes(&attributes, 1), "create writer settings");
    checked(attributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE), "set export pacing");
    ComPtr<IMFSinkWriter> writer;
    const auto temporaryPath = QDir::toNativeSeparators(intermediate).toStdWString();
    checked(api.writer(temporaryPath.c_str(), nullptr, attributes.Get(), &writer),
            "create MP4 writer");
    ComPtr<IMFMediaType> output, input;
    checked(api.mediaType(&output), "create output format");
    format(output.Get(), MFVideoFormat_H264, plan.size);
    const auto bitrate = std::clamp<qint64>(qint64(plan.size.width()) * plan.size.height() * 30 * 2,
                                            1000000, 40000000);
    checked(output->SetUINT32(MF_MT_AVG_BITRATE, UINT32(bitrate)), "set video quality");
    DWORD stream = 0;
    checked(writer->AddStream(output.Get(), &stream), "add video stream");
    checked(api.mediaType(&input), "create input format");
    format(input.Get(), MFVideoFormat_RGB32, plan.size);
    checked(input->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(plan.size.width() * 4)),
            "set image stride");
    checked(writer->SetInputMediaType(stream, input.Get(), nullptr), "configure video converter");
    checked(writer->BeginWriting(), "begin video encoding");
    int completed = 0;
    for (int repeat = 0; repeat < options.repeats; ++repeat)
        for (int frame = 0; frame < asset.frames.size(); ++frame) {
            if (progress && !progress(completed, plan.ticks))
                return false;
            QImage image(plan.size, QImage::Format_RGB32);
            image.fill(options.background);
            {
                QPainter painter(&image);
                painter.drawImage(QRect(QPoint(), asset.size * options.scale),
                                  asset.composite(frame));
            }
            const DWORD bytes = DWORD(image.sizeInBytes());
            for (int hold = 0; hold < std::max(1, asset.frames[frame].ticks); ++hold) {
                if (progress && !progress(completed, plan.ticks))
                    return false;
                ComPtr<IMFMediaBuffer> buffer;
                checked(api.buffer(bytes, &buffer), "allocate video sample");
                BYTE *data = nullptr;
                checked(buffer->Lock(&data, nullptr, nullptr), "lock video sample");
                std::memcpy(data, image.constBits(), bytes);
                checked(buffer->Unlock(), "unlock video sample");
                checked(buffer->SetCurrentLength(bytes), "set video sample size");
                ComPtr<IMFSample> sample;
                checked(api.sample(&sample), "create video sample");
                checked(sample->AddBuffer(buffer.Get()), "attach video pixels");
                const LONGLONG start = LONGLONG(completed) * 10000000 / 30;
                const LONGLONG end = LONGLONG(completed + 1) * 10000000 / 30;
                checked(sample->SetSampleTime(start), "set video timestamp");
                checked(sample->SetSampleDuration(end - start), "set video duration");
                checked(writer->WriteSample(stream, sample.Get()), "encode video sample");
                ++completed;
            }
        }
    if (progress && !progress(completed, plan.ticks))
        return false;
    checked(writer->Finalize(), "finalize video");
    writer.Reset();
    if (progress && !progress(completed, plan.ticks))
        return false;
    QFile encoded(intermediate);
    QSaveFile destination(path);
    if (!encoded.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly))
        throw Error("Cannot save the completed video");
    while (!encoded.atEnd()) {
        if (progress && !progress(completed, plan.ticks))
            return false;
        const auto bytes = encoded.read(1024 * 1024);
        if (bytes.isEmpty() || destination.write(bytes) != bytes.size())
            throw Error("Cannot write the completed video");
    }
    if (progress && !progress(completed, plan.ticks))
        return false;
    if (!destination.commit())
        throw Error("Cannot commit video: " + destination.errorString());
    return true;
#else
    Q_UNUSED(options);
    throw Error("Video export requires Windows media services");
#endif
}
} // namespace lordly
