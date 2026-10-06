#pragma once
#include "engine/rom.h"
#include <QVariant>
#include <QVector>
namespace lordly {
class ClassData;
enum class SaveTable { Characters, Equipment, Consumables, Game };
struct SaveField {
    QString key, label;
    int offset = 0, bytes = 1;
    quint32 minimum = 0, maximum = 255;
};
struct SaveSlot {
    int id;
    QString name;
    bool valid = false;
    QString reason;
};
struct SaveSpan {
    int offset;
    QByteArray before, after;
};
struct SaveChange {
    int slot = -1;
    QVector<SaveSpan> spans;
};
struct SaveUnit {
    int index = 0, label = 0, unity = 0, tactic = 0;
    QVector<int> members, cells;
};
class SaveDocument {
  public:
    static SaveDocument open(const Rom &, const QByteArray &, int classCount = 165);
    QVector<SaveSlot> saveSlots() const;
    void selectSlot(int);
    int selectedSlot() const {
        return slot_;
    }
    bool battery() const {
        return format_ == Format::Battery;
    }
    bool expanded() const {
        return expanded_;
    }
    QString formatName() const;
    QString defaultExtension() const;
    QVector<int> rows(SaveTable) const;
    QVector<SaveField> fields(SaveTable) const;
    QVariant value(SaveTable, int row, const QString &key) const;
    SaveChange planEdit(SaveTable, int row, const QString &key, const QVariant &) const;
    QVector<SaveUnit> units() const;
    bool canEditUnits() const;
    SaveChange planUnitEdit(int index, const QVector<int> &cells, int unity, int tactic,
                            const ClassData &) const;
    SaveChange planUnitSwap(int fromUnit, int fromMember, int toUnit, int toMember,
                            const ClassData &) const;
    void apply(const SaveChange &, bool forward);
    QByteArray serialize() const;
    void setClassCount(int);
    void validateClassEdits(int classCount) const;
    const QByteArray &memory() const {
        return ram_;
    }

  private:
    enum class Format { Battery, Ram, State, Rzip };
    struct Cell {
        int address, bytes, bits, bit;
        bool sign;
    };
    void readCodec(const Rom &);
    QByteArray decode(int slot) const;
    bool validSlot(int slot) const;
    int address(SaveTable, int row) const;
    Format format_ = Format::Ram;
    QByteArray original_, data_, originalData_, ram_, blob_;
    QByteArray monthLengths_;
    QVector<Cell> codec_;
    int classCount_ = 165, slot_ = -1, chunkSize_ = 0x20000;
    bool swapped_ = false, expanded_ = false;
    int daysInMonth(unsigned year, unsigned month) const;
    SaveChange planBytes(int address, const QByteArray &replacement) const;
    SaveChange difference(const QByteArray &next, int first, int size) const;
    QPair<int, int> memberClasses(int id) const;
    void validateFormation(const SaveUnit &, const ClassData &) const;
};
} // namespace lordly
