#pragma once
#include "engine/rom.h"
#include <QVariant>
#include <QVector>
namespace lordly {
class ClassData;
class Project;
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
    QVector<int> members, cells, supplies;
};
struct SaveLegion {
    int index = 0, label = 0;
    QVector<int> units;
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
    QVector<SaveLegion> legions() const;
    SaveChange planDisbandLegion(int index) const;
    SaveChange planDetachLegionUnit(int index, int unit) const;
    int legionCapacity(int index) const;
    SaveChange planJoinLegion(int index, int unit, const ClassData &) const;
    QVector<int> reserveMembers() const;
    QVector<int> newUnitLeaders(const ClassData &) const;
    SaveChange planCreateUnit(int leader, int cell, const ClassData &) const;
    SaveChange planDisbandUnit(int index) const;
    int reserveSoldiers() const;
    int soldiersInGroup(int member) const;
    SaveChange planSoldierGroup(int toUnit, int cell, const ClassData &) const;
    bool canEditUnits() const;
    static QVector<int> carryableItems(const Project &);
    int maximumUnitSupply(int index, int slot) const;
    int unitCarryingCapacity(int index, const ClassData &) const;
    int consumableAvailable(int id) const;
    SaveChange planUnitSupplies(int index, const QVector<int> &, const Project &) const;
    SaveChange planUnitEdit(int index, const QVector<int> &cells, int unity, int tactic,
                            const ClassData &) const;
    SaveChange planUnitLeader(int index, int member, const ClassData &) const;
    SaveChange planUnitSwap(int fromUnit, int fromMember, int toUnit, int toMember,
                            const ClassData &) const;
    SaveChange planUnitTransfer(int fromUnit, int fromMember, int toUnit, int cell,
                                const ClassData &) const;
    SaveChange planToReserve(int fromUnit, int fromMember, const ClassData &) const;
    SaveChange planFromReserve(int member, int toUnit, int cell, const ClassData &) const;
    void apply(const SaveChange &, bool forward);
    QByteArray serialize() const;
    void setClassCount(int);
    void validateClassEdits(int classCount) const;
    void validateUnitSupplyEdits(const Project &) const;
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
    QByteArray monthLengths_, movementOrder_;
    QVector<Cell> codec_;
    int classCount_ = 165, slot_ = -1, chunkSize_ = 0x20000;
    bool swapped_ = false, expanded_ = false;
    int daysInMonth(unsigned year, unsigned month) const;
    SaveChange planBytes(int address, const QByteArray &replacement) const;
    SaveChange difference(const QByteArray &next, int first, int size) const;
    SaveLegion editableLegion(int index) const;
    SaveChange planLegionMembers(const SaveLegion &) const;
    QPair<int, int> memberClasses(int id) const;
    QPair<int, int> memberRuntimeClasses(int id) const;
    void validateFormation(const SaveUnit &, const ClassData &) const;
    void validateTransferMember(int, const QVector<SaveUnit> &) const;
    int unitMovementType(const SaveUnit &, const ClassData &) const;
    int consumableOwned(int id) const;
    int consumableAssigned(int id) const;
    SaveChange planMembershipChange(const QVector<SaveUnit> &, const ClassData &,
                                    bool resetUnity = true) const;
};
} // namespace lordly
