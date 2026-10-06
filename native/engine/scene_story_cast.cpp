#include "engine/scene_story_cast.h"
#include "engine/save_document.h"
namespace lordly {
bool SceneStoryCast::supports(const DirectorProgram &program) {
    QSet<int> indexes, sources, owners;
    for (const auto &command : program.commands) {
        for (auto word : command.operands)
            if ((word & 0xffffff00U) == 0x08880000U)
                indexes.insert(word & 255);
        if (command.opcode == 0x2a && command.operands.size() == 6) {
            const auto primary = command.operands[1], secondary = command.operands[5];
            if (primary >= 0x08880001U && primary <= 0x08880005U && secondary == primary + 5) {
                sources.insert(primary & 255);
                owners.insert(secondary & 255);
            }
        }
    }
    if (indexes.size() != 17 || sources.size() != 5 || owners.size() != 5)
        return false;
    for (int i = 0; i <= 16; ++i)
        if (!indexes.contains(i))
            return false;
    return true;
}
QVector<int> SceneStoryCast::choices() {
    return {0x56, 0x57, 0x54, 0x55, 0x61, 0x58, 0x5b, 0x5c, 0x59, 0x5a, 0x5e, 0x77,
            0x7a, 0x78, 0x75, 0x7b, 0x5d, 0x79, 0x5f, 0x60, 0x76, 0x7c, 4,    2};
}
std::array<StoryCastMember, 5> SceneStoryCast::sample() {
    return {{{0x58, 0x58}, {0x59, 0x59}, {0x5a, 0x5a}, {0x5b, 0x5b}, {0x5c, 0x5c}}};
}
std::array<StoryCastMember, 5> SceneStoryCast::saved(const SaveDocument &save, int classCount) {
    // Event source-bank getters ROM46314/46324, resident RAM80170414/80170424.
    // The current-ROM save descriptor preserves all ten bytes. These are the
    // saved launch inputs, not a new execution of event67's army selection.
    const auto &memory = save.memory();
    if (memory.size() < 0x193686)
        throw Error("The selected save has no complete story launch cast");
    std::array<StoryCastMember, 5> cast;
    const auto allowed = choices();
    for (int i = 0; i < 5; ++i) {
        cast[i] = {u8(memory, 0x19367c + i), u8(memory, 0x193681 + i)};
        if (!allowed.contains(cast[i].primary) || cast[i].primary >= classCount ||
            !cast[i].secondary || cast[i].secondary >= classCount)
            throw Error("The selected save has no compatible five-person story launch cast; "
                        "choose preview classes manually");
    }
    return cast;
}
DirectorLaunchTranslations
SceneStoryCast::translations(const std::array<StoryCastMember, 5> &cast) {
    // Event66's fixed variant/portrait and its primary-class dialogue classifier.
    // Event67 supplies five paired classes; the preview declares those pairs.
    const auto allowed = choices();
    DirectorLaunchTranslations result;
    result.set(0, 2);
    result.set(16, 83);
    for (int i = 0; i < 5; ++i) {
        const int primary = cast[i].primary, secondary = cast[i].secondary;
        if (!allowed.contains(primary) || secondary == 0)
            throw Error("Invalid story preview class pair");
        int dialogue = 20;
        switch (primary) {
        case 0x56:
        case 0x57:
            dialogue = 0;
            break;
        case 0x54:
        case 0x55:
            dialogue = 1;
            break;
        case 0x61:
            dialogue = 2;
            break;
        case 0x58:
            dialogue = 3;
            break;
        case 0x5b:
            dialogue = 4;
            break;
        case 0x5c:
            dialogue = 5;
            break;
        case 0x59:
            dialogue = 6;
            break;
        case 0x5a:
            dialogue = 7;
            break;
        case 0x5e:
            dialogue = 8;
            break;
        case 0x77:
            dialogue = 9;
            break;
        case 0x7a:
            dialogue = 10;
            break;
        case 0x78:
            dialogue = 11;
            break;
        case 0x75:
            dialogue = 12;
            break;
        case 0x7b:
            dialogue = 13;
            break;
        case 0x5d:
            dialogue = 14;
            break;
        case 0x79:
            dialogue = 15;
            break;
        case 0x5f:
        case 0x60:
            dialogue = 16;
            break;
        case 0x76:
            dialogue = 17;
            break;
        case 0x7c:
            dialogue = 18;
            break;
        case 4:
            dialogue = 19;
            break;
        }
        result.set(i + 1, primary);
        result.set(i + 6, secondary);
        result.set(i + 11, dialogue);
    }
    return result;
}
} // namespace lordly
