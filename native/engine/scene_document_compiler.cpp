#include "engine/scene_document_compiler.h"
#include <QFile>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <cmath>
namespace lordly {
struct SceneDocumentCompiler::Impl {
    QJSEngine engine;
    QJSValue compile;
    QJSValue checked(QJSValue value) {
        if (value.isError() || engine.hasError() ||
            (value.isObject() && value.hasProperty("message"))) {
            if (engine.hasError())
                value = engine.catchError();
            throw Error("Legacy scene conversion: " + value.property("message").toString() + " (" +
                        value.property("fileName").toString() + ":" +
                        value.property("lineNumber").toString() + ")");
        }
        return value;
    }
    Impl() {
        checked(engine.evaluate("var window = this; var OB64 = {};", "legacy-bootstrap"));
        for (const auto name : {"cutscene-model.js", "cutscene-director.js", "cutscene-codec.js"}) {
            QFile file(":/legacy/" + QString::fromLatin1(name));
            if (!file.open(QIODevice::ReadOnly))
                throw Error("Missing embedded legacy scene compiler");
            checked(engine.evaluate(QString::fromUtf8(file.readAll()), QString::fromLatin1(name)));
        }
        compile = checked(engine.evaluate(R"JS((function(inputText) {
            var input = JSON.parse(inputText), document = input.document;
            var scene = {assetId: document.native.sourceAssetId, sceneId: document.identity.sceneId,
                source: {decodedLength: input.words.length * 4, decodedWordCount: input.words.length,
                    reparsedForAuthoring: input.reparsed, registeredWaits: input.waits},
                launchProfile: {directorMode: {value: input.mode < 0 ? null : input.mode}}};
            OB64.cutsceneData = {directorGrammar: input.grammar};
            var bytes = new Uint8Array(input.words.length * 4);
            input.words.forEach(function(word, i) {
                bytes[i*4] = word >>> 24; bytes[i*4+1] = word >>> 16;
                bytes[i*4+2] = word >>> 8; bytes[i*4+3] = word;
            });
            var result = OB64.cutsceneCodec.compileSceneDocument(scene,
                {decodedBytes: bytes, nodeDefinitions: input.nodes}, document);
            return Array.prototype.map.call(result.decodedBytes, function(byte) {
                return byte.toString(16).padStart(2, '0');
            }).join('');
        }))JS",
                                          "legacy-scene-converter"));
    }
};
const QJsonObject &SceneDocumentCompiler::catalog() {
    static const auto rows = [] {
        QFile file(":/game/director/legacy-scenes.json");
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing legacy scene identity catalog");
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        if (root["version"] != 1 || !root["scenes"].isObject())
            throw Error("Invalid legacy scene identity catalog");
        return root["scenes"].toObject();
    }();
    return rows;
}
SceneDocumentCompiler::SceneDocumentCompiler() : impl_(std::make_unique<Impl>()) {}
SceneDocumentCompiler::~SceneDocumentCompiler() = default;
QByteArray SceneDocumentCompiler::compile(const DirectorProgram &source,
                                          const QJsonObject &document, int launchMode) {
    const auto native = document["native"].toObject();
    if (!native["commands"].isArray() || !native["gaps"].isArray() ||
        !native["gaps"].toArray().isEmpty())
        throw Error("Imported scene has missing or unsupported source boundaries");
    const auto commands = native["commands"].toArray();
    if (commands.size() != source.commands.size())
        throw Error("Imported scene command count differs from its ROM preimage");
    QJsonArray words, nodes, grammar;
    QSet<QString> nodeIds;
    for (int i = 0; i < commands.size(); ++i) {
        const auto saved = commands[i].toObject(), origin = saved["source"].toObject();
        const auto &actual = source.commands[i];
        const auto raw = saved["words"].toArray();
        const auto nodeId = origin["nodeId"].toString();
        if (nodeId.isEmpty() || nodeIds.contains(nodeId) ||
            raw.size() != actual.operands.size() + 1 ||
            origin["startWord"].toDouble(-1) != actual.word ||
            origin["endWord"].toDouble(-1) != actual.word + raw.size())
            throw Error("Imported scene lost an exact source-command boundary");
        nodeIds.insert(nodeId);
        for (int word = 0; word < raw.size(); ++word) {
            const auto expected = word ? actual.operands[word - 1] : actual.opcode;
            if (!raw[word].isDouble() || raw[word].toDouble() != expected)
                throw Error("Imported scene source words differ from the loaded ROM");
            words << qint64(expected);
        }
        const auto &definition = Directors::grammar()[actual.opcode];
        QJsonArray roles;
        for (const auto &role : definition.operands)
            roles << role;
        nodes << QJsonObject{{"id", nodeId},
                             {"startWord", actual.word},
                             {"endWord", actual.word + raw.size()},
                             {"wordCount", raw.size()},
                             {"opcodeU32", qint64(actual.opcode)},
                             {"name", definition.name},
                             {"operandRoles", roles},
                             {"queryRecordKind", definition.queryWords == 4 ? "Q4"
                                                 : definition.queryWords == 3
                                                     ? "Q3"
                                                     : QJsonValue(QJsonValue::Null)},
                             {"editPolicy", origin["editPolicy"]},
                             {"insertBefore", origin["insertBefore"]},
                             {"semanticSummary", definition.description}};
    }
    for (const auto &definition : Directors::grammar())
        grammar << QJsonObject{
            {"opcodeU32", qint64(definition.opcode)},
            {"parser",
             QJsonObject{{"directorModeRequirement", definition.requiredMode < 0
                                                         ? QJsonValue(QJsonValue::Null)
                                                         : QJsonValue(definition.requiredMode)}}}};
    const auto reference =
        catalog()[document["identity"].toObject()["sceneId"].toString()].toObject();
    const auto original =
        !reference.isEmpty() &&
        sha256(source.source).toUpper() == reference["decodedSha256"].toString().toLatin1();
    if (launchMode < 0)
        launchMode = reference["mode"].toInt(-1);
    const auto input =
        QJsonDocument(
            QJsonObject{
                {"document", document},
                {"mode", launchMode},
                {"words", words},
                {"nodes", nodes},
                {"grammar", grammar},
                {"reparsed", !original || reference["reparsedForAuthoring"].toBool()},
                {"waits", original ? reference["registeredWaits"].toArray() : QJsonArray{}}})
            .toJson(QJsonDocument::Compact);
    const auto result = impl_->checked(impl_->compile.call({QString::fromUtf8(input)}));
    if (!result.isString())
        throw Error("Legacy compiler returned an invalid scene program: " + result.toString());
    const auto hex = result.toString().toLatin1(), bytes = QByteArray::fromHex(hex);
    if (bytes.toHex() != hex || bytes.size() > 16 * 1024 * 1024)
        throw Error("Legacy compiler returned invalid scene bytes");
    const auto parsed = DirectorProgram::decode(bytes);
    if (parsed.resourceClass() != source.resourceClass())
        throw Error("Imported scene changed its terminal resource class");
    return bytes;
}
} // namespace lordly
