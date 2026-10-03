#include "engine/project.h"
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

using namespace lordly;
static QByteArray syntheticRom() {
    QByteArray data(0x2800000,char(0)); put32(data,0,0x80371240); data.replace(0x3c,3,"NOE");
    return data;
}
class EngineTests : public QObject {
    Q_OBJECT
private slots:
    void byteOrders() {
        const auto expected=QByteArray::fromHex("8037124011223344")+QByteArray(56,0);
        const auto swapped=QByteArray::fromHex("3780401222114433")+QByteArray(56,0);
        const auto little=QByteArray::fromHex("4012378044332211")+QByteArray(56,0);
        for(const auto& input:{expected,swapped,little}) { auto rom=Rom::fromBytes(input); QCOMPARE(rom.bytes(),expected); QCOMPARE(rom.serialize(rom.bytes()),input); }
        QVERIFY_THROWS_EXCEPTION(Error,Rom::fromBytes(expected+char(0)));
        QVERIFY_THROWS_EXCEPTION(Error,Rom::fromBytes(QByteArray(64,0)));
    }
    void compressionCommands() {
        QCOMPARE(decodeLzss(QByteArray::fromHex("42414243"),3),QByteArray("ABC"));
        QCOMPARE(decodeLzss(QByteArray::fromHex("20"),2),QByteArray(2,0));
        QCOMPARE(decodeLzss(QByteArray::fromHex("0102"),5),QByteArray(5,char(-1)));
        QCOMPARE(decodeLzss(QByteArray::fromHex("0201"),4),QByteArray(4,0));
        QCOMPARE(decodeLzss(QByteArray::fromHex("424142438802"),7),QByteArray("ABCABCA"));
        QCOMPARE(decodeLzss(QByteArray::fromHex("4041100000"),5),QByteArray("AAAAA"));
        QCOMPARE(decodeLzss(QByteArray::fromHex("404100000000"),6),QByteArray("AAAAAA"));
        QCOMPARE(decodeLzss(QByteArray::fromHex("8000"),3),QByteArray(3,0));
        QCOMPARE(decodeLzss(QByteArray::fromHex("030f4042"),1),QByteArray("B"));
        QVERIFY_THROWS_EXCEPTION(Error,decodeLzss(QByteArray::fromHex("424142"),3));
        QVERIFY_THROWS_EXCEPTION(Error,decodeLzss(QByteArray::fromHex("80"),3));
        QByteArray mixed; for(int i=0;i<1024;++i) mixed.append(char(i)); mixed+=QByteArray(1000,0)+QByteArray(700,char(-1));
        QCOMPARE(decodeLzss(encodeLzss(mixed),mixed.size()),mixed);
    }
    void recordFramingAndPreservation() {
        auto data=syntheticRom(); const int knight=5; auto header=0x5dad8+(knight+1)*72-12;
        data[header+7]=char(-61); put16(data,header+8,123); put16(data,header+12,456);
        auto p=Project::fromRom(Rom::fromBytes(data));
        const auto& row=p.record(Table::Classes,knight); QCOMPARE(u16(row.bytes,8),quint16(123)); QCOMPARE(u16(row.bytes,12),quint16(456));
        int str=-1; for(int n=0;n<fields(Table::Classes).size();++n) if(fields(Table::Classes)[n].key=="strBase") str=n;
        QVERIFY(str>=0); p.setValue(Table::Classes,knight,str,900);
        auto rebuilt=p.rebuildRecords(); auto expected=data; put16(expected,header+12,900); QCOMPARE(rebuilt,expected);
        QVERIFY_THROWS_EXCEPTION(Error,p.setValue(Table::Classes,knight,str,65536));
        QCOMPARE(p.rom().bytes(),data);
    }
    void projectPersistence() {
        auto rom=Rom::fromBytes(syntheticRom()); auto p=Project::fromRom(rom);
        p.setValue(Table::Classes,5,0,2); QTemporaryDir dir; QVERIFY(dir.isValid());
        p.saveFolder(dir.path()); auto saved=Project::readProject(dir.path()); saved["extensionExample"]=QJsonObject{{"keep",42}};
        auto restored=Project::fromRom(rom); restored.applyManifest(saved);
        QCOMPARE(restored.value(Table::Classes,5,0),2); QCOMPARE(restored.manifest()["extensionExample"],saved["extensionExample"]);
        auto archive=dir.filePath("share.lcproject"); restored.saveArchive(archive);
        auto packed=Project::readProject(archive); QVERIFY(!packed.contains("sourcePath")); QCOMPARE(packed["records"],saved["records"]);
        auto wrong=saved; wrong["sourceSha256"]="wrong"; QVERIFY_THROWS_EXCEPTION(Error,restored.applyManifest(wrong));
        QCOMPARE(restored.value(Table::Classes,5,0),2);
    }
};
QTEST_GUILESS_MAIN(EngineTests)
#include "engine_tests.moc"
