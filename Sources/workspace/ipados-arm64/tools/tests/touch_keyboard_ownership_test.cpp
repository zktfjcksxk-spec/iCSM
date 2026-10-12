#include "../../touch/touch_keyboard_ownership.h"
#include <cassert>
int main() {
    constexpr unsigned char hardware=1,virtualKey=2,touch=ICSM_KEYBOARD_TOUCH_SOURCE;
    assert(ICSMKeyboardRemainingSources(touch,touch)==0);
    assert(ICSMKeyboardRemainingSources(hardware|touch,touch)==hardware);
    assert(ICSMKeyboardRemainingSources(hardware|touch,hardware)==touch);
    assert(ICSMKeyboardRemainingSources(hardware|virtualKey|touch,virtualKey)==(hardware|touch));
    assert(ICSMKeyboardRemainingSources(hardware,touch)==hardware);
    assert(ICSMKeyboardRemainingSources(touch,hardware)==touch);
    unsigned char held=hardware|touch;
    held=ICSMKeyboardRemainingSources(held,touch);assert(held==hardware);
    held=ICSMKeyboardRemainingSources(held,hardware);assert(held==0);
    held=hardware|touch;held=ICSMKeyboardRemainingSources(held,hardware);assert(held==touch);
    held=ICSMKeyboardRemainingSources(held,touch);assert(held==0);
}
