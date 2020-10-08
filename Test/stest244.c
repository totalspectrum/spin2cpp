struct blah {
    unsigned r:4;
    unsigned g:4;
    unsigned b:4;
};

struct blah foo = { .b=0xc, .g=0xb, .r=0xa };
//struct blah foo = { 0xa, 0xb, 0xc };

unsigned getg()
{
    return foo.b;
}
