#pragma once
#include "engine/rom.h"
#include <QVector>
namespace lordly {
struct PoseRecord {
    int opcode = 0;
    QByteArray operands;
};
struct PoseProgram {
    QByteArray source;
    QVector<PoseRecord> records;
};
// Native counted programs. Empty physical slices alias the following program.
class PoseBank {
  public:
    static PoseBank parse(const QByteArray &);
    static PoseProgram parseProgram(const QByteArray &);
    static QByteArray encodeProgram(const QVector<PoseRecord> &);
    const QVector<PoseProgram> &programs() const {
        return programs_;
    }
    const PoseProgram &effective(int state) const;
    QByteArray encoded() const;
    void validateEntry(int state)const { validateProgress(state); }
    void setProgram(int state, const QByteArray &);
    int appendProgram(const QByteArray &);
    void setFrame(int state, int record, int token, int ticks);
    void insertFrame(int state, int before, int token, int ticks);
    void removeFrame(int state, int record);
    void moveFrame(int state, int record, int destination);
    void replaceFrames(int state, const QVector<QPair<int, int>> &frames);

  private:
    void validateProgress(int state) const;
    QVector<PoseProgram> programs_;
};
} // namespace lordly
