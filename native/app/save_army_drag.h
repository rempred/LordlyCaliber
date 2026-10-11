#pragma once
#include <QMimeData>
#include <QPointer>

namespace lordly {
enum ArmyOverviewRole { ArmyOverviewImage = Qt::UserRole + 1, ArmyOverviewHits, ArmyOverviewCells };
// Keep transfers inside the originating save editor and retain the source identity
// so a refresh cannot turn an old drag into a move of a different character.
class SaveArmyDrag final : public QMimeData {
  public:
    SaveArmyDrag(QObject *source, int sourceUnit, int sourceMember, int character)
        : owner(source), unit(sourceUnit), member(sourceMember), id(character) {
        setData("application/x-lordly-save-army-member", QByteArray::number(character));
    }
    QPointer<QObject> owner;
    int unit, member, id;
};
}
