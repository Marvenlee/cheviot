#ifndef KERNEL_UTILITY_H
#define KERNEL_UTILITY_H

#include <kernel/types.h>
#include <stdarg.h>

typedef struct {
  int separator_cnt;
  char *input;
  char *current;
  char *separators;
  char *sepval;
  char *seppos;
} tokenizer_t;

void InitTokenizer(tokenizer_t *tokenizer, char *input, char *separators);
char *NextToken(tokenizer_t *tokenizer);

int Snprintf(char *str, size_t size, const char *format, ...);
int Vsnprintf(char *str, size_t size, const char *format, va_list args);

char *StrDup(const char *src);
size_t StrLen(const char *s);
int StrCmp(const char *s, const char *t);
char *StrChr(char *str, char ch);
int AtoI(const char *str);

size_t StrLCpy(char *dst, const char *src, size_t size);
size_t StrLCat(char *dst, const char *src, size_t size);

void DoPrintf(void (*printchar_fp)(char, void *), void *printchar_arg,
              const char *format, va_list *ap);
#endif
