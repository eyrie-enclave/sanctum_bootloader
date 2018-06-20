#include "platform_interface.h"

static const char logo[] =
"RISCY\n"
"=====\n"
"\n";

long platform__disabled_hart_mask = 0;

const char *platform__get_logo(void)
{
  return logo;
}

int platform__use_htif(void)
{
  return 1;
}
