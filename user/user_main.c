#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>
#include <ip_addr.h>
#include <espconn.h>
#include <user_config.h>
#include <user_interface.h>
#include <mem.h>

#include "driver/uart.h"
#include "user_tcpserver.h"
#include "user_settings.h"
#include "user_webserver.h"

/*
 * Set the SSID and the Password of your Wlan Accesspoint here.
 */
#define SSID 		"SSID"
#define PASSWORD 	"Password"

// Taster Interrupt Handler
LOCAL void ICACHE_FLASH_ATTR
taster_handler(void)
{
	static uint32 outp = 0;
	uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

	change_out(outp);
	outp = ~outp;

	// Clear interrupt status
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);
}

void wifi_handle_event_cb(System_Event_t *evt)
{
#ifdef DEBUG
	ets_uart_printf("event %x\n", evt->event);
#endif

	switch (evt->event)
	{
		case EVENT_STAMODE_CONNECTED:
#ifdef DEBUG
			os_printf("connect to ssid %s, channel %d\n",
							evt->event_info.connected.ssid,
							evt->event_info.connected.channel);
#endif
			break;
		case EVENT_STAMODE_DISCONNECTED:
#ifdef DEBUG
			os_printf("disconnect from ssid %s, reason %d\n",
					evt->event_info.disconnected.ssid,
					evt->event_info.disconnected.reason);
#endif
			break;
		case EVENT_STAMODE_AUTHMODE_CHANGE:
#ifdef DEBUG
			os_printf("mode: %d -> %d\n", evt->event_info.auth_change.old_mode,
					evt->event_info.auth_change.new_mode);
#endif
			break;
		case EVENT_STAMODE_GOT_IP:
#ifdef DEBUG
			os_printf("WiFi connected\r\n");
			os_printf("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR,
						IP2STR(&evt->event_info.got_ip.ip),
						IP2STR(&evt->event_info.got_ip.mask),
						IP2STR(&evt->event_info.got_ip.gw));
			os_printf("\n");
			os_printf("Start TCP connecting...\r\n");
#endif
			GPIO_OUTPUT_SET(LED_GPIO, 0);
			// Taster Interrupt setzen
			ETS_GPIO_INTR_ATTACH(taster_handler, TASTER_GPIO);
			gpio_pin_intr_state_set(GPIO_ID_PIN(TASTER_GPIO), GPIO_PIN_INTR_POSEDGE);
			ETS_GPIO_INTR_ENABLE();
			init_tcp_server(USER_PORT);
			break;
		case EVENT_SOFTAPMODE_STACONNECTED:
			GPIO_OUTPUT_SET(LED_GPIO, 1);
#ifdef DEBUG
			os_printf("station: " MACSTR "join, AID = %d\n",
					MAC2STR(evt->event_info.sta_connected.mac),
					evt->event_info.sta_connected.aid);
#endif
			break;
		case EVENT_SOFTAPMODE_STADISCONNECTED:
			GPIO_OUTPUT_SET(LED_GPIO, 0);
#ifdef DEBUG
			os_printf("station: " MACSTR "leave, AID = %d\n",
					MAC2STR(evt->event_info.sta_disconnected.mac),
					evt->event_info.sta_disconnected.aid);
#endif
			break;
		default:
			break;
	}
}

void ICACHE_FLASH_ATTR user_init(void)
{
	#ifdef DEBUG
		system_set_os_print(1);
		uart_init(BIT_RATE_115200, BIT_RATE_115200);
		os_delay_us(1000);
		ets_uart_printf("SDK Version: %s\n", system_get_sdk_version());
	#else
		system_set_os_print(0);
	#endif

	// Relais
    PIN_FUNC_SELECT(OUT_PERIPHS_MUX, OUT_GPIO_FUNC);
    PIN_PULLDWN_DIS(OUT_PERIPHS_MUX);
    PIN_PULLUP_DIS(OUT_PERIPHS_MUX);
    GPIO_OUTPUT_SET(OUT_GPIO, 0);
    // LED
    PIN_FUNC_SELECT(LED_PERIPHS_MUX, LED_GPIO_FUNC);
    PIN_PULLDWN_DIS(LED_PERIPHS_MUX);
    PIN_PULLUP_DIS(LED_PERIPHS_MUX);
    // Taster Pull-Down
    PIN_FUNC_SELECT(TASTER_PERIPHS_MUX, TASTER_GPIO_FUNC);
    GPIO_DIS_OUTPUT(TASTER_GPIO);
    PIN_PULLDWN_EN(TASTER_PERIPHS_MUX);
    PIN_PULLUP_DIS(TASTER_PERIPHS_MUX);

    wifi_station_set_auto_connect(0);

    initSettings();
    if( GPIO_INPUT_GET(TASTER_GPIO) )
	{
    	// Button pressed => Configure Soft AP
		struct softap_config softapConf;
		char softap_password[64] = "Password";
		char softap_ssid[32] = "Wlan Steckdose";
    	struct ip_info info;
    	struct dhcps_lease dhcp_range;

		wifi_softap_dhcps_stop();
		IP4_ADDR(&info.ip, 192, 168, 0, 1);
		IP4_ADDR(&info.gw, 10, 10, 10, 1);
		IP4_ADDR(&info.netmask, 255, 255, 255, 0);
		wifi_set_ip_info(SOFTAP_IF, &info);

		IP4_ADDR(&dhcp_range.start_ip, 192, 168, 10, 0);
		IP4_ADDR(&dhcp_range.end_ip, 192, 168, 10, 50);
		wifi_softap_set_dhcps_lease(&dhcp_range);

		os_memset(softapConf.ssid, 0, sizeof(softapConf.ssid));
		os_sprintf(softapConf.ssid, softap_ssid, os_strlen(softap_ssid));

		os_memset(softapConf.password, 0, sizeof(softapConf.password));
		os_memcpy(&softapConf.password, softap_password, os_strlen(softap_password));

		softapConf.ssid_len = os_strlen(softap_ssid);
		softapConf.channel = 7;
		softapConf.authmode = AUTH_WPA2_PSK;
		softapConf.ssid_hidden = 0;
		softapConf.max_connection = 4;
		softapConf.beacon_interval = 200;

    	wifi_set_opmode(SOFTAP_MODE);					// Set station mode
		wifi_softap_set_config(&softapConf);
		wifi_softap_dhcps_start();

		user_webserver_init();
	}
    else
    {
    	// Connect to Wifi AP
    	char ssid[32] = SSID;							// Wifi SSID
		char password[64] = PASSWORD;					// Wifi Password
		struct station_config stationConf;				// Station conf struct

		// Uncomment for static IP
		/*
		struct ip_info info;
		wifi_station_dhcpc_stop();
		IP4_ADDR(&info.ip, 192, 168, 0, 0);
		IP4_ADDR(&info.gw, 192, 168, 0, 0);
		IP4_ADDR(&info.netmask, 255, 255, 255, 0);
		wifi_set_ip_info(STATION_IF, &info);
		*/

		GPIO_OUTPUT_SET(LED_GPIO, 1);					// Indicate Wifi Connection

		os_memcpy(&stationConf.ssid, ssid, 32);			// Set settings
		os_memcpy(&stationConf.password, password, 64);	// Set settings

		wifi_set_opmode(STATION_MODE);					// Set station mode
		wifi_station_set_reconnect_policy(true);
		wifi_station_set_config(&stationConf);			// Set wifi config
		wifi_station_set_auto_connect(1);
    }
    wifi_set_event_handler_cb(wifi_handle_event_cb);
}
