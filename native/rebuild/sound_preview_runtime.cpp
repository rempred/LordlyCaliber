#include "rebuild/sound_preview_runtime.h"
#include "rebuild/compiler.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutex>
#include <QTemporaryDir>
namespace lordly {
quint32 installSoundPreviewRuntime(EffectMachine &m) {
    struct Runtime { CompiledModule module; QByteArray trampolines; QMap<quint32,quint32> hooks; };
    static QMutex mutex;static std::optional<Runtime> runtime;
    QMutexLocker lock(&mutex);
    if(!runtime) {
        auto tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if(tools.isEmpty())tools=QCoreApplication::applicationDirPath()+"/n64-toolchain";
        if(!QDir(tools).exists())tools=QCoreApplication::applicationDirPath()+"/../../.local/n64-toolchain";
        Compiler compiler(tools);compiler.authenticate();QTemporaryDir directory;
        auto source=[](const char *name){QFile f(QString(":/game/attacks/")+name);if(!f.open(QIODevice::ReadOnly))throw Error("Missing sound runtime source");return f.readAll();};
        Runtime r;QMap<QString,quint32> symbols{{"lc_audio_state",0x80511000}};
        const auto hooks=QJsonDocument::fromJson(source("hooks.json")).object()["hooks"].toArray();
        for(const auto &v:hooks){const auto h=v.toObject();const auto name=h["entry"].toString();
            if(name!="lc_attack_edit_sound_frame" && name!="lc_attack_edit_sound_flush" && name!="lc_attack_edit_sound_decode")continue;
            QByteArray code(16,0);const auto words=h["words"].toArray();
            put32(code,0,quint32(words[0].toInteger()));put32(code,4,quint32(words[1].toInteger()));
            put32(code,8,0x08000000|((quint32(h["address"].toInteger())+8)>>2&0x3ffffff));
            symbols[h["original"].toString()]=0x80510000+r.trampolines.size();r.trampolines+=code;
        }
        r.module=compiler.compileModule(source("sound-parts.c"),directory.path(),"sound_parts_preview",0x80500000,{},symbols,true);
        if(r.module.bytes.size()>0x10000)throw Error("Sound runtime exceeds preview code allocation");
        for(const auto &v:hooks){const auto h=v.toObject();const auto target=r.module.entries.value(h["entry"].toString());
            if(target)r.hooks[quint32(h["address"].toInteger())]=target;}
        runtime=std::move(r);
    }
    m.mount(runtime->module.address,runtime->module.bytes,true);m.mount(0x80510000,runtime->trampolines,true);
    m.mount(0x80511000,QByteArray(9248,0));
    for(auto i=runtime->hooks.begin();i!=runtime->hooks.end();++i){m.word(i.key(),0x08000000|(i.value()>>2&0x3ffffff));m.word(i.key()+4,0);}
    return runtime->module.entries.value("lc_audio_submit");
}
}
