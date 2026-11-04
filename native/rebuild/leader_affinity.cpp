#include "rebuild/leader_affinity.h"
#include <QFile>
namespace lordly {
LeaderAffinityBuild buildLeaderAffinity(const LeaderAffinity &model, quint32 address,
    quint32 romOffset, const Compiler &compiler, const QString &directory) {
    LeaderAffinityBuild result;
    result.data = model.runtimeData();
    const quint32 codeOffset = quint32(result.data.size());
    QFile source(":/game/tools/leader-affinity.c");
    if (!source.open(QIODevice::ReadOnly)) throw Error("Missing combat-affinity C source");
    const auto code = compiler.compileModule(source.readAll(), directory, "leader_affinity",
        address + codeOffset,
        {{"affinity_config.h", QByteArray("#define LC_AFFINITY_GROUPS ") + QByteArray::number(model.groups().size()) + "\n"}},
        {{"lc_affinity_members",address+64}, {"lc_affinity_strengths",address+320}});
    if (code.bytes.isEmpty() || code.bytes.size()>1024)
        throw Error("Combat-affinity lookup exceeds its code allocation");
    result.data += code.bytes;
    while (result.data.size()%16) result.data.append(char(0));
    const auto entry = code.entries.value("lc_leader_affinity");
    if (entry<address+codeOffset || entry>=address+quint32(result.data.size()) || entry%4)
        throw Error("Invalid compiled combat-affinity entry");
    put32(result.data,8,quint32(result.data.size()));
    put32(result.data,28,quint32(result.data.size())-codeOffset);
    put32(result.data,32,entry-address);
    result.owner = model.extendedOwner(romOffset,address,result.data,entry);
    return result;
}
}
