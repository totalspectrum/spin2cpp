//
// zip file output for spin2cpp
//
// Copyright 2023 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include "common.h"
#include "cmdline.h"
#include "zip.h"

void
OutputZipFile(const char *zipname)
{
    SourceFile *sf;
    int i;
    struct zip_t *zip;
    const char *zipEntryName;
    
    zip = zip_open(zipname, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    if (!zip) {
        perror(zipname);
        gl_errors++;
        return;
    }
    for (i = 0; i < numSourceFiles; i++) {
        sf = sourceData + i;
        if (sf->isSysFile) continue;
        zipEntryName = sf->shortName;
#ifdef WIN32
        if (zipEntryName[0] && zipEntryName[1] == ':') {
            zipEntryName += 2;
        }
        while (zipEntryName[0] == '\\') zipEntryName++;
#endif        
        while (zipEntryName[0] == '/') zipEntryName++;
        
        zip_entry_open(zip, zipEntryName);
        if (zip_entry_fwrite(zip, sf->fullName) != 0) {
            ERROR(NULL, "Unable to write file %s to zip", sf->fullName);
            zip_close(zip);
            remove(zipname);
            return;
        }
        zip_entry_close(zip);
    }
    zip_close(zip);
}
