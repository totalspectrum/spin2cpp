long long line;

long long bump1() {
    long long x = line++;
    return x;
}

void bump2() {
    ++line;
}

long long bump3() {
    long long x = ++line;
    return x;
}
