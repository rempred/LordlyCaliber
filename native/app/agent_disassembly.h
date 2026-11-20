#pragma once
#include <QString>
namespace lordly {
QString disassembleGameWord(quint32 pc, quint32 word, bool knownAddress = true);
}
