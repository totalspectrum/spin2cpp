#include <stdint.h>

union f_or_i {
  int32_t i;
  float f;
};

class FloatNative {
private:
  int32_t asInt(float x) {
    union f_or_i u;
    u.f = x;
    return u.i;
  }
  float asFloat(int32_t v) {
    union f_or_i u;
    u.i = v;
    return u.f;
  }

public:
  void Start() {
  }

  int32_t Ffloat(int32_t x) {
    return asInt((float)x);
  }

  int32_t Fadd(int32_t a, int32_t b) {
    return asInt(asFloat(a) + asFloat(b));
  }
  int32_t Fsub(int32_t a, int32_t b) {
    return asInt(asFloat(a) - asFloat(b));
  }
  int32_t Fmul(int32_t a, int32_t b) {
    return asInt(asFloat(a) * asFloat(b));
  }
  int32_t Fdiv(int32_t a, int32_t b) {
    return asInt(asFloat(a) / asFloat(b));
  }
};
