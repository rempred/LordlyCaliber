#pragma once
#include "engine/rom.h"
#include <QMap>
#include <QStringList>
#include <QVector>

namespace lordly {
struct DirectorOpcode {
    quint32 opcode = 0;
    int words = 0;
    QString name, description;
    QStringList operands;
    int queryWords = 0, requiredMode = -1;
};
struct DirectorCommand {
    int word = 0;
    quint32 opcode = 0;
    QVector<quint32> operands;
};
struct DirectorProgram {
    QVector<DirectorCommand> commands;
    QByteArray source;
    static DirectorProgram decode(const QByteArray &bytes);
    QByteArray encode() const;
    int resourceClass() const;
};
struct DirectorResource {
    quint32 key = 0, envelope = 0;
    QVector<int> selectors;
    QString name;
    DirectorProgram program;
};
class Directors {
  public:
    static const QMap<quint32, DirectorOpcode> &grammar();
    static Directors load(const Rom &rom);
    const QMap<quint32, DirectorResource> &entries() const {
        return resources_;
    }
    const QVector<quint32> &selectors() const {
        return selectors_;
    }

  private:
    QMap<quint32, DirectorResource> resources_;
    QVector<quint32> selectors_;
};
} // namespace lordly
