#pragma once
#include "engine/director.h"
#include <QJsonObject>
#include <memory>
namespace lordly {
// Compatibility compiler for browser SceneDocuments. Execution, resource allocation
// and rendering remain in the native engine. The scripts are embedded in the binary.
class SceneDocumentCompiler {
  public:
    static const QJsonObject &catalog();
    SceneDocumentCompiler();
    ~SceneDocumentCompiler();
    QByteArray compile(const DirectorProgram &source, const QJsonObject &document,
                       int launchMode = -1);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace lordly
