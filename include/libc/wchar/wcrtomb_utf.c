#include <wchar.h>
#include <stdint.h>
#include <errno.h>

size_t
wcrtomb(char *s, wchar_t wcorig, mbstate_t *ps)
{
  uint32_t wc = wcorig;
  uint32_t count;
  uint32_t left;
  uint32_t init;

  if (wc <= 0x7f) {
    init = wc;
    wc = 0;
    count = 1;
  } else if (wc <= 0x7ff) {
    /* 11 bits valid */
    /* shift up by 21 bits, then down by 3 */
    init = 0xC0;
    wc = (wc << 18);
    count = 2;
  } else if (wc <= 0xffff) {
    /* 16 bits valid; shift up by 16 bits, down by 4 */
    init = 0xE0;
    wc = (wc << 12);
    count = 3;
  } else if (wc <= 0x10ffff) {
    /* 21 bits (perhaps) valid */
    /* shift up by 11, down by 5 */
    init = 0xF0;
    wc = (wc << 6);
    count = 4;
  } else {
    errno = EILSEQ;
    return (size_t)-1;
  }

  left = count;
  do {
    if (s) *s++ = (init | ((wc >> 24) & 0x3F));
    wc = wc << 6;
    init = 0x80;
    --left;
  } while (left > 0);

  return count;
}

#ifdef TEST
#include <string.h>
#include <stdio.h>
static void test(wchar_t x)
{
  size_t i, n;
  unsigned char buf[8];

  memset(buf, 0, sizeof(buf));
  n = _wcrtomb_utf8((char *)buf, x, NULL);
  printf("test of 0x%x returned %lu chars=%2x %2x %2x %2x\n",
	 x, n, buf[0], buf[1], buf[2], buf[3]);
}

int
main()
{
  test(0x20);
  test(0x7f);
  test(0x80);
  test(0xfd);   /* should produce C3 BD */
  test(0x20ac); /* should produce E2 82 AC */
  test(0xffff); /* should produce EB BF BF */
  test(0x024B62); /* should produce F0 A4 AD A2 */
  return 0;
}
#endif
