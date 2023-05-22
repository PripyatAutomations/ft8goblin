#if	!defined(_cfg_h)
#define	_cfg_h
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <yajl/yajl_tree.h>
#include <sys/stat.h>
#include <errno.h>
#include "dict.h"

#define	PATHMAX_JSON	16
///////////////////////
// Some tunable bits //
///////////////////////
#define	MAX_MENULEVEL	8		// how deep can menus go?
#define	MAX_MENUNAME	32		// how long can the menu name be?
#define	MAX_MENUTITLE	128		// how long of a description is allowed?
#define	MAX_MENUITEMS	32		// how many items per menu?

#ifdef __cplusplus
extern "C" {
#endif
    // Global configuration pointer
    extern yajl_val cfg;
    extern dict     *runtime_cfg;
    // Parse the configuration and return a yajl tree
    extern yajl_val parse_config(const char *cfgfile);
    extern int free_config(yajl_val node);
    extern yajl_val load_config(void);
    extern int cfg_get_int(yajl_val cfg, const char *path);
    extern const char *cfg_get_str(yajl_val cfg, const char *path);
#ifdef __cplusplus
};
#endif

#endif	// !defined(_cfg_h)
