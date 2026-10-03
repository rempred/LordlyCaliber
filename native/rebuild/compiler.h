#pragma once
#include "engine/project.h"
#include <functional>

namespace lordly {
class Compiler {
  public:
    explicit Compiler(QString toolsRoot);
    QByteArray compile(const QByteArray &source, const QString &directory, const QString &name,
                       quint32 address, const QString &section) const;
    QByteArray compileData(const QByteArray &bytes, const QString &directory,
                           const QString &name) const;
    void authenticate() const;
    QByteArray assembleOwner(const QByteArray &source, const QString &directory,
                             const QString &name, quint32 address) const;

  private:
    void run(const QString &tool, const QStringList &args, const QString &directory) const;
    QString root_;
};
struct BuildResult {
    QByteArray image;
    QStringList report;
};
BuildResult rebuild(const Project &project, const QString &toolsRoot, const QString &workDirectory);
void exportRom(const Project &project, const BuildResult &build, const QString &path);
} // namespace lordly
