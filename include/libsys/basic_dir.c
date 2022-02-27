/*
 * directory handling for BASIC
 */
#include <compiler.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

#define fbReadOnly  0x01
#define fbHidden    0x02
#define fbSystem    0x04
#define fbDirectory 0x10
#define fbArchive   0x20
#define fbNormal    (fbReadOnly | fbArchive)

static int getlower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

/*
 * simple pattern matching:
 *   * or *.* matches everything
 *   *xyz matches anything ending in xyz
 *   xyz* matches anything starting in xyz
 */

int _pat_match(const char *name, const char *pattern)
{
    int cname, cpattern;
    int name_len, pat_len;
    
    while (*pattern && *name && *pattern != '*') {
        cpattern = getlower(*pattern);
        cname = getlower(*name);
        if (cname != cpattern) return 0;
        name++;
        pattern++;
    }
    if (*pattern == '*') {
        const char *tmp;
        int i;
        if (pattern[1] == '.' && pattern[2] == '*' && !pattern[3]) {
            return 1;
        }
        /* check for tail */
        pattern++;
        for (pat_len = 0; pattern[pat_len] != 0; pat_len++) ;
        for (name_len = 0; name[name_len] != 0; name_len++) ;
        if (name_len < pat_len) return 0;
        name += (name_len - pat_len);
        while (pat_len > 0) {
            if (getlower(*name) != getlower(*pattern)) return 0;
            name++; pattern++;
            --pat_len;
        }
    }
    if (*pattern == 0 && *name == 0) {
        return 1;
    }
    return 0;
}

char *_basic_dir(const char *pattern = 0, unsigned attrib = 0) {
    static DIR* olddir = 0;
    static unsigned oldattrib = 0;
    static const char *oldpattern = "";
    struct dirent *ent;
    struct stat sbuf;
    int r;
    unsigned mode;
    char *string;
    
    if (pattern && *pattern) {
        // start a new search
        if (olddir) closedir(olddir);
        olddir = opendir(".");
        oldattrib = attrib;
        oldpattern = pattern;
    }
    if (!olddir) {
#ifdef _DEBUG
        __builtin_printf("_basic_dir: olddir is NULL\n");
#endif        
        return "";
    }
    for(;;) {
        ent = readdir(olddir);
        if (!ent) {
            closedir(olddir);
            olddir = 0;
#ifdef _DEBUG
            __builtin_printf("_basic_dir: entry is NULL\n");
#endif        
            return "";
        }
#ifdef _DEBUG
        __builtin_printf("_basic_dir: entry name is %s\n", ent->d_name);
#endif        
        /* see if the entry matches the pattern */
        if (!_pat_match(ent->d_name, oldpattern)) continue;
        /* now see if the entry matches the attributes */
        if (oldattrib != 0) {
            r = stat(ent->d_name, &sbuf);
            if (r) {
                // error in stat, break
                return "";
            }
            mode = sbuf.st_mode & S_IFMT;
            if (ent->d_name[0] == '.') {
                attrib = fbHidden;
            } else {
                attrib = 0;
            }
            if ( mode == S_IFDIR ) {
                attrib |= fbDirectory;
            } else if ( mode == S_IFCHR || mode == S_IFBLK || mode == S_IFIFO) {
                attrib |= fbSystem;
            } else {
                if (0 == (sbuf.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH))) {
                    attrib |= fbReadOnly;
                } else if (mode != S_IFDIR) {
                    attrib |= fbArchive;
                }
            }
            if ( 0 == (attrib & oldattrib) ) {
                continue;
            }
#ifdef _DEBUG
            __builtin_printf("attrib=0x%x oldattrib=0x%x\n", attrib, oldattrib);
#endif            
        }
        string = _gc_alloc_managed(strlen(ent->d_name)+1);
        if (string) {
            strcpy(string, ent->d_name);
        }
        return string;
    }
}
