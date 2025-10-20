//
// zip file output for spin2cpp
//
// Copyright 2023 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include "common.h"
#include "cmdline.h"
#include "zip.h"

static const char *
ShortZipName(SourceFile *sf, const char *root, size_t rootlen)
{
    if (!strncmp(sf->fullName, root, rootlen)) {
        return sf->fullName + rootlen;
    }
    if (!strcmp(sf->fullName, sf->shortName)) {
        return sf->fullName;
    }
    return strdupcat("library/", sf->shortName);
}

void
OutputZipFile(const char *zipname)
{
    SourceFile *sf;
    int i;
    struct zip_t *zip;
    const char *zipEntryName;
    char *rootname;
    char *ptr, *lastslash;
    size_t rootnamelen;
    
    if (numSourceFiles == 0)
        return;
    
    zip = zip_open(zipname, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    if (!zip) {
        perror(zipname);
        gl_errors++;
        return;
    }
    ptr = rootname = strdup(sourceData[0].fullName);
    lastslash = 0;
    while (*ptr) {
        if (*ptr == '/'
#ifdef WIN32
            || *ptr == '\\'
#endif
            )
            lastslash = ptr;
        ptr++;
    }
    if (lastslash) {
        lastslash[1] = 0;
    } else {
        rootname = "";
    }
    rootnamelen = strlen(rootname);

    for (i = 0; i < numSourceFiles; i++) {
        sf = sourceData + i;
        if (sf->isSysFile) continue;
        zipEntryName = ShortZipName(sf, rootname, rootnamelen);
#ifdef WIN32
        if (zipEntryName[0] && zipEntryName[1] == ':') {
            zipEntryName += 2;
        }
        while (zipEntryName[0] == '\\') zipEntryName++;
#endif        
        while (zipEntryName[0] == '/') zipEntryName++;
        
        zip_entry_open(zip, zipEntryName);
        if (zip_entry_fwrite(zip, sf->fullName) != 0) {
            ERROR(sf->whereFrom, "Unable to write file %s to zip", sf->fullName);
            zip_close(zip);
            remove(zipname);
            return;
        }
        zip_entry_close(zip);
    }
    zip_close(zip);
}
