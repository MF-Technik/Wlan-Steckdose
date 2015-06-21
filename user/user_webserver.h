/*
 * user_webserver.h
 *
 *  Created on: 13.06.2015
 *      Author: Max
 */

#ifndef USER_USER_WEBSERVER_H_
#define USER_USER_WEBSERVER_H_

#define URLSize 10

typedef enum ProtocolType {
    GET = 0,
    POST,
} ProtocolType;

typedef struct URL_Frame {
    enum ProtocolType Type;
    char pSelect[URLSize];
    char pCommand[URLSize];
    char pFilename[URLSize];
} URL_Frame;

void user_webserver_init(void);

#endif /* USER_USER_WEBSERVER_H_ */
