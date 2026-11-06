#include "rebuild/retail_event_gate_runtime.h"
#include "engine/class_data.h"
namespace lordly {
namespace {
constexpr quint32 slot=0x66654,noop=0x80171b58;
const QByteArray marker("LCGATE01",8);
void authenticate(const Rom &source) {
    const auto previous=u32(source.bytes(),slot);
    if(previous==noop)return;
    const auto placement=ClassPlacement::read(source);
    if(!placement||previous<placement->residentAddress+8||
       previous>=quint64(placement->residentAddress)+placement->residentBytes||
       source.bytes().mid(previous-ClassPlacement::residentDelta-8,8)!=marker)
        throw Error("The event-VM opcode8 dispatch is owned by another modification");
}
}
CompiledModule buildRetailEventGateRuntime(const Compiler &compiler,const QString &directory,quint32 address) {
    if(address%16)throw Error("The event branch helper requires aligned resident storage");
    // These predicates use pure flag/property reads and cannot yield or call a
    // second interpreter. One private frame therefore brackets the inserted
    // condition without consuming any of the native VM's 32 shared stack slots.
    const QByteArray source=R"C(
extern unsigned short lc_event_gate_frame[];
int lc_event_gate_lookup(const unsigned char *program, unsigned short *registers,
                         unsigned int selector)
{
    unsigned int table, target, mode, i;
    const unsigned char *directory, *entry;
    table = ((unsigned int)program[0] << 8) | program[1];
    if (table < 1028U) return -1;
    directory = program + table - 1028U;
    if (directory[0] != 'L' || directory[1] != 'C' ||
        directory[2] != 'J' || directory[3] != 'G') return -1;
    entry = directory + 4 + (selector & 255U) * 4U;
    target = ((unsigned int)entry[0] << 8) | entry[1];
    mode = entry[2];
    if (entry[3] || mode > 2U || target < 6U || target * 2U >= table - 1028U) return -1;
    if (mode == 1U) {
        for (i = 0; i < 8U; ++i) lc_event_gate_frame[i] = registers[i];
    } else if (mode == 2U) {
        for (i = 0; i < 8U; ++i) registers[i] = lc_event_gate_frame[i];
    }
    return (int)target - 1;
}
)C";
    auto result=compiler.compileModule(source,directory,"retail_event_gate_lookup",address+16,
        {},{{"lc_event_gate_frame",address}});
    result.address=address;result.bytes.prepend(QByteArray(16,0));
    while(result.bytes.size()%16)result.bytes.append(char(0));
    result.bytes.append(marker);
    const quint32 bridge=address+quint32(result.bytes.size());
    const auto assembly=QByteArray(R"ASM(
.set noreorder
.set noat
.globl lc_event_gate_bridge
lc_event_gate_bridge:
addiu $sp,$sp,-32
sw $ra,28($sp)
addu $a0,$s7,$zero
addiu $a1,$sp,112
lbu $a2,129($sp)
jal lc_event_gate_lookup
nop
lw $ra,28($sp)
addiu $sp,$sp,32
bltz $v0,lc_event_gate_finish
nop
sh $v0,0($s2)
lc_event_gate_finish:
j lc_event_gate_increment
nop
)ASM");
    result.bytes+=compiler.assembleOwner(assembly,directory,"retail_event_gate_bridge",bridge,
        {{"lc_event_gate_lookup",result.entries.value("lc_event_gate_lookup")},
         {"lc_event_gate_increment",noop}});
    while(result.bytes.size()%16)result.bytes.append(char(0));
    result.entries["lc_event_gate_bridge"]=bridge;
    result.entries["lc_event_gate_frame"]=address;
    return result;
}
void linkRetailEventGateRuntime(QByteArray &image,quint32 bridge,const Rom &source) {
    authenticate(source);
    const auto current=u32(image,slot),previous=u32(source.bytes(),slot);
    if(current!=noop&&current!=previous&&current!=bridge)
        throw Error("Another export owner changed the event-VM opcode8 dispatch");
    if(bridge) {
        if(bridge<ClassPlacement::residentBegin+8||bridge>=ClassPlacement::residentEnd||
           image.mid(bridge-ClassPlacement::residentDelta-8,8)!=marker)
            throw Error("The event branch helper is outside its resident allocation");
    }
    put32(image,slot,bridge?bridge:noop);
}
}
