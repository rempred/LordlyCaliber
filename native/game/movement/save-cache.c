#include "save-cache.h"
/* Source byte24 and Legion byte9 are derived caches with three-bit retail save
 * fields. Rebuild them after unpacking; preserve the original packed layout.
 * Army aggregation follows BD154/43AD8: Soldiers use character0, and named
 * identities retain their linked record only while the current-class anchor matches.
 */
static unsigned char choose(const unsigned char *values, unsigned int count,
    const unsigned char *priority, unsigned int priority_count) {
    unsigned int i,j;
    for(i=0;i<priority_count;++i)for(j=0;j<count;++j)
        if(values[j]==priority[i])return priority[i];
    return 7;
}
void lc_refresh_movement_cache(unsigned char *sources,unsigned char *groups,
    const unsigned char *characters,const unsigned char *classes,unsigned int class_count,
    const unsigned char *priority,unsigned int priority_count) {
    unsigned int i,j;
    for(i=0;i<30;++i){
        unsigned char *source=sources+i*25;
        unsigned char values[5];unsigned int count=0;int valid=1;
        if(!source[1])continue;
        for(j=0;j<5;++j){
            unsigned int member=source[j+2],identity,current,selected;
            const unsigned char *character;
            if(!member)continue;
            if(member>=100)member=0;
            character=characters+member*56;identity=character[17];current=character[18];
            if(identity>=class_count||current>=class_count){valid=0;break;}
            selected=classes[identity*72+69]==current?identity:current;
            values[count++]=classes[selected*72+44];
        }
        if(valid)source[24]=choose(values,count,priority,priority_count);
    }
    for(i=0;i<10;++i){
        unsigned char *group=groups+i*11;unsigned char values[5];unsigned int count=0;int valid=1;
        /* Active player Legions have bit7 clear, unlike enemy groups. */
        if((group[1]&0x81)!=1)continue;
        for(j=0;j<5;++j){unsigned int member=group[j+2];if(member==255)continue;if(member>=30){valid=0;break;}values[count++]=sources[member*25+24];}
        if(valid)group[9]=choose(values,count,priority,priority_count);
    }
}
