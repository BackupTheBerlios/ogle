#ifndef INTERPRET_CONFIG_H
#define INTERPRET_CONFIG_H


void init_interpret_config(char *pgm_name,
		      void(*keybinding_cb)(char *, char *),
		      void(*dvd_path_cb)(char *));

int interpret_config(void);


#endif /* INTERPRET_CONFIG_H */
