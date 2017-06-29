#ifndef PYBIDNINGS_H
#define PYBIDNINGS_H
#include "level.h"

struct level_ops *pyb_load(const char *path);
void pyb_interactive();

#endif
