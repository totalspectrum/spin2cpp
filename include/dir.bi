const fbReadOnly = &h01
const fbHidden   =  &h02
const fbSystem   =  &h04
const fbDirectory = &h10
const fbArchive   = &h20
const fbNormal    = (fbReadOnly or fbArchive)
