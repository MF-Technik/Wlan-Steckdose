/*
 * user_webserver.c
 *
 *  Created on: 13.06.2015
 *      Author: Max
 */

#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>
#include <ip_addr.h>
#include <espconn.h>
#include <user_config.h>
#include <user_interface.h>
#include <mem.h>

#include "user_webserver.h"

LOCAL char *precvbuffer;
static uint32 dat_sumlength = 0;

static char index_html[] = "<html><head><title>Wlan Steckdose</title></head><body><h1>Wlan Steckdose</h1><h3>Einstellungen</h3></body></html>";

/******************************************************************************
 * FunctionName : parse_url
 * Description  : parse the received data from the server
 * Parameters   : precv -- the received data
 *                purl_frame -- the result of parsing the url
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
parse_url(char *precv, URL_Frame *purl_frame)
{
    char *str = NULL;
    uint8 length = 0;
    char *pbuffer = NULL;
    char *pbufer = NULL;

    if (purl_frame == NULL || precv == NULL) return;

    pbuffer = (char *)os_strstr(precv, "Host:");

    if (pbuffer != NULL) {
        length = pbuffer - precv;
        pbufer = (char *)os_zalloc(length + 1);
        pbuffer = pbufer;
        os_memcpy(pbuffer, precv, length);
        os_memset(purl_frame->pSelect, 0, URLSize);
        os_memset(purl_frame->pCommand, 0, URLSize);
        os_memset(purl_frame->pFilename, 0, URLSize);

        if (os_strncmp(pbuffer, "GET ", 4) == 0)
        {
            purl_frame->Type = GET;
            pbuffer += 4;
        }
        else if (os_strncmp(pbuffer, "POST ", 5) == 0)
        {
            purl_frame->Type = POST;
            pbuffer += 5;
        }

        pbuffer ++;
        str = (char *)os_strstr(pbuffer, "?");

        if (str != NULL)
        {
            length = str - pbuffer;
            os_memcpy(purl_frame->pSelect, pbuffer, length);
            str ++;
            pbuffer = (char *)os_strstr(str, "=");
            if (pbuffer != NULL)
            {
                length = pbuffer - str;
                os_memcpy(purl_frame->pCommand, str, length);
                pbuffer ++;
                str = (char *)os_strstr(pbuffer, "&");
                if (str != NULL)
                {
                    length = str - pbuffer;
                    os_memcpy(purl_frame->pFilename, pbuffer, length);
                }
                else
                {
                    str = (char *)os_strstr(pbuffer, " HTTP");
                    if (str != NULL)
                    {
                        length = str - pbuffer;
                        os_memcpy(purl_frame->pFilename, pbuffer, length);
                    }
                }
            }
        }
        os_free(pbufer);
    } else return;
}

LOCAL bool save_data(char *precv, uint16 length)
{
    bool flag = false;
    char length_buf[10] = {0};
    char *ptemp = NULL;
    char *pdata = NULL;
    uint16 headlength = 0;
    static uint32 totallength = 0;

    ptemp = (char *)os_strstr(precv, "\r\n\r\n");

    if (ptemp != NULL) {
        length -= ptemp - precv;
        length -= 4;
        totallength += length;
        headlength = ptemp - precv + 4;
        pdata = (char *)os_strstr(precv, "Content-Length: ");

        if (pdata != NULL) {
            pdata += 16;
            precvbuffer = (char *)os_strstr(pdata, "\r\n");

            if (precvbuffer != NULL) {
                os_memcpy(length_buf, pdata, precvbuffer - pdata);
                dat_sumlength = atoi(length_buf);
            }
        } else {
        	if (totallength != 0x00){
        		totallength = 0;
        		dat_sumlength = 0;
        		return false;
        	}
        }
        if ((dat_sumlength + headlength) >= 1024) {
        	precvbuffer = (char *)os_zalloc(headlength + 1);
            os_memcpy(precvbuffer, precv, headlength + 1);
        } else {
        	precvbuffer = (char *)os_zalloc(dat_sumlength + headlength + 1);
        	os_memcpy(precvbuffer, precv, os_strlen(precv));
        }
    } else {
        if (precvbuffer != NULL) {
            totallength += length;
            os_memcpy(precvbuffer + os_strlen(precvbuffer), precv, length);
        } else {
            totallength = 0;
            dat_sumlength = 0;
            return false;
        }
    }

    if (totallength == dat_sumlength) {
        totallength = 0;
        dat_sumlength = 0;
        return true;
    } else {
        return false;
    }
}

/******************************************************************************
 * FunctionName : data_send
 * Description  : processing the data as http format and send to the client or server
 * Parameters   : arg -- argument to set for client or server
 *                responseOK -- true or false
 *                psend -- The send data
 * Returns      :
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
data_send(void *arg, bool responseOK, char *psend)
{
    uint16 length = 0;
    char *pbuf = NULL;
    char httphead[256];
    struct espconn *ptrespconn = arg;
    os_memset(httphead, 0, 256);

    if (responseOK)
    {
		os_sprintf(httphead,
				"HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\n",
				psend ? os_strlen(psend) : 0);
        if (psend)
        {
			os_sprintf(httphead + os_strlen(httphead),
					"Content-type: application/json\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n");
            length = os_strlen(httphead) + os_strlen(psend);
            pbuf = (char *)os_zalloc(length + 1);
            os_memcpy(pbuf, httphead, os_strlen(httphead));
            os_memcpy(pbuf + os_strlen(httphead), psend, os_strlen(psend));
        }
        else
        {
            os_sprintf(httphead + os_strlen(httphead), "\n");
            length = os_strlen(httphead);
        }
    }
    else
    {
		os_sprintf(httphead, "HTTP/1.0 400 BadRequest\r\n\
Content-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
        length = os_strlen(httphead);
    }

    if (psend)  espconn_sent(ptrespconn, pbuf, length);
    else 		espconn_sent(ptrespconn, httphead, length);

    if (pbuf)
    {
        os_free(pbuf);
        pbuf = NULL;
    }
}

/******************************************************************************
 * FunctionName : response_send
 * Description  : processing the send result
 * Parameters   : arg -- argument to set for client or server
 *                responseOK --  true or false
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
response_send(void *arg, bool responseOK)
{
    struct espconn *ptrespconn = arg;

    data_send(ptrespconn, responseOK, NULL);
}

/******************************************************************************
 * FunctionName : webserver_recv
 * Description  : Processing the received data from the server
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pusrdata -- The received data (or NULL when the connection has been closed!)
 *                length -- The length of received data
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
webserver_recv(void *arg, char *pusrdata, unsigned short length)
{
    URL_Frame *pURL_Frame = NULL;
    char *pParseBuffer = NULL;
    bool parse_flag = false;
    struct espconn *ptrespconn = arg;

	parse_flag = save_data(pusrdata, length);
	if (parse_flag == false){ response_send(ptrespconn, false);}

	pURL_Frame = (URL_Frame *)os_zalloc(sizeof(URL_Frame));
	parse_url(precvbuffer, pURL_Frame);

	switch (pURL_Frame->Type) {
		case GET:
#ifdef DEBUG
			os_printf("We have a GET request.\n");
#endif

			data_send(ptrespconn, true, index_html);
/*
			if (os_strcmp(pURL_Frame->pSelect, "client") == 0 && os_strcmp(pURL_Frame->pCommand, "command") == 0)
			{
				if (os_strcmp(pURL_Frame->pFilename, "info") == 0) {

				}

				if (os_strcmp(pURL_Frame->pFilename, "status") == 0)
				{

				}
				else
				{
					response_send(ptrespconn, false);
				}
			}
			else if (os_strcmp(pURL_Frame->pSelect, "config") == 0 && os_strcmp(pURL_Frame->pCommand, "command") == 0)
			{
				if (os_strcmp(pURL_Frame->pFilename, "wifi") == 0)
				{

				}
				else if (os_strcmp(pURL_Frame->pFilename, "switch") == 0)
				{

				}
				else if (os_strcmp(pURL_Frame->pFilename, "reboot") == 0)
				{

				}
				else
				{
					response_send(ptrespconn, false);
				}
			}
			else if (os_strcmp(pURL_Frame->pSelect, "upgrade") == 0 && os_strcmp(pURL_Frame->pCommand, "command") == 0)
			{
				if (os_strcmp(pURL_Frame->pFilename, "getuser") == 0)
				{

				}
			}
			else
			{
				response_send(ptrespconn, false);
			}
*/
			break;
		case POST:
#ifdef DEBUG
			os_printf("We have a POST request.\n");
#endif

			pParseBuffer = (char *)os_strstr(precvbuffer, "\r\n\r\n");

			if (pParseBuffer == NULL) {
				break;
			}

			pParseBuffer += 4;
/*
			if (os_strcmp(pURL_Frame->pSelect, "config") == 0 && os_strcmp(pURL_Frame->pCommand, "command") == 0)
			{
				if (os_strcmp(pURL_Frame->pFilename, "switch") == 0)
				{
					if (pParseBuffer != NULL)
					{
						response_send(ptrespconn, true);
					}
					else
					{
						response_send(ptrespconn, false);
					}
				}
				else
				{
					response_send(ptrespconn, false);
				}
			}
			else
			{
				response_send(ptrespconn, false);
			}
*/
			break;
	}

	if (precvbuffer != NULL)
	{
		os_free(precvbuffer);
		precvbuffer = NULL;
	}
	os_free(pURL_Frame);
	pURL_Frame = NULL;
}

/******************************************************************************
 * FunctionName : webserver_recon
 * Description  : the connection has been err, reconnection
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL ICACHE_FLASH_ATTR
void webserver_recon(void *arg, sint8 err)
{
    struct espconn *pesp_conn = arg;
#ifdef DEBUG
    os_printf("webserver's %d.%d.%d.%d:%d err %d reconnect\n", pesp_conn->proto.tcp->remote_ip[0],
    		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
    		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port, err);
#endif
}

/******************************************************************************
 * FunctionName : webserver_recon
 * Description  : the connection has been err, reconnection
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL ICACHE_FLASH_ATTR
void webserver_discon(void *arg)
{
    struct espconn *pesp_conn = arg;
#ifdef DEBUG
    os_printf("webserver's %d.%d.%d.%d:%d disconnect\n", pesp_conn->proto.tcp->remote_ip[0],
        		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
        		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port);
#endif
}

/******************************************************************************
 * FunctionName : user_accept_listen
 * Description  : server listened a connection successfully
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
webserver_listen(void *arg)
{
    struct espconn *pesp_conn = arg;

    espconn_regist_recvcb(pesp_conn, webserver_recv);
    espconn_regist_reconcb(pesp_conn, webserver_recon);
    espconn_regist_disconcb(pesp_conn, webserver_discon);
}

/******************************************************************************
 * FunctionName : user_webserver_init
 * Description  : parameter initialize as a server
 * Parameters   : port -- server port
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_webserver_init(void)
{
    static struct espconn esp_conn;
    static esp_tcp esptcp;

    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    esp_conn.proto.tcp->local_port = 80;

    espconn_regist_connectcb(&esp_conn, webserver_listen);
    espconn_accept(&esp_conn);
}


