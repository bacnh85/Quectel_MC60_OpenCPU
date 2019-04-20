#ifndef __FOTA_HTTP_H__
#define __FOTA_HTTP_H__

#include "ql_type.h"
#include "ql_stdlib.h"
#include "fota_http_code.h"

/******************************************************************************
* Macros
******************************************************************************/
#define QUECTEL_HTTP_URL_LENGTH 200
#define QUECTEL_HTTP_DOMAINNAME_LENGTH 150
#define QUECTEL_HTTP_DATABUFFER_SIZE 512
#define HTTP_SERVICE_PORT 80
#define HTTP_GETREADTIMEROUT  120000 // 2 mins for read socket timerout
#define HTTP_GETREADTIMERID  0x2F


typedef enum HttpResultTag
{
    HTTP_RESULT_ERROR_WOULDBLOCK=-2,
    
    HTTP_RESULT_OK=0,
    HTTP_RESULT_ERROR_SOC_CLOSE,
    HTTP_RESULT_ERROR_HTTP_RELOCATION,
    HTTP_RESULT_ERROR_HTTP_RESPONSE_FAILED,
    HTTP_RESULT_ERROR_DECODEERROR,
    END_OF_HTTP_RESULT
}HttpResult_e;


typedef struct HttpMainContextTag
{
    u8      contextId;  /*relation for tcpip*/
    u8      sourceid; /*relation for tcpip*/
    u8      socketid;/*relation for tcpip*/

    bool    bpeersocketclose;

    u32     httpGettimeout;
    u32     httpGetHeadTimer;
    bool    httpGettimerstate;

    u32     downloadsize;
    u32     downloadtimeout;
    u32     httpDownloadTimer;

    u8       Address[QUECTEL_HTTP_URL_LENGTH+1];
    u32     AddressValidLegth;

    u32     ContentLength;

    bool    getbody;
    u32     receivedbodydata;
    
    u8   	hostip[30];
    u16      hostport;
    u8        hostname[QUECTEL_HTTP_DOMAINNAME_LENGTH+1];
    u8        hostipAddres[5];

    u8        *http_socketdata_p; //   porint to the http socket received buffer 
    u8        *genhttp_dstconstptr;
    u16      genhttp_dstconstsize;
    u16      genhttp_dstpos;
    s32      genhttp_dstvaliddatalen;

    HttpHeader_t httpheader;

    u8        httpversion;
}HttpMainContext_t;



extern HttpMainContext_t  httpMainContext; 

void http_TimerCallback(u32 timerId, void* param);
void Callback_GetIpByHostName(u8 contexId, u8 requestId, s32 errCode,  u32 ipAddrCnt, u32* ipAddr);


/*****************************************************************
* GPRS and Socket callback function
******************************************************************/
void HttpCallback_GPRS_Actived(u8 contexId, s32 errCode, void* customParam);
void HttpCallBack_GPRS_Deactived(u8 contextId, s32 errCode, void* customParam );

/*****************************************************************
*socket callback function
******************************************************************/
void Httpcallback_socket_connect(s32 socketId, s32 errCode, void* customParam);
void Httpcallback_socket_close(s32 socketId, s32 errCode, void* customParam);
void Httpcallback_socket_accept(s32 listenSocketId, s32 errCode, void* customParam);
void Httpcallback_socket_read(s32 socketId, s32 errCode, void* customParam);
void Httpcallback_socket_write(s32 socketId, s32 errCode, void* customParam);

bool HTTP_IsHttpServer(u8* URL);

s32 HTTP_FotaMain(u8 contextId, u8* URL);

void http_initialize(void);

void http_ConnectToServer(void);

s32 http_SendHttpGetHead( s32 socketId);

s8 http_RecvHttpHead(s32 socketid, bool bContinue);

s8 http_RecvHttpBody(s32 socketid, bool bContinue);

s8 http_RecvHttpChunkedBody(s32 socketid, bool bContinue);

bool http_DecodeURL(u8 *Address, u32 *AddressValidLegth, u8 *hostip, u16 hostiplength, 
                            u8 *hostname, u16 hostnamelength, u16 *hostport);


#endif 
