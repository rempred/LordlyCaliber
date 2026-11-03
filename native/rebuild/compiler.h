#pragma once
#include "engine/project.h"
#include <functional>

namespace lordly {
// Delivered on the build's calling thread. Consumers must marshal UI updates.
// Stages measure completed work, not an estimate of remaining time.
struct BuildProgress {
    static constexpr int stageCount = 8;
    int stage = 0;
    QString title;
    QString activity;
    QStringList messages;
    int completed = 0;
    int total = 0; // Zero means the current operation has no measurable total.
};
using BuildProgressCallback = std::function<void(const BuildProgress &)>;
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
struct CompiledOwner {
    QByteArray code;
    QMap<quint32, QByteArray> tables;
};
class Compiler {
  public:
    explicit Compiler(QString toolsRoot, std::function<void(const QString &, int, int)> activity = {});
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
    // Freshly compile an authenticated original C owner at its exact game address.
    // Small, separately bounded mod hooks may then be applied by the caller.
    QByteArray compileAcceptedOwner(quint32 romStart, const QString &directory,
                                    const QString &name) const;
    // Maintained C adaptation with qualified original allocation and table layout.
    CompiledOwner compileQualifiedOwner(const QJsonObject &owner, const QString &directory,
                                        const QString &name) const;
    QByteArray assembleOwner(const QByteArray &source, const QString &directory,
                             const QString &name, quint32 address,
                             const QMap<QString, quint32> &symbols = {},
                             bool preserveInstructionPositions = false) const;

  private:
    QByteArray compileAtLayout(const QByteArray &source, const QString &directory,
                               const QString &name, quint32 address, const QString &section,
                               const QMap<QString, QByteArray> &headers,
                               const QMap<QString, quint32> &symbols, bool inlineReadOnlyData,
                               const QJsonObject &fixedLayout) const;
    void run(const QString &tool, const QStringList &args, const QString &directory) const;
    QString root_;
    std::function<void(const QString &, int, int)> activity_;
};
struct BuildResult {
    QByteArray image;
    QStringList report;
};
BuildResult rebuild(const Project &project, const QString &toolsRoot, const QString &workDirectory,
                    const BuildProgressCallback &progress = {});
void exportRom(const Project &project, const BuildResult &build, const QString &path,
               const BuildProgressCallback &progress = {});
} // namespace lordly
