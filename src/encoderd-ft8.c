#include "config.h"
#include "debuglog.h"
#include "ft8goblin_types.h"
#include <stdio.h>
#include <stdlib.h>

bool dying = false;
char *progname = "ft8encoder";

void exit_fix_config(void) {
   printf("Please edit your config.json and try again!\n");
   exit(255);
}

int main(int argc, char **argv) {
   printf("ft8encoder: Sub-process used to encode ft8 messages for transmission\n\n");

   if (argc != 4) {
      fprintf(stderr, "ERROR: Only THREE argument are expected: [bandname] [frequency] [message]!\n");
      fprintf(stderr, "All other settings are loaded from the configuration file config.json in the current directory or /etc/ft8goblin/\n");
      exit(1);
   }

   // This can't work without a valid configuration...
   if (!load_config())
      exit_fix_config();

   return 0;
}
