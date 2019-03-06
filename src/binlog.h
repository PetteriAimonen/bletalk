// Fast binary logging system that can be read out by debugger.

#pragma once

#include <stdint.h>

void binlog_init();
void binlog(const char *format, uint32_t arg1, uint32_t arg2);
