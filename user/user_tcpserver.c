/*
 * user_tcpserver.c
 *
 *  Created on: 22.02.2015
 *      Author: Max
 */
#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <ip_addr.h>
#include <espconn.h>
#include <user_config.h>
#include <user_interface.h>
#include <gpio.h>
#include <spi_flash.h>
#include <upgrade.h>

#include "user_tcpserver.h"

uint8 out_state = 0;
uint8 old_state = 0;
uint8 sperre = 0;
LOCAL os_timer_t out_timer;

uint8 upgrade_lock = 0;
uint8 user_bin = 0;
LOCAL os_timer_t restart_timer;

LOCAL void ICACHE_FLASH_ATTR
restart_timer_handler(void)
{
	system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
	system_upgrade_reboot();
}

LOCAL void write_flash(uint32 address, char *data, uint16 length)
{
	ETS_UART_INTR_DISABLE();
	spi_flash_write(address, (uint32*) data, length);
	ETS_UART_INTR_ENABLE();
}

LOCAL void local_upgrade_download(void* arg, char *pusrdata, unsigned short length)
{
    char *ptr = NULL;
    char *ptmp2 = NULL;
    char lengthbuffer[32];
    static uint32_t totallength = 1;
    static uint32_t sumlength = 0;
    struct espconn *tcpconn = (struct espconn *)arg;

    if ( totallength == 1 )
    {
    	if ( (ptr = (char *)os_strstr(pusrdata, "Content-Length: ")) != NULL )
    	{
			if (ptr != NULL)
			{
				ptr += 16;
				ptmp2 = (char *)os_strstr(ptr, "\r\n");
				if (ptmp2 != NULL)
				{
					os_memset(lengthbuffer, 0, sizeof(lengthbuffer));
					os_memcpy(lengthbuffer, ptr, ptmp2 - ptr);
					sumlength = atoi(lengthbuffer);

					uint8 sec;
					if(user_bin == UPGRADE_FW_BIN1)
						for(sec = 65; sec <= 124; sec++) spi_flash_erase_sector(sec);
					else if(user_bin == UPGRADE_FW_BIN2)
						for(sec = 1; sec <= 59; sec++) spi_flash_erase_sector(sec);

					espconn_sent(tcpconn, "Sumlength ACK", sizeof("Sumlength ACK"));
					totallength = 0; // Lets start with flashing in the next round
				}
				else
				{
					espconn_sent(tcpconn, "No Content-Length sent!", sizeof("No Content-Length sent!"));
				}
			}
    	}
        else
        {
        	espconn_sent(tcpconn, "No Content-Length sent!", sizeof("No Content-Length sent!"));
        }
    }
    else
    {
		if(user_bin == UPGRADE_FW_BIN1) 	 write_flash(0x41000 + totallength, pusrdata, length);
		else if(user_bin == UPGRADE_FW_BIN2) write_flash(0x01000 + totallength, pusrdata, length);
		totallength += length;
        espconn_sent(tcpconn, "Upgrade ACK", sizeof("Upgrade ACK"));
    }

    if (totallength == sumlength)
    {
    	espconn_disconnect(tcpconn);
    	os_timer_disarm(&restart_timer);
    	os_timer_setfn(&restart_timer, restart_timer_handler, NULL);
    	os_timer_arm(&restart_timer, 2000, 0);
    }
}

LOCAL void ICACHE_FLASH_ATTR
out_timer_handler(void)
{
	os_timer_disarm(&out_timer);
	sperre = 0;
}

uint8_t ICACHE_FLASH_ATTR
change_out(uint8_t pState)
{
	if( (old_state != pState) && (!sperre) )
	{
		sperre = 1;

		if(pState) GPIO_OUTPUT_SET(OUT_GPIO, 1);
		else GPIO_OUTPUT_SET(OUT_GPIO, 0);

		os_timer_arm(&out_timer, 2000, 0);

		out_state = pState;
		old_state = pState;
		return 1;
	}
	else return 0;
}

LOCAL void ICACHE_FLASH_ATTR
tcpNetworkRecvCb(void *arg, char *data, unsigned short len)
{
    struct espconn *tcpconn = (struct espconn *)arg;

	if(upgrade_lock != 1)
	{
		if(strcmp(data, "Status?") == 0)
		{
			if(out_state)
			{
				uint8_t data[] = "IsOn!\n";
				espconn_sent(tcpconn, data, sizeof(data));
			}
			else {
				uint8_t data[] = "IsOff!\n";
				espconn_sent(tcpconn, data, sizeof(data));
			}
		}
		else if( strcmp(data, "Upgrade" ) == 0)
		{
			char buffer[28];
			upgrade_lock = 1;
			user_bin = system_upgrade_userbin_check();
			os_sprintf(buffer, "Version 1.1 bin: %u [1;2]", user_bin+1);
			espconn_sent(tcpconn, buffer, sizeof(buffer));
		}
		else
		{
			if(strcmp(data, "On") == 0)
			{
				change_out(1);
			}
			else if(strcmp(data, "Off") == 0)
			{
				change_out(0);
			}
			if(out_state)
			{
				uint8_t data[] = "IsOn!\n";
				espconn_sent(tcpconn, data, sizeof(data));
			}
			else {
				uint8_t data[] = "IsOff!\n";
				espconn_sent(tcpconn, data, sizeof(data));
			}
		}
		espconn_disconnect(tcpconn);
	}
	else
	{
		local_upgrade_download(tcpconn, data, len);
	}
}

LOCAL void ICACHE_FLASH_ATTR
tcpNetworkReconCb(void *arg, sint8 err)
{
    struct espconn *tcpconn = (struct espconn *)arg;
	#ifdef DEBUG
		ets_uart_printf("Reconnect Cb\n");
	#endif
}

LOCAL void ICACHE_FLASH_ATTR
tcpNetworkDisconCb(void *arg)
{
    struct espconn *tcpconn = (struct espconn *)arg;
	#ifdef DEBUG
		ets_uart_printf("Disconnect Cb\n");
	#endif
}

LOCAL void ICACHE_FLASH_ATTR
tcpNetworkConnectedCb(void *arg)
{
    struct espconn *tcpconn=(struct espconn *)arg;
	#ifdef DEBUG
		ets_uart_printf("Cennect Cb\n");
	#endif
	espconn_regist_disconcb(tcpconn, tcpNetworkDisconCb);
	espconn_regist_reconcb(tcpconn, tcpNetworkReconCb);
	espconn_regist_recvcb(tcpconn, tcpNetworkRecvCb);
}

void ICACHE_FLASH_ATTR
init_tcp_server(uint32_t pPort)
{
	static struct espconn esp_conn;
	static esp_tcp esptcp;

	os_timer_disarm(&out_timer);
	os_timer_setfn(&out_timer, out_timer_handler, NULL);

	esp_conn.type = ESPCONN_TCP;
	esp_conn.state = ESPCONN_NONE;
	esp_conn.proto.tcp = &esptcp;
	esp_conn.proto.tcp->local_port = pPort;

	espconn_regist_connectcb(&esp_conn, tcpNetworkConnectedCb);
	espconn_accept(&esp_conn);
}



