typedef struct { int a, b, c, d; } Node;

#if 0
#include <stdio.h>
#include <stdlib.h>

void main() {
    int size = 1;
    int lsize = 0;
    if (lsize >  ( ((int)((sizeof(int) * 8 - 1))) - 1)  || (1u << lsize) >  (( ((size_t)((1u << ( ((int)((sizeof(int) * 8 - 1))) - 1) ))) <= ((size_t)(~(size_t)0)) /sizeof(Node)) ? (1u << ( ((int)((sizeof(int) * 8 - 1))) - 1) ) : ((unsigned int)((( ((size_t)(~(size_t)0)) /sizeof(Node))))) ) )
    {
      printf("?? size=%d lsize=%d\n", size, lsize);
    }
    else
    {
        printf("good\n");
    }
}
#else
void main() {
    int c = ((unsigned int)(~(unsigned int)0)) / sizeof(Node);  
//    printf("%d\n",c);
    _OUTA = c;
}
#endif
