/*
 * user_settings.h
 *
 *  Created on: 09.06.2015
 *      Author: Max
 */

#ifndef USER_USER_SETTINGS_H_
#define USER_USER_SETTINGS_H_

void initSettings(void);
struct settings getSettings(void);

struct settings {
	uint8 write_flag;
	char ssid[32];
	char password[64];
	char socket_name[16];
};

#endif /* USER_USER_SETTINGS_H_ */
