#include <stdio.h>
#include <stdint.h>
#include <propeller.h>

char pattern[] = { '.','-','+','x','o','X','O','#' };
#define patterns (sizeof pattern)

int64_t c4=4<<28;

void myexit(int n)
{
    _txraw(0xff);
    _txraw(0x0);
    _txraw(n);
    waitcnt(getcnt() + 40000000);
#ifdef __OUTPUT_BYTECODE__
    _cogstop(1);
    _cogstop(_cogid());
#else
    __asm {
        cogid n
        cogstop n
    }
#endif    
}

void main(int argc, char *argv[] ) {

  int64_t xmin=-2<<28;
  int64_t xmax= 2<<28;

  int64_t ymin=-2<<28;
  int64_t ymax= 2<<28;

  int64_t maxiter=32;

  int64_t dx=(xmax-xmin)/79;
  int64_t dy=(ymax-ymin)/32;

  int64_t delta = xmax-xmin;
  
  int64_t cx,cy,iter,x,y,x2,y2;

  for(cy=ymax;cy>=ymin;cy-=dy) {
    for(cx=xmin;cx<=xmax;cx+=dx) {
      x=0;
      y=0;
      for(iter=0;iter<maxiter;iter++) {
	x2=(x*x)>>28;
	y2=(y*y)>>28;
        //printf("  iter = %llx\n", iter);
	if((x2+y2)>c4) break;
	y=((x*y)>>27)+cy;
	x=x2-y2+cx;
      }
      //printf("cx=%llx cy=%llx iter=%lld\n", cx, cy, iter);
      //_waitms(200);
      //int i = iter % patterns;
      //printf("i = %d iter=%lld patterns=%d\n", i, iter, patterns);
      putchar(pattern[iter%patterns]);
    }
    putchar('\n');
  }
  myexit(0);
}
