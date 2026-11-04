/* Resident event-command writer. This owner cannot call world-overlay code:
 * events also run when that overlay is absent. Only these 40 bits are saved. */
void campaign_event_road_set(unsigned int id,unsigned int open) {
    unsigned char *flags=(unsigned char *)0x80196a81;
    unsigned int bit,mask;
    bit=(id&255)-1;
    if(bit>=40)return;
    mask=1<<(bit&7);
    flags[bit>>3]=(flags[bit>>3]&~mask)|((open&1)<<(bit&7));
}
