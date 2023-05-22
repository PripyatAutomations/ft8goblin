#if	!defined(_tui_menu_h)
#define	_tui_menu_h
#include "config.h"
#include <libied/cfg.h>
#ifdef __cplusplus
extern "C" {
#endif
   ///////////////////
   // forward declare this, so we can use it in menu_t and menu_history_t below
   typedef struct menu_item menu_item_t;

   typedef struct menu_s {
     char	menu_name[MAX_MENUNAME];
     char	menu_title[MAX_MENUTITLE];
     menu_item_t *menu_items[MAX_MENUITEMS];
   } menu_t;

   struct menu_item {
     char	item_name[MAX_MENUNAME];
     char	item_title[MAX_MENUTITLE];
     int	(*callback)();		// Callback to fire when clicked
     /////////////////////////////////////////////////
     // Only one of these can be selected at a time //
     /////////////////////////////////////////////////
     menu_t *item_menu;		// menu target (menu pointer)
     int	  item_menu_item;	// menu item (menu pointer)
   };

   typedef struct menu_history {
      menu_t *menu;
      int	   item;		// menu.menu_items[x]
   } menu_history_t;

   typedef struct menu_window {
      char *name;
      char *title;
      int	x, y;			// x, y of origin of window
      int width, height;		// size of dialog
   } menu_window_t;

   extern menu_history_t menu_history[MAX_MENULEVEL];	// history for ESC (go back) in menus ;)
   extern int menu_history_clear(void);
   extern void menu_history_push(menu_t *menu, int item);
   extern void menu_history_pop(void);
   extern menu_window_t *menu_window_render(menu_t *menu, int menu_entries);
   extern int menu_item_render(menu_window_t *mp, menu_t *menu, int menu_item);
   extern int menu_close(void);
   extern int menu_show(menu_t *menu, int item);

   // globals from tui-menu.c
   extern menu_history_t menu_history[MAX_MENULEVEL];
   extern int menu_level;
   extern menu_item_t menu_main_items[];
   extern menu_item_t menu_bands_items[];
   extern menu_t menu_main;
   extern menu_t menu_bands;
#ifdef __cplusplus
};
#endif
#endif	// !defined(_tui_menu_h)
