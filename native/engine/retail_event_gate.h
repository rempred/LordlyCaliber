#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <optional>
namespace lordly {
// Structural changes address the original comparison word. Existing code and
// relative destinations retain their byte addresses throughout a rebuild.
class RetailEventGate {
  public:
    static bool supports(const QByteArray &retail,int row,int comparisonOffset);
    static QByteArray compile(const QByteArray &compiled,const QByteArray &retail,
                              int row,const QJsonObject &gates);
    static bool needsRuntime(const QJsonObject &allRows);
    static int directory(const QByteArray &program);
    static std::optional<int> destination(const QByteArray &program,int offset);
    static int transferMode(const QByteArray &program,int offset);
    static int originalComparison(const QByteArray &program,int offset);
};
}
