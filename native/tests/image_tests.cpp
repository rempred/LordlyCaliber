#include "engine/archive.h"
#include "engine/image.h"
#include "engine/scene_images.h"
#include "engine/scene_image_presentation.h"
#include "engine/color_combiner.h"
#include "game/director/image_presentation.h"
#include <bit>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class ImageTests : public QObject {
    Q_OBJECT
  private slots:
    void imageDrawingAgainstMips() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        QTemporaryDir work;QProcess reference;
        reference.start("node",{QFINDTESTDATA("image-drawing-mips.cjs"),path,work.filePath("drawing.json")});
        QVERIFY(reference.waitForFinished(60000));QVERIFY2(reference.exitCode()==0,reference.readAllStandardError().constData());
        QFile file(work.filePath("drawing.json"));QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data=QJsonDocument::fromJson(file.readAll()).object();QCOMPARE(data["cases"].toArray().size(),112);
        for(const auto &v:data["cases"].toArray()) {
            const auto row=v.toObject();const auto context=QByteArray::fromHex(row["context"].toString().toLatin1());
            LordlyImageEcho echo{};for(int i=0;i<3;++i){echo.live[i]=(row["mask"].toInt()>>i)&1;
                const auto b=QByteArray::fromHex(row["saved"].toArray()[i].toString().toLatin1());memcpy(echo.matrix[i],b.constData(),64);}
            const auto plan=SceneImagePresentation::drawPlan(context,echo);
            const auto expected=row["draws"].toArray();
            QCOMPARE(plan.size(),expected.size());
            for(int i=0;i<plan.size();++i) {
                const auto draw=expected[i].toObject();const auto stack=draw["stack"].toArray();
                QCOMPARE(plan[i].matrix,QByteArray::fromHex(stack[1].toString().toLatin1()));
                QCOMPARE(plan[i].alpha,quint8(draw["primitive"].toInteger()));
                QCOMPARE(stack.size(),plan[i].current&&row["selected"].toString()=="processed"?3:2);
                const auto combine=draw["combine"].toArray();const ColorCombiner combiner(quint32(combine[0].toInteger()),quint32(combine[1].toInteger()),false);
                for(const quint8 alpha:{quint8(0),quint8(37),quint8(255)}) {
                    CombineInputs inputs;inputs.texel0={31,87,129,alpha};inputs.primitive={255,255,255,plan[i].alpha};
                    const auto color=combiner.evaluate(inputs);QCOMPARE(color[0],31);QCOMPARE(color[1],87);QCOMPARE(color[2],129);
                    QCOMPARE(int(color[3]),plan[i].current?qMin(255,int(alpha)+1):int(plan[i].alpha));
                }
            }
            const auto special=row["special"].toObject();QCOMPARE(special["pass"].toInt(),1);QCOMPARE(special["special"].toInt(),1);
            QCOMPARE(special["stack"].toArray(),QJsonArray{data["parent"]});
        }
    }
    void imageEchoAgainstMips() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        const auto rom=Rom::open(path);QTemporaryDir work;QProcess reference;
        reference.start("node",{QFINDTESTDATA("image-echo-mips.cjs"),path,work.filePath("echo.json")});
        QVERIFY(reference.waitForFinished(60000));QVERIFY2(reference.exitCode()==0,reference.readAllStandardError().constData());
        QFile file(work.filePath("echo.json"));QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data=QJsonDocument::fromJson(file.readAll()).object();
        const auto settled=std::bit_cast<double>(quint64(u32(rom.bytes(),0x2b8ac0))<<32|u32(rom.bytes(),0x2b8ac4));
        auto bytes=[](const QJsonObject &row,const char *key){return QByteArray::fromHex(row[key].toString().toLatin1());};
        QCOMPARE(data["rows"].toArray().size(),96);int checked=0;
        for(const auto &value:data["rows"].toArray()) {
            const auto row=value.toObject(),initial=row["initial"].toObject();
            QByteArray context(0x848,0);context.replace(0,0x54,bytes(initial,"head"));context.replace(0x798,0xb0,bytes(initial,"tail"));
            auto channel=bytes(initial,"channel");LordlyImageEcho echo{};
            for(const auto &step:row["steps"].toArray()) {
                const auto entry=step.toObject(),expected=entry["after"].toObject();
                if(entry.contains("start")) {
                    uint32_t args[3];for(int i=0;i<3;++i)args[i]=quint32(entry["start"].toArray()[i].toInteger());
                    QVERIFY(lordly_image_echo_start(reinterpret_cast<uint8_t *>(context.data()),reinterpret_cast<uint8_t *>(channel.data()),args));
                } else QVERIFY(lordly_image_echo_advance(&echo,reinterpret_cast<uint8_t *>(context.data()),settled));
                QCOMPARE(context.first(0x54),bytes(expected,"head"));QCOMPARE(context.mid(0x798),bytes(expected,"tail"));
                QCOMPARE(channel,bytes(expected,"channel"));
                QCOMPARE(lordly_image_echo_query(reinterpret_cast<const uint8_t *>(context.constData())),quint32(expected["query"].toInteger()));
                for(int i=0;i<3;++i) {
                    const auto matrix=expected["matrices"].toArray()[i];QCOMPARE(bool(echo.live[i]),!matrix.isNull());
                    if(echo.live[i])QCOMPARE(QByteArray(reinterpret_cast<const char *>(echo.matrix[i]),64),QByteArray::fromHex(matrix.toString().toLatin1()));
                }
                ++checked;
            }
        }
        QCOMPARE(checked,4299);
        QCOMPARE(data["fades"].toArray().size(),1008);
        for(const auto &value:data["fades"].toArray()) {
            const auto row=value.toObject(),initial=row["initial"].toObject();QByteArray context(0x848,0);
            context.replace(0,0x54,bytes(initial,"head"));context.replace(0x798,0xb0,bytes(initial,"tail"));
            LordlyImageEcho echo{};for(int i=0;i<3;++i)echo.live[i]=(row["mask"].toInt()>>i)&1;
            for(const auto &step:row["steps"].toArray()) {
                lordly_image_echo_fade(reinterpret_cast<uint8_t *>(context.data()),&echo,quint16(row["pressed"].toInt()));
                QCOMPARE(context.first(0x54),bytes(step.toObject(),"head"));QCOMPARE(context.mid(0x798),bytes(step.toObject(),"tail"));
            }
        }
        QByteArray context(0x848,0),channel(88,0);put32(channel,0x54,0x7fc00000);
        const auto before=context,view=channel;const uint32_t args[]{1,30,4};
        QVERIFY(!lordly_image_echo_start(reinterpret_cast<uint8_t *>(context.data()),reinterpret_cast<uint8_t *>(channel.data()),args));
        QCOMPARE(context,before);QCOMPARE(channel,view);
    }
    void imagePresentationAgainstMips() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        const auto rom=Rom::open(path);QTemporaryDir work;QProcess reference;
        reference.start("node",{QFINDTESTDATA("image-presentation-mips.cjs"),path,work.filePath("presentation.json")});
        QVERIFY(reference.waitForFinished(60000));QVERIFY2(reference.exitCode()==0,reference.readAllStandardError().constData());
        QFile file(work.filePath("presentation.json"));QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data=QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(data["pixels"].toArray().size(),24);
        for(const auto &v:data["pixels"].toArray()) {
            const auto row=v.toObject();const int w=row["width"].toInt(),h=row["height"].toInt();
            QImage source(w,h,QImage::Format_RGBA8888);
            for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
                const auto color=quint16((y*w+x)*977+0x9345);auto p=source.scanLine(y)+x*4;
                p[0]=quint8((color>>11)<<3);p[1]=quint8((color>>6&31)<<3);p[2]=quint8((color>>1&31)<<3);p[3]=(color&1)?255:0;
            }
            const auto retained=source.copy();const auto output=SceneImagePresentation::vignette(source,row["cap"].toInt());
            QCOMPARE(source,retained);QCOMPARE(output.size(),QSize(w/2,h/2));
            const auto expected=QByteArray::fromBase64(row["rgba"].toString().toLatin1());
            for(int y=0;y<h/2;++y)
                QCOMPARE(QByteArray(reinterpret_cast<const char *>(output.constScanLine(y)),w/2*4),expected.mid(y*(w/2)*4,w/2*4));
        }
        double constants[5];for(int i=0;i<5;++i)constants[i]=std::bit_cast<double>(quint64(u32(rom.bytes(),0x2b8a98+i*8))<<32|u32(rom.bytes(),0x2b8a9c+i*8));
        const float radians=std::bit_cast<float>(u32(rom.bytes(),0x3ab50));
        QCOMPARE(data["matrices"].toArray().size(),96);
        for(const auto &v:data["matrices"].toArray()) {
            const auto row=v.toObject();auto context=QByteArray::fromHex(row["before"].toString().toLatin1());
            const auto expected=QByteArray::fromHex(row["after"].toString().toLatin1());
            float camera[14],placement[4];for(int i=0;i<14;++i)camera[i]=float(row["camera"].toArray()[i].toDouble());
            for(int i=0;i<4;++i)placement[i]=float(row["placement"].toArray()[i].toDouble());
            QVERIFY(lordly_image_matrices(reinterpret_cast<uint8_t *>(context.data()),camera,placement,quint32(row["flags"].toInteger()),constants,radians));
            put8(context,0x838,2);put8(context,0x83d,130);put32(context,0x50,1);put32(context,0x4c,2);
            QCOMPARE(context,expected);
        }
        auto owner=SceneImagePresentation::load(rom,1);auto context=owner.initialContext();
        QByteArray cameraBytes(56,0);const float cam[]{45,4.f/3,1,20000,1,272,80,127,0,0,0,0,1,0};
        for(int i=0;i<14;++i)put32(cameraBytes,i*4,std::bit_cast<quint32>(cam[i]));
        const SceneCamera camera(cameraBytes,1);
        const auto before=context;
        QVERIFY_THROWS_EXCEPTION(Error,owner.construct(rom,{20,0,0,0,100,100,130,4},camera,context));
        QCOMPARE(context,before);QCOMPARE(owner.slot(),-1);QVERIFY(owner.processed().isNull());
        owner.construct(rom,{2,quint32(-102),39,0,29,29,130,4},camera,context);
        QCOMPARE(owner.slot(),2);QCOMPARE(owner.processed().size(),owner.source().size()/2);
        const auto built=context;
        const auto original=owner.source();
        QVERIFY_THROWS_EXCEPTION(Error,owner.construct(rom,{2,0,0,0,100,100,130,4},camera,context));
        QCOMPARE(context,built);QCOMPARE(owner.source(),original);
    }
    void imageLaunchAgainstMips() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        QTemporaryDir work;QProcess reference;
        reference.start("node",{QFINDTESTDATA("image-launch-mips.cjs"),path,work.filePath("launch.json")});
        QVERIFY(reference.waitForFinished(60000));QVERIFY2(reference.exitCode()==0,reference.readAllStandardError().constData());
        QFile file(work.filePath("launch.json"));QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data=QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(data["cases"].toArray().size(),127);
        const auto rom=Rom::open(path);const auto directors=Directors::load(rom);
        for(const auto &value:data["cases"].toArray()) {
            const auto row=value.toObject();QVector<uint32_t> words;
            for(const auto &word:row["words"].toArray())words<<quint32(word.toInteger());
            LordlySceneLaunch launch{0,-1,-1,0};QVERIFY(lordly_scene_launch_scan(words.constData(),int(words.size()),&launch));
            const auto expected=QByteArray::fromHex(row["record"].toString().toLatin1());
            QCOMPARE(launch.resource_class,qint16(u16(expected,0)));QCOMPARE(launch.environment,qint16(u16(expected,2)));
            QCOMPARE(launch.image_row,qint16(u16(expected,4)));QCOMPARE(launch.capture,qint16(u16(expected,6)));
            const auto key=quint32(row["key"].toInteger());
            if(key) {
                QVERIFY(directors.entries().contains(key));
                const auto actual=SceneImages::launch(directors.entries()[key].program);
                QCOMPARE(actual.resource_class,launch.resource_class);QCOMPARE(actual.environment,launch.environment);
                QCOMPARE(actual.image_row,launch.image_row);QCOMPARE(actual.capture,launch.capture);
            }
        }
        QCOMPARE(data["rows"].toArray().size(),69);
        for(const auto &value:data["rows"].toArray()) {
            const auto row=value.toObject();
            if(row["image"].isNull())QVERIFY_THROWS_EXCEPTION(Error,SceneImages::presentationImage(rom,row["row"].toInt()));
            else QCOMPARE(SceneImages::presentationImage(rom,row["row"].toInt()),row["image"].toString());
        }
        auto changed=rom.bytes();put8(changed,0x654a0+9+3,40);
        QCOMPARE(SceneImages::presentationImage(Rom::fromBytes(changed),1),QString("archive:154"));
        auto pointerChange=rom.bytes();const auto root=0x18bd022+0x594280+4;
        put32(pointerChange,root+5*4,u32(pointerChange,root+36*4));
        QCOMPARE(SceneImages::presentationImage(Rom::fromBytes(pointerChange),1),QString("archive:154"));
        QVERIFY_THROWS_EXCEPTION(Error,SceneImages::presentationImage(rom,-1));
        QVERIFY_THROWS_EXCEPTION(Error,SceneImages::presentationImage(rom,69));
        QByteArray script(16,0);put32(script,0,0x80000007);put32(script,4,65538);
        put32(script,8,0x80000001);put32(script,12,0xff000004);
        const auto edited=SceneImages::launch(DirectorProgram::decode(script));QCOMPARE(edited.image_row,2);
        put32(script,4,65535);QCOMPARE(SceneImages::launch(DirectorProgram::decode(script)).image_row,-1);
        LordlySceneLaunch preserved{4,5,6,7};const uint32_t invalid=0x80000007;
        QVERIFY(!lordly_scene_launch_scan(&invalid,1,&preserved));QCOMPARE(preserved.image_row,6);QCOMPARE(preserved.resource_class,4);
    }
    void indexedPaddingAndBounds() {
        // 3x2 CI4, 8-byte storage rows. Padding deliberately selects another
        // palette entry; expected visible pixels are specified independently.
        auto bytes = QByteArray::fromHex("3634020000030002") + QByteArray(16, char(-18)) +
                     QByteArray(32, 0);
        put8(bytes, 8, 0x12);
        put8(bytes, 9, 0x3e);
        put8(bytes, 16, 0x32);
        put8(bytes, 17, 0x1e);
        put16(bytes, 26, 0xf801);
        put16(bytes, 28, 0x07c1);
        put16(bytes, 30, 0x003f);
        const auto decoded = decodeN64Image(bytes);
        QCOMPARE(decoded.width, 3);
        QCOMPARE(decoded.height, 2);
        QCOMPARE(decoded.rgba,
                 QByteArray::fromHex("ff0000ff00ff00ff0000ffff0000ffff00ff00ffff0000ff"));
        QCOMPARE(decodeN64Image(bytes + QByteArray("retained tail")).rgba, decoded.rgba);
        QVERIFY_THROWS_EXCEPTION(Error, decodeN64Image(bytes.first(bytes.size() - 1)));
        auto bad = bytes;
        put16(bad, 4, 0);
        QVERIFY_THROWS_EXCEPTION(Error, decodeN64Image(bad));
        bad = bytes;
        put16(bad, 4, 8192);
        put16(bad, 6, 8192);
        QVERIFY_THROWS_EXCEPTION(Error, decodeN64Image(bad));
        bad = bytes;
        put8(bad, 2, 7);
        QVERIFY_THROWS_EXCEPTION(Error, decodeN64Image(bad));
        QVERIFY_THROWS_EXCEPTION(Error, decodeN64Image(bytes.first(7)));
    }
    void browserPixelAgreement() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required for real image comparisons");
        QTemporaryDir work;
        const auto fixture = work.filePath("images.json");
        QProcess generator;
        generator.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("image-fixture.cjs"),
             path, fixture});
        QVERIFY(generator.waitForFinished(30000));
        const auto errors = generator.readAllStandardError();
        QVERIFY2(generator.exitCode() == 0, errors.constData());
        QFile file(fixture);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data = QJsonDocument::fromJson(file.readAll()).object();
        const auto rows = data["rows"].toArray();
        QCOMPARE(rows.size(), 104);
        QCOMPARE(data["types"].toObject().size(), 6);
        const auto rom = Rom::open(path);
        for (const auto &value : rows) {
            const auto row = value.toObject();
            const auto source = LhaMember::read(rom.bytes(), row["offset"].toInt()).decode(rom);
            const auto image = decodeN64Image(source);
            QCOMPARE(image.width, row["width"].toInt());
            QCOMPARE(image.height, row["height"].toInt());
            QCOMPARE(sha256(image.rgba), row["sha256"].toString().toLatin1());
        }
        const auto image = decodeN64Image(LhaMember::find(rom, "s_map04.n64").decode(rom));
        QImage view(reinterpret_cast<const uchar *>(image.rgba.constData()), image.width,
                    image.height, image.width * 4, QImage::Format_RGBA8888);
        QVERIFY(view.scaled(image.width * 3, image.height * 3, Qt::KeepAspectRatio,
                            Qt::FastTransformation)
                    .save("native-map-image.png"));
    }
};
QTEST_GUILESS_MAIN(ImageTests)
#include "image_tests.moc"
