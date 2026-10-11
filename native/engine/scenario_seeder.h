#pragma once
#include "engine/save_document.h"
#include "engine/story_routes.h"
namespace lordly {
struct SeedChoice {
    StoryRoute route;
    int chaos = 50;
    QString location, mission;
};
class ScenarioSeeder {
  public:
    explicit ScenarioSeeder(Rom);
    const Rom &rom() const {
        return rom_;
    }
    const QVector<SeedChoice> &choices() const {
        return choices_;
    }
    QByteArray defaultArmyBytes() const {
        return source_.serialize();
    }
    QByteArray seed(int choice, const SaveDocument *army = nullptr) const;
    static QString batteryPath(const Rom &, const QString &project64);
    // Returns the backup path, or empty for a new file. Existing slot 2 remains
    // untouched when merging a generated seed; a full user save is verbatim.
    static QString install(const Rom &, const QByteArray &, const QString &destination,
                           bool mergeSeed);
    static bool project64Running();

  private:
    static SaveDocument patch(SaveDocument, const QByteArray &memory);
    SaveDocument withArmy(SaveDocument, const SaveDocument &) const;
    Rom rom_;
    StoryRoutes routing_;
    SaveDocument source_;
    QVector<SeedChoice> choices_;
};
} // namespace lordly
