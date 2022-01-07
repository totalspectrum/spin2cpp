extern char *x;
char *x = "hello";

int blah() {
    extern char *x;
    return *x++;
}
