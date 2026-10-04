#include "engine/scene_camera.h"
#include <bit>
#include <cmath>
#include <numbers>
namespace lordly {
namespace {
float real(const QByteArray &b, int at) {
    return std::bit_cast<float>(u32(b, at));
}
SceneVector vector(const QByteArray &b, int at) {
    return {real(b, at), real(b, at + 4), real(b, at + 8)};
}
bool finite(SceneVector v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
double dot(SceneVector a, SceneVector b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
SceneVector cross(SceneVector a, SceneVector b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
} // namespace
double SceneVector::length() const {
    return std::sqrt(dot(*this, *this));
}
SceneCamera::SceneCamera(QByteArray bank, float scale)
    : bank_(std::move(bank)), modelScale_(scale) {
    prepare();
}
void SceneCamera::prepare() {
    if (bank_.size() != 56 || !std::isfinite(modelScale_) || modelScale_ <= 0)
        throw Error("Camera requires its fourteen native floats and positive model scale");
    for (int i = 0; i < 14; ++i)
        if (!std::isfinite(real(bank_, i * 4)))
            throw Error("Camera contains nonfinite fields");
    const auto eye = vector(bank_, 20), target = vector(bank_, 32), up = vector(bank_, 44);
    const float fov = real(bank_, 0), aspect = real(bank_, 4);
    near_ = real(bank_, 8);
    far_ = real(bank_, 12);
    if (fov <= 0 || fov >= 179 || aspect <= 0 || near_ <= 0 || far_ <= near_ ||
        real(bank_, 16) <= 0 || (eye - target).length() < 1e-6 ||
        cross(up, eye - target).length() < 1e-6)
        throw Error("Camera view or clipping planes are degenerate");
    eye_ = eye;
    aspect_ = aspect;
    back_ = (eye - target) / (eye - target).length();
    right_ = cross(up, back_);
    right_ = right_ / right_.length();
    up_ = cross(back_, right_);
    focal_ = float(120.0 / std::tan(double(fov) * std::numbers::pi / 360.0));
}
void SceneCamera::select(const QVector<quint32> &args) {
    if (args.size() != 7)
        throw Error("Camera command is truncated");
    auto bank = bank_;
    // Native order: target XYZ, eye XYZ, field of view, each divided by 1000.
    constexpr int offsets[] = {32, 36, 40, 20, 24, 28, 0};
    for (int i = 0; i < 7; ++i)
        put32(bank, offsets[i], std::bit_cast<quint32>(float(qint32(args[i])) / 1000.f));
    *this = SceneCamera(std::move(bank), modelScale_);
}
CameraPoint SceneCamera::projectWorld(SceneVector world) const {
    if (!finite(world))
        throw Error("Cannot project a nonfinite coordinate");
    const auto relative = world - eye_;
    const double depth = -dot(relative, back_);
    CameraPoint result;
    result.depth = float(depth);
    if (std::abs(depth) < 1e-8)
        return result;
    result.screen = {160.0 + dot(relative, right_) * (double(focal_) * 4.0 / 3.0) / aspect_ / depth,
                     120.0 - dot(relative, up_) * focal_ / depth};
    if (!std::isfinite(result.screen.x()) || !std::isfinite(result.screen.y()))
        throw Error("Camera projection overflow");
    result.visible = result.depth >= near_ && result.depth <= far_;
    if (result.depth > 0)
        result.pixelsPerUnit = modelScale_ * focal_ / result.depth;
    return result;
}
CameraPoint SceneCamera::project(SceneVector point) const {
    return projectWorld(point * modelScale_);
}
CameraPoint SceneCamera::projectPackedOrigin(const QByteArray &matrix) const {
    if (matrix.size() != 64)
        throw Error("Native packed matrix must contain 64 bytes");
    auto component = [&](int i) {
        return float(qint32(quint32(u16(matrix, i * 2)) << 16 | u16(matrix, 32 + i * 2))) / 65536.f;
    };
    return projectWorld({component(12), component(13), component(14)});
}
SceneVector SceneCamera::unprojectAtHeight(QPointF screen, double height) const {
    if (!std::isfinite(screen.x()) || !std::isfinite(screen.y()) || !std::isfinite(height))
        throw Error("Invalid screen coordinate");
    const auto direction =
        right_ * ((screen.x() - 160.0) * aspect_ / (double(focal_) * 4.0 / 3.0)) +
        up_ * ((120.0 - screen.y()) / focal_) - back_;
    if (std::abs(direction.y) < 1e-8)
        throw Error("View ray is parallel to the selected height plane");
    const auto result =
        (eye_ + direction * ((height * modelScale_ - eye_.y) / direction.y)) / modelScale_;
    if (!finite(result))
        throw Error("Unprojected coordinate overflow");
    return result;
}
SceneCameras::SceneCameras(SceneCamera actor, SceneCamera registered, SceneCamera shared)
    : actor_(std::move(actor)), registered_(std::move(registered)), shared_(std::move(shared)) {}
SceneCameras SceneCameras::freshModeTwo(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Scene camera defaults require US revision zero");
    auto actor = rom.bytes().mid(0x2866f0, 56);
    const auto registered = rom.bytes().mid(0x286748, 56);
    // Mode-two shared slab: ROM1EFA4C -> RAM801CE8CC. Its combat counterpart
    // ROM211D5C contains the same retail defaults but is a different load owner.
    for (int i = 0; i < 6; ++i)
        put32(actor, 20 + i * 4, u32(rom.bytes(), 0x1efa4c + i * 4));
    put32(actor, 0, u32(rom.bytes(), 0x1efa68));
    const auto scale = real(rom.bytes(), 0x1efa64);
    return {SceneCamera(actor, scale), SceneCamera(registered, scale), SceneCamera(actor, scale)};
}
bool SceneCameras::service(int mode, quint32 opcode, const QVector<quint32> &args) {
    if (opcode != 0x35 && opcode != 0x36)
        return false;
    auto next = *this;
    if (opcode == 0x35)
        next.registered_.select(args);
    else
        next.actor_.select(args);
    if (opcode == 0x35 || mode == 2)
        next.shared_.select(args);
    *this = std::move(next);
    return true;
}
} // namespace lordly
