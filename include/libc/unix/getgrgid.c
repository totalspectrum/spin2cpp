#include <string.h>
#include <sys/types.h>
#include <grp.h>

#define NUMOF(a) ((int)(sizeof(a) / sizeof(a[0])))

static char *root_members[] = {
    "root",
    NULL
};
    
static struct group groups[] = {
    { "root", 0, root_members },
};

struct group *getgrgid(gid_t gid)
{
    int i;
    for (i = 0; i < NUMOF(groups); i++) {
        if (gid == groups[i].gr_gid) {
            return &groups[i];
        }
    }
    return 0;
}
