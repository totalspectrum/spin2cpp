#include <string.h>
#include <sys/types.h>
#include <pwd.h>

#define NUMOF(a) ((int)(sizeof(a) / sizeof(a[0])))

static struct passwd users[] = {
    { "root", 0, 0, "/" },
};

struct passwd *getpwnam(const char *name)
{
    int i;
    for (i = 0; i < NUMOF(users); i++) {
        if (!strcmp(name, users[i].pw_name)) {
            return &users[i];
        }
    }
    return 0;
}

struct passwd *getpwuid(uid_t uid)
{
    int i;
    for (i = 0; i < NUMOF(users); i++) {
        if (uid == users[i].pw_uid) {
            return &users[i];
        }
    }
    return 0;
}

