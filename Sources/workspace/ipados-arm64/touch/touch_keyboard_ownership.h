#pragma once
// SDL's hardware, text-input virtual keyboard and our touch controls have
// independent ownership. Releasing one source cannot lift another source.
enum { ICSM_KEYBOARD_TOUCH_SOURCE=0x08 };
static inline unsigned char ICSMKeyboardRemainingSources(unsigned char owners,unsigned char source) {
    return (unsigned char)(owners & (unsigned char)~source);
}
