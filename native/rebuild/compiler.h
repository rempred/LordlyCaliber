#pragma once
#include "engine/project.h"
#include <functional>

namespace lordly {
struct CompiledAddressReference {
    QString symbol;
    quint32 highOffset = 0, lowOffset = 0;
    qint32 addend = 0;
};
struct CompiledModule {
    QByteArray bytes;
    quint32 address = 0;
    QMap<QString, quint32> entries;
    quint32 functionExtent = 0;
    QVector<CompiledAddressReference> addressReferences;
    QByteArray fitOwner(qsizetype size) const;
};
class Compiler {
  public:
    explicit Compiler(QString toolsRoot);
    QByteArray compile(const QByteArray &source, const QString &directory, const QString &name,
                       quint32 address, const QString &section,
                       const QMap<QString, QByteArray> &headers = {},
                       const QMap<QString, quint32> &symbols = {},
                       bool inlineReadOnlyData = false) const;
    QByteArray compileData(const QByteArray &bytes, const QString &directory,
                           const QString &name) const;
    CompiledModule compileModule(const QByteArray &source, const QString &directory,
                                 const QString &name, quint32 address,
                                 const QMap<QString, QByteArray> &headers = {},
                                 const QMap<QString, quint32> &symbols = {},
                                 bool inlineReadOnlyData = false) const;
    void authenticate() const;
    QByteArray assembleOwner(const QByteArray &source, const QString &directory,
                             const QString &name, quint32 address,
                             const QMap<QString, quint32> &symbols = {},
                             bool preserveInstructionPositions = false) const;

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
