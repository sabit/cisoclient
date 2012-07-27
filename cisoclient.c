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

#define ISOHEADER 4
#define ISOHEADERSTR "%04d"

int pack(char *packBuf)
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
    (void)DL_ISO8583_MSG_SetField_Str(0,"1800",&isoMsg);
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

int unpack(char *packBuf, unsigned short pack_len)
{
    DL_ISO8583_HANDLER isoHandler;
    DL_ISO8583_MSG     isoMsg;
    /* get ISO-8583 1993 handler */
    DL_ISO8583_DEFS_1993_GetHandler(&isoHandler);
 /* initialise ISO message */
    DL_ISO8583_MSG_Init(NULL,0,&isoMsg);

    /* unpack ISO message */
    (void)DL_ISO8583_MSG_Unpack(&isoHandler,packBuf,pack_len,&isoMsg);

    /* output ISO message content */
    DL_ISO8583_MSG_Dump(stdout,NULL,&isoHandler,&isoMsg);

    /* free ISO message */
    DL_ISO8583_MSG_Free(&isoMsg);
    return 0;
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
    char iso_req_buf[10000];
    char iso_resp_buf[10000];
    unsigned short iso_req_size;
    unsigned short iso_resp_size;
    int raw_resp_size;
    char iso_header[ISOHEADER+1];

    cp_log_init("test_client.log", LOG_LEVEL_DEBUG);

    if (ini_parse("cisoclient.ini", handler, &config) < 0) {
        printf("Can't load 'cisoclient.ini'\n");
        return 1;
    }

    host = strdup(config.host);
    port = config.port;
    cp_log("Host: %s Port: %d", host, port);
    cp_client_init();
    client = cp_client_create(host, port);
    cp_client_set_timeout(client, 1, 1);
    cp_client_set_retry(client, 1);
    cp_log("Connecting\n");

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

    //Pack ISO msg
    iso_req_size = pack(iso_req_buf+ISOHEADER);
    sprintf(iso_header, ISOHEADERSTR, iso_req_size);
    strncpy(iso_req_buf, iso_header, ISOHEADER);

    //Send ISO Request
    if (write(client->fd, iso_req_buf, iso_req_size+ISOHEADER) == -1)
        perror("write");

    //response processing
    if(cp_client_read(client, iso_resp_buf, ISOHEADER)!=ISOHEADER)
    {
        cp_warn("No response from server");
        return 1;
    }
    iso_resp_buf[ISOHEADER] = '\0';
    iso_resp_size=atoi(iso_resp_buf);
    if(cp_client_read(client, iso_resp_buf+ISOHEADER, iso_resp_size) != iso_resp_size)
    {
        cp_warn("ISO msg + header != packet length");
        return 1;
    }

    DL_OUTPUT_Hex(stdout,NULL,iso_resp_buf,iso_resp_size+ISOHEADER);
    unpack(iso_resp_buf+ISOHEADER, iso_resp_size);
    cp_client_close(client);
    cp_client_destroy(client);

    cp_log_close();

    free(hostbuf);

    return 0;
}
