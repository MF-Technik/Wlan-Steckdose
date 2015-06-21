/*
 * user_settings.c
 *
 *  Created on: 09.06.2015
 *      Author: Max
 */
#include <os_type.h>

#include "user_settings.h"

struct settings setting;

void ICACHE_FLASH_ATTR
initSettings(void)
{
	setting.write_flag = 0;
	//os_memset(setting.password, 0, sizeof(setting.password));
	//setting.ssid = "";
	//setting.socket_name = "Steckdose";
}

struct settings ICACHE_FLASH_ATTR
getSettings(void)
{
	return setting;
}
