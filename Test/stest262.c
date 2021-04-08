static const char * const msgs[] = {
    "hello",
    "goodbye",
    0
};

const char *func1()
{
    return msgs[ina & 1];
}
