char buf[80] = "ABC";

int fetch() {
    char& p = buf[1];
    int x = p;
    p = 'X';
    return x;
}
