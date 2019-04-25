#if defined(__STDC__) || defined(__cplusplus)
# define _P(s) s
#else
# define _P(s) ()
#endif


/* mkptypes.c */
Word *word_alloc _P((char *s));
void word_free _P((Word *w));
int List_len _P((Word *w));
Word *word_append _P((Word *w1, Word *w2));
int foundin _P((Word *w1, Word *w2));
void addword _P((Word *w, char *s));
void typefixhack _P((Word *w));
int ngetc _P((FILE *f));
int fnextch _P((FILE *f));
int nextch _P((FILE *f));
int getsym _P((char *buf, FILE *f));
int skipit _P((char *buf, FILE *f));
int is_type_word _P((char *s));
Word *typelist _P((Word *p));
Word *getparamlist _P((FILE *f));
void emit _P((Word *wlist, Word *plist, char *fname, long startline));
void getdecl _P((FILE *f, char *fname));
int main _P((int argc, char **argv));
void Usage _P((void));

#undef _P
