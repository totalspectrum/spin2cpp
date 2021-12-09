#define fbReadOnly  &h01
#define fbHidden    &h02
#define fbSystem    &h04
#define fbDirectory &h10
#define fbArchive   &h20
#define fbNormal    (fbReadOnly or fbArchive)

declare function dir$ lib "libsys/dir.bas" (pat = "" as string, attrib = 0 as integer) as string

declare function curdir$ lib "libsys/dir.bas" () as string
