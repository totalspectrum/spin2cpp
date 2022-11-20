#include <stdint.h>

int x;

int update_stuff(uint16_t& siz) {
    siz += x;
    return 0;
}
