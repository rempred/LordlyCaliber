#pragma once
#include <QImage>
namespace lordly {
// Working RGB for imported artwork. Untagged images are assumed to be sRGB.
// Convert the profile before reducing high-depth input to eight-bit channels.
QImage imageToSrgb(const QImage &);
} // namespace lordly
