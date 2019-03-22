#include "binlog.h"
#include "em_core.h"

typedef struct {
  uint32_t time;
  const char *format;
  uint32_t arg1;
  uint32_t arg2;
} binlog_t;

#define BINLOG_LEN 64
binlog_t g_binlog[BINLOG_LEN];
uint32_t g_binlog_idx;

void binlog_init()
{
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  g_binlog_idx = 0;
  binlog("BOOT!", 0, 0);
}

void binlog(const char *format, uint32_t arg1, uint32_t arg2)
{
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; // This seems to get reset sometimes..

  uint32_t idx = __atomic_fetch_add(&g_binlog_idx, 1, __ATOMIC_RELAXED);
  g_binlog[idx & (BINLOG_LEN-1)] = (binlog_t){DWT->CYCCNT, format, arg1, arg2};
}
