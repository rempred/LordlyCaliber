#include "app/record_model.h"
#include "app/value_editor.h"
#include "game/director/actor_body.h"
#include "engine/resource.h"
#include "engine/text.h"
#include "rebuild/compiler.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QtTest>
#include <algorithm>
using namespace lordly;
class WeaponTypesTests : public QObject {
    Q_OBJECT
    QJsonObject catalog() {
        QFile file(":/catalog.json");
        if (!file.open(QIODevice::ReadOnly)) throw Error("Missing test catalog");
        return QJsonDocument::fromJson(file.readAll()).object();
    }
  private slots:
    void capacityAndValidation() {
        WeaponTypes types;
        QSet<int> ids;
        for (int i = 0; i < 230; ++i) {
            const auto id = types.create(QString("Weapon group %1").arg(i), {});
            QVERIFY(!ids.contains(id));
            QVERIFY(id == 6 || (id >= 26 && id <= 254));
            ids.insert(id);
        }
        QCOMPARE(ids.size(), 230);
        QVERIFY_THROWS_EXCEPTION(Error, types.create("Overflow", {}));
        for (const auto &bad : {QJsonObject{{"0", "Reserved"}}, QJsonObject{{"255", "Reserved"}},
                               QJsonObject{{"1", "Retail"}}, QJsonObject{{"06", "Wrong key"}},
                               QJsonObject{{"6", ""}}, QJsonObject{{"6", "Bad\nname"}},
                               QJsonObject{{"6", "Same"}, {"26", "same"}}})
            QVERIFY_THROWS_EXCEPTION(Error, types.setNames(bad));
        WeaponTypes skip;
        QCOMPARE(skip.create("Available", {6, 26}), 27);
        QByteArray items(12 + 64, 0);
        const uint16_t equipment[4] = {1, 0, 0, 0};
        for (int type = 0; type < 256; ++type) {
            put8(items, 48, quint8(type));
            uint16_t selected = 0;
            QCOMPARE(lordly_body_equipment(reinterpret_cast<const uint8_t *>(items.constData()),
                                          2, equipment, &selected), 0);
            const bool expected = (type >= 1 && type <= 13) || type == 24 || (type >= 26 && type <= 254);
            QCOMPARE(selected, expected ? 1 : 0);
        }
    }
    void projectAndUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto p = Project::fromRom(Rom::open(path), catalog());
            QVERIFY(p.hasItemData());
            QUndoStack undo;
            RecordModel model(&p, Table::Items, &undo);
            const auto itemFields = fields(Table::Items);
            const auto typeField = std::find_if(itemFields.begin(), itemFields.end(),
                [](const Field &f) { return f.choices == "EQUIP_TYPES"; });
            QVERIFY(typeField != itemFields.end());
            int chosen = -1;
            ValueEditor selector(*typeField, p.catalog(), [&](int value) { chosen = value; });
            const auto before = p.manifest();
            QCOMPARE(model.createWeaponType("Golem arms", 1), 6);
            QCOMPARE(u8(p.record(Table::Items, 1).bytes, 4), 6);
            QCOMPARE(p.catalog()["EQUIP_TYPES"].toObject()["6"].toString(), "Golem arms");
            selector.setChoices(p.catalog());
            auto dropdown = selector.findChild<QComboBox *>();
            QVERIFY(dropdown); QVERIFY(dropdown->findText("Golem arms") >= 0);
            dropdown->setCurrentIndex(dropdown->findText("Golem arms"));
            QCOMPARE(chosen, 6);
            QVERIFY(p.changeReport().text().contains("Golem arms"));
            const auto after = p.manifest();
            undo.undo();
            QCOMPARE(p.manifest(), before);
            QVERIFY(!p.catalog()["EQUIP_TYPES"].toObject().contains("6"));
            undo.redo();
            QCOMPARE(p.manifest(), after);
            QVERIFY_THROWS_EXCEPTION(Error, p.createWeaponType("golem arms"));
            QTemporaryDir work;
            QVERIFY(work.isValid());
            p.saveArchive(work.filePath("golem.lcproj"));
            auto restored = Project::fromRom(Rom::open(path), catalog());
            restored.applyManifest(Project::readProject(work.filePath("golem.lcproj")));
            QCOMPARE(restored.manifest(), after);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void golemExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and compiler required");
        try {
            const auto source = Rom::open(path);
            auto p = Project::fromRom(source, catalog());
            const int golemType = p.createWeaponType("Golem arms");
            const int otherType = p.createWeaponType("Other arms");
            QCOMPARE(golemType, 6); QCOMPARE(otherType, 26);
            const int first = p.appendItem(1, "Stone fists"), second = p.appendItem(1, "Iron fists"),
                      other = p.appendItem(1, "Other weapon");
            for (const int id : {first, second, other}) {
                auto bytes = p.record(Table::Items, id).bytes;
                put8(bytes, 4, id == other ? otherType : golemType);
                put8(bytes, 6, 1); // A selectable weapon, rather than a fixed innate attack.
                put8(bytes, 10, id == second ? 40 : 20); // STR contribution.
                p.replaceBytes(Table::Items, id, bytes);
            }
            int golem = -1;
            QJsonArray golems;
            for (const auto &row : p.records(Table::Classes))
                if (row.name == "Golem" || row.name == "Stone Golem" || row.name == "Baldr Golem") {
                    if (row.name == "Golem") golem = row.id;
                    golems << row.id;
                }
            QVERIFY2(golem >= 0, "Retail Golem class must be named in the catalog");
            QCOMPARE(golems.size(), 3);
            for (const auto &value : golems) {
                const auto id = value.toInt();
                auto body = p.record(Table::Classes, id).bytes;
                put16(body, 46, first); put8(body, 54, u8(body, 54) & ~1);
                p.replaceBytes(Table::Classes, id, body);
            }
            p.loadShops(); auto shops = *p.shops();
            shops.setStock(1, {{quint16(first), quint16(second), quint16(other), 1, 100, 200}, {}});
            p.setShops(shops.json());
            QTemporaryDir work;
            QVERIFY(work.isValid());
            for (int stage = 0; stage < 3; ++stage) {
                if (stage == 1) {
                    p.setItemNameBytes(first, encodeText("Longer stone fists", false));
                    auto names = p.weaponTypes().names(); names["6"] = "Golem weapons";
                    p.setWeaponTypes(names);
                }
                if (stage == 2) {
                    while (p.weaponTypes().names().size() < WeaponTypes::maximumCustom)
                        p.createWeaponType(QString("Category %1 ").arg(p.weaponTypes().names().size()).leftJustified(32, 'x'));
                    auto item = p.record(Table::Items, other).bytes;
                    put8(item, 4, 254); p.replaceBytes(Table::Items, other, item);
                }
                const auto built = rebuild(p, tools, work.path());
                auto reopened = Project::fromRom(Rom::fromBytes(built.image), catalog());
                QCOMPARE(reopened.weaponTypes().names(), p.weaponTypes().names());
                QVERIFY(reopened.hasItemData());
                reopened.loadTexts(); reopened.loadShops();
                QCOMPARE(reopened.shops()->json(), p.shops()->json());
                QCOMPARE(reopened.record(Table::Classes, golem).bytes.mid(4), p.record(Table::Classes, golem).bytes.mid(4));
                const auto items = reopened.itemData();
                const auto at = items.loadedTypesOffset();
                QVERIFY(at > 0);
                const auto helpAt = at + u32(built.image, at + 20);
                const auto help = Resource::load(reopened.rom(), helpAt, u32(built.image, helpAt) + 4).decoded;
                QVERIFY(help.contains(encodeText(stage ? "Golem weapons" : "Golem arms", false)));
                QTemporaryDir output(QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs/weapon-types-XXXXXX"));
                QVERIFY(output.isValid()); output.setAutoRemove(false);
                const auto target = output.filePath(QFileInfo(path).fileName());
                exportRom(p, built, target);
                if (!stage) p.saveArchive(output.filePath("Golem weapons.lcproj"));
                QJsonArray records, names;
                for (const auto &row : p.records(Table::Items)) {
                    records << QString::fromLatin1(row.bytes.toHex());
                    names << QString::fromLatin1(p.itemNameBytes(row.id).toHex());
                }
                atomicWrite(output.filePath("weapon-types.json"), QJsonDocument(QJsonObject{
                    {"rom", target}, {"original", path}, {"golem", golem}, {"golems", golems}, {"first", first},
                    {"second", second}, {"other", other}, {"records", records}, {"names", names},
                    {"metadataRom", qint64(at)}, {"help", QString::fromLatin1(help.toBase64())},
                    {"categories", p.weaponTypes().names()}}).toJson());
                qInfo().noquote() << "Weapon test fixture:" << output.filePath("weapon-types.json");
                QProcess runtime;
                runtime.setProcessChannelMode(QProcess::MergedChannels);
                runtime.start("node", {QFINDTESTDATA("weapon-types-mips.cjs"), output.filePath("weapon-types.json")});
                QVERIFY(runtime.waitForFinished(60000));
                const auto log = runtime.readAll();
                QVERIFY2(runtime.exitCode() == 0, log.constData());
                qInfo().noquote() << log;
                p = std::move(reopened);
            }
            QCOMPARE(Rom::open(path).bytes(), source.bytes());
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
};
QTEST_MAIN(WeaponTypesTests)
#include "weapon_types_tests.moc"
