#ifndef PARSE_CONFIG_H
#define PARSE_CONFIG_H

int parse_config(void);

char *get_audio_device(void);

int get_num_sub_speakers(void);
int get_num_rear_speakers(void);
int get_num_front_speakers(void);

#endif /* PARSE_CONFIG_H */
