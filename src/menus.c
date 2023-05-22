#if	0
// main menu metadata
menu_t menu_main = { "Main Menu", "This is the main menu", menu_main_items };

// XXX: This needs generated from the config, but some examples to get started...
menu_item_t menu_bands_items[] = {
   { "160 Meters FT8", "Toggle RX/TX on ft8-160m", NULL },
   { "80 Meters FT8", "Toggle RX/TX on ft8-80m", NULL },
   { "60 Meters FT8", "Toggle RX/TX on ft8-60m", NULL },
   { "40 Meters FT8", "Toggle RX/TX on ft8-40m", NULL },
   { "30 Meters FT8", "Toggle RX/TX on ft8-30m", NULL },
   { "20 Meters FT8", "Toggle RX/TX on ft8-20m", NULL },
   { "17 Meters FT8", "Toggle RX/TX on ft8-17m", NULL },
   { "15 Meters FT8", "Toggle RX/TX on ft8-15m", NULL },
   { "12 Meters FT8", "Toggle RX/TX on ft8-12m", NULL },
   { "10 Meters FT8", "Toggle RX/TX on ft8-10m", NULL },
   { "6 Meters FT8", "Toggle RX/TX on ft8-6m", NULL },
   { (char *)NULL, (char *)NULL, 0 }
};

menu_t menu_bands = { "Band Settings", "Configure RX and TX bands here", menu_bands_items };
#endif
