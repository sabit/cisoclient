/*
 * =====================================================================================
 *
 *       Filename:  cisoclient.c
 *
 *    Description:  A simple ISO8583 client using CPROPS
 *
 *        Version:  1.0
 *        Created:  07/26/2012 10:55:56 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *
 * =====================================================================================
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cprops/str.h>
#include <cprops/log.h>
#include <cprops/client.h>
#include <dl_iso8583.h>
#include <dl_iso8583_defs_1993.h>
#include <dl_output.h>
#include "ini.h"

int pack(DL_UINT8 *packBuf)
{
    DL_ISO8583_HANDLER isoHandler;
    DL_ISO8583_MSG     isoMsg;
    DL_UINT16          packedSize;

    /* get ISO-8583 1993 handler */
    DL_ISO8583_DEFS_1993_GetHandler(&isoHandler);

    //
    // Populate/Pack message
    //

    /* initialise ISO message */
    DL_ISO8583_MSG_Init(NULL,0,&isoMsg);

    /* set ISO message fields */
    (void)DL_ISO8583_MSG_SetField_Str(0,"0800",&isoMsg);
    (void)DL_ISO8583_MSG_SetField_Str(2,"1234567890123456",&isoMsg);
    (void)DL_ISO8583_MSG_SetField_Str(125,"BLAH BLAH",&isoMsg);

    /* output ISO message content */
    DL_ISO8583_MSG_Dump(stdout,NULL,&isoHandler,&isoMsg);

    /* pack ISO message */
    (void)DL_ISO8583_MSG_Pack(&isoHandler,&isoMsg,packBuf,&packedSize);

    /* free ISO message */
    DL_ISO8583_MSG_Free(&isoMsg);

    /* output packed message (in hex) */
    return packedSize;

}

typedef struct
{
    int port;
    const char* host;
} configuration;

static int handler(void* user, const char* section, const char* name,
        const char* value)
{
    configuration* pconfig = (configuration*)user;

#define MATCH(s, n) strcasecmp(section, s) == 0 && strcasecmp(name, n) == 0
    if (MATCH("host", "ip")) {
        pconfig->host = strdup(value);
    } else if (MATCH("host", "port")) {
        pconfig->port = atoi(value);
    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}



int main(int argc, char *argv[])
{
    configuration config;

    cp_client *client;
    char request[0x100];
    char *hostbuf, *host, *uri, *pbuf;
    int port;
    cp_string *buf;
    DL_UINT8 isoBuf[10000];
    DL_UINT16 iso_buf_size;
    char iso_buf_size_ascii[5];

    cp_log_init("test_client.log", LOG_LEVEL_DEBUG);

    if (ini_parse("cisoclient.ini", handler, &config) < 0) {
        printf("Can't load 'cisoclient.ini'\n");
        return 1;
    }

    host = strdup(config.host);
    port = config.port;
    cp_log("Host: %s Port: %d", host, port);

    client = cp_client_create(host, port);
    if (client == NULL)
    {
        printf("can\'t create client\n");
        exit(2);
    }

    if (cp_client_connect(client) == -1)
    {
        printf("connect failed\n");
        exit(3);
    }

    //sprintf(request, request_fmt, uri, host);
    iso_buf_size = pack(isoBuf+4);
    DL_OUTPUT_Hex(stdout,NULL,isoBuf,iso_buf_size+4);
    sprintf(iso_buf_size_ascii, "%04d", iso_buf_size);
    strncpy(isoBuf, iso_buf_size_ascii, 4);
    DL_OUTPUT_Hex(stdout,NULL,isoBuf,iso_buf_size+4);

    if (write(client->fd, isoBuf, iso_buf_size+4) == -1)
        perror("write");

    buf = cp_string_read(client->fd, 4);
    iso_buf_size=atoi(cp_string_data((buf)));
    cp_log("ISO Resp Size: %d\n", iso_buf_size );
    buf = cp_string_read(client->fd, iso_buf_size);
    cp_ndump(LOG_LEVEL_INFO, buf, iso_buf_size);
    cp_string_destroy(buf);

    cp_client_close(client);
    cp_client_destroy(client);

    cp_log_close();

    free(hostbuf);

    return 0;
}
