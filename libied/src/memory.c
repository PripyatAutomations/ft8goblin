/*
 * memory management related utilities
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Force garbage collection
bool mm_do_gc(void) {
    return false;
}

