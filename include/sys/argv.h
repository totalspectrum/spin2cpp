#ifndef _SYS_ARGV_H
#define _SYS_ARGV_H
#pragma once

#include <compiler.h>

#define MAX_ARGC 32
#define ARGV_MAGIC ('A' | ('R' << 8) | ('G'<<16) | ('v'<<24))

#define ARGV_ADDR  0xFC000
#define START_ARGS ((char *)(ARGV_ADDR+4))
#define END_ARGS   ((char *)(ARGV_ADDR+0xFFF))

const char **__fetch_argv(int *argc_ptr) _IMPL("libsys/argv.c");

#endif
