#include "engine/eset.h"

namespace lordly {
Eset Eset::decode(const QByteArray &data) {
    if (data.size() < 17 || u16(data, 0) != 8 || u16(data, 2) != 15 || u8(data, 14))
        throw Error("Unrecognized ESET header");
    const int deploymentCount = u16(data, 14), nodeOffset = u16(data, 4),
              conditionOffset = u16(data, 6);
    if (nodeOffset != 16 + deploymentCount * 18 || nodeOffset >= data.size())
        throw Error("ESET deployment rows exceed their section");
    const int nodeCount = u8(data, nodeOffset), nodeEnd = nodeOffset + 1 + nodeCount * 18;
    if (nodeEnd > data.size() || (deploymentCount && !nodeCount))
        throw Error("ESET movement nodes are truncated or missing");
    const bool present = conditionOffset != 0;
    if (present && (conditionOffset != nodeEnd || conditionOffset >= data.size()))
        throw Error("ESET condition section does not follow its movement nodes");
    const int conditionCount = present ? u8(data, conditionOffset) : 0;
    const int end = present ? conditionOffset + 1 + conditionCount * 10 : nodeEnd;
    if (end > data.size())
        throw Error("ESET conditions are truncated");
    Eset result;
    result.header = data.first(16);
    result.seed = data.mid(16, 2);
    result.conditionsPresent = present;
    result.tail = data.mid(end);
    for (int i = 0; i < deploymentCount; ++i)
        result.deployments << data.mid(18 + i * 18, 18);
    for (int i = 0; i < nodeCount; ++i)
        result.nodes << data.mid(nodeOffset + 1 + i * 18, 18);
    for (int i = 0; i < conditionCount; ++i)
        result.conditions << data.mid(conditionOffset + 1 + i * 10, 10);
    return result;
}
QByteArray Eset::encode() const {
    if (header.size() != 16 || seed.size() > 2 || deployments.size() > 255 || nodes.size() > 255 ||
        conditions.size() > 255 || (!deployments.isEmpty() && nodes.isEmpty()))
        throw Error("Invalid ESET framing or section counts");
    const qsizetype nodeOffset = 16 + deployments.size() * 18;
    const qsizetype nodeEnd = nodeOffset + 1 + nodes.size() * 18;
    const bool present = conditionsPresent || !conditions.isEmpty();
    const qsizetype end = present ? nodeEnd + 1 + conditions.size() * 10 : nodeEnd;
    QByteArray data(end, 0);
    data.replace(0, 16, header);
    put16(data, 0, 8);
    put16(data, 2, 15);
    put16(data, 4, quint16(nodeOffset));
    put16(data, 6, quint16(present ? nodeEnd : 0));
    put16(data, 14, quint16(deployments.size()));
    // With no deployments these bytes alias node framing, which wins below.
    for (qsizetype i = 0; i < seed.size() && 16 + i < data.size(); ++i)
        data[16 + i] = seed[i];
    auto copyRows = [&](const QVector<QByteArray> &rows, qsizetype at, int size) {
        for (const auto &row : rows) {
            if (row.size() != size)
                throw Error("Invalid ESET row length");
            data.replace(at, size, row);
            at += size;
        }
    };
    copyRows(deployments, 18, 18);
    // The final deployment's last two bytes belong to this section: its count
    // and first node ID. Regenerate them when row counts or node IDs change.
    put8(data, nodeOffset, quint8(nodes.size()));
    copyRows(nodes, nodeOffset + 1, 18);
    if (present) {
        put8(data, nodeEnd, quint8(conditions.size()));
        copyRows(conditions, nodeEnd + 1, 10);
    }
    data += tail;
    return data;
}
int Eset::baseLevel() const {
    return u8(header, 8);
}
void Eset::setBaseLevel(int level) {
    if (level < 1 || level > 99)
        throw Error("Scenario base level must be between 1 and 99");
    put8(header, 8, quint8(level));
}
} // namespace lordly
