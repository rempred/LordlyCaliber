#pragma once
#include <QByteArray>
namespace lordly {
struct AcceptedRomCoverage {
    int codeRegions = 0;
    int dataRegions = 0;
    int retainedRegions = 0;
};
// The application build compiles and authenticates the complete accepted C
// library. Exports reuse that baseline, retaining project/foreign modifications.
AcceptedRomCoverage applyAcceptedRom(QByteArray &image);
}
