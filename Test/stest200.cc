typedef class bar {
public:
  int getx() { return x; }
  int pubint;
private:
  int x;
} Bar;

int foo(Bar *y) {
  return y->getx();
}
