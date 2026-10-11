#include "rebuild/body_markers.h"
#include "engine/body_markers.h"
#include <QFile>
namespace lordly {
QByteArray rebuildBodyMarkers(const Rom &target, const Rom &source, bool wide,
                              const Compiler &compiler, const QString &directory) {
    const int before = BodyMarkerImage::mode(source);
    if (before < 0) {
        if (!wide)
            return target.bytes();
        throw Error("The equipment marker code is not a recognized source-built owner");
    }
    if (target.bytes().mid(BodyMarkerImage::offset, BodyMarkerImage::size) !=
        source.bytes().mid(BodyMarkerImage::offset, BodyMarkerImage::size))
        throw Error("Another export owner changed the equipment marker code");
    if (before == int(wide))
        return target.bytes();
    QFile file(":/game/art/body-markers.s");
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing equipment marker source");
    const auto bytes = compiler.assembleOwner(
        QString(".equ LC_WIDE,%1\n").arg(wide ? 1 : 0).toLatin1() + file.readAll(), directory,
        "body_markers", BodyMarkerImage::address,
        {{"lc_pose_widths", BodyMarkerImage::widthsAddress}}, true);
    if (bytes.size() != BodyMarkerImage::size)
        throw Error("Equipment marker code exceeds its original allocation");
    auto image = target.bytes();
    image.replace(BodyMarkerImage::offset, bytes.size(), bytes);
    if (BodyMarkerImage::mode(Rom::fromBytes(image)) != int(wide))
        throw Error("Compiled equipment marker code differs from its checked source");
    return image;
}
} // namespace lordly
