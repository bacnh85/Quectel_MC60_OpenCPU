#include "custom_feature_def.h"
#include "ql_stdlib.h"
#include "ql_common.h"
#include "ql_type.h"
#include "ql_socket.h"
#include "ql_timer.h"
#include "ql_fota.h"
#include "ql_gprs.h"
#include "fota_http.h"
#include "fota_http_code.h"
#include "fota_main.h"

#ifdef __OCPU_FOTA_BY_HTTP__  

extern ST_GprsConfig  Fota_gprsCfg;
extern u8 Fota_apn[10];
extern u8 Fota_userid[10];
extern u8 Fota_passwd[10];

#if UPGRADE_APP_DEBUG_ENABLE > 0
extern char FOTA_DBGBuffer[DBG_BUF_LEN];
#endif

extern Callback_Upgrade_State Fota_UpgardeState;

/*****************************************************************
* Global Variables
******************************************************************/
ST_PDPContxt_Callback  Httpcallback_gprs_func = 
{
    //HttpCallback_GPRS_Actived,
    NULL,
    HttpCallBack_GPRS_Deactived
};

ST_SOC_Callback Httpcallback_SOC_func=
{
    Httpcallback_socket_connect,
    Httpcallback_socket_close,
    Httpcallback_socket_accept,
    Httpcallback_socket_read,    
    Httpcallback_socket_write
};

HttpMainContext_t  httpMainContext;
void http_GetImagefilefromServer();

char* HttpGetHead[] =
{
    "GET %s HTTP/1.1\r\n",  // "%s"  URL address; 
    "Host: %s:%d\r\n",                  // "%s"   host name or ip    %s port;   port is not need
    "Accept: */*\r\n",
    "User-Agent: QUECTEL_MODULE\r\n", 
    "Connection: Keep-Alive\r\n",
    "\r\n",
};


bool HTTP_IsHttpServer(u8* URL)
{
    char buffer[5];
    Ql_memcpy(buffer, URL, sizeof(buffer));
    Ql_StrToUpper(buffer);
    if(NULL == Ql_strstr(buffer, "HTTP"))
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

s32 HTTP_FotaMain(u8 contextId, u8* URL)
{
    s32 ret;
    bool retValue;
    bool httpDecodeURL;
    u8* pUserName = NULL;
    HttpMainContext_t *httpContext_p =&httpMainContext;

    UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Fota http Main entry !-->\r\n");
    FOTA_DBG_PRINT("Enter into HTTP_FotaMain\r\n");
        
    /*****************************************************************
        Register the GPRS callback function first.
    ******************************************************************/
    ret = Ql_GPRS_Register(contextId, &Httpcallback_gprs_func, NULL);
    
     /*****************************************************************
       config the GPRS , set the APN.
    ******************************************************************/
    Ql_memset(&Fota_gprsCfg,0x00, sizeof(Fota_gprsCfg));
    Ql_strcpy(Fota_gprsCfg.apnName, Fota_apn);
    Ql_strcpy(Fota_gprsCfg.apnUserId, Fota_userid);
    Ql_strcpy(Fota_gprsCfg.apnPasswd, Fota_passwd);
    Fota_gprsCfg.authtype = 0;   
    ret = Ql_GPRS_Config(contextId, &Fota_gprsCfg);     
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Ql_GPRS_Config (RET =%d) -->\r\n", ret);
    FOTA_DBG_PRINT("<-- Config GPRS -->\r\n");
    ret = Ql_GPRS_ActivateEx(contextId, TRUE);
    if(GPRS_PDP_SUCCESS != ret && GPRS_PDP_ALREADY != ret)
    {

        FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--GPRS ACTIVATE FAILD return(err =%d)-->\r\n", ret);
        FOTA_DBG_PRINT("<-- Fail to activate GPRS -->\r\n");
        return -1;
    }
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--GPRS ACTIVATE successfully(%d) -->\r\n", ret);
    FOTA_DBG_PRINT("<-- Successfully activate GPRS -->\r\n");
    
    http_initialize();
    httpContext_p->socketid = Ql_SOC_CreateEx(contextId,SOC_TYPE_TCP, Ql_OS_GetActiveTaskId(), Httpcallback_SOC_func);
    if(httpContext_p->socketid <0)
    {
        FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Create socket  FAILD return(err =%d) -->\r\n", httpContext_p->socketid);
        FOTA_DBG_PRINT("<-- Fail to create socket -->\r\n");
        return -1;
    }
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Create socket successfully(%d) -->\r\n", ret);

    pUserName = Ql_strstr(URL, "@");
    if(NULL == pUserName)// http URL :   http://23.11.67.89/file/xxx.bin@userName:passowrd
    {
        // no userName and password
        httpContext_p->AddressValidLegth = Ql_strlen(URL);
        Ql_memcpy(httpContext_p->Address, URL,httpContext_p->AddressValidLegth);
    }
    else
    {
        httpContext_p->AddressValidLegth = pUserName - URL;
        Ql_memcpy(httpContext_p->Address, URL,httpContext_p->AddressValidLegth);      
    }
        
    httpDecodeURL = http_DecodeURL(httpContext_p->Address,&httpContext_p->AddressValidLegth, httpContext_p->hostip, sizeof(httpContext_p->hostip), 
                                httpContext_p->hostname, sizeof(httpContext_p->hostname), &httpContext_p->hostport);
    if(!httpDecodeURL)
    {
        FOTA_UPGRADE_IND(UP_URLDECODEFAIL,0,retValue);
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--HTTP DECODE URL FAILED !!!-->\r\n");
        FOTA_DBG_PRINT("<-- HTTP DECODE URL FAILED !!! -->\r\n");
    }

    if(httpContext_p->hostip[0] != 0) // host name is ip string
    {
        u32 hostipAddr = 0;
        ret = Ql_IpHelper_ConvertIpAddr(httpContext_p->hostip,&hostipAddr);
        if(SOC_SUCCESS == ret)
        {
            Ql_memcpy(httpContext_p->hostipAddres, &hostipAddr, sizeof(hostipAddr));
            FOTA_UPGRADE_IND(UP_CONNECTING,0,retValue);
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Convert Ip Address successfully,m_ipaddress=%d,%d,%d,%d-->\r\n",httpContext_p->hostipAddres[0],httpContext_p->hostipAddres[1],httpContext_p->hostipAddres[2],httpContext_p->hostipAddres[3]);
            FOTA_DBG_PRINT("<--Convert Ip Address successfully,m_ipaddress=");
            FOTA_DBG_PRINT(httpContext_p->hostipAddres);
            FOTA_DBG_PRINT(" -->\r\n");
            http_GetImagefilefromServer();         
        }
        else
        {
            FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Ql_IpHelper_ConvertIpAddr FAILD(err =%d)--> \r\n", ret);
            FOTA_DBG_PRINT("<--Ql_IpHelper_ConvertIpAddr FAILD --> \r\n");
            return -1;
        }
    }
    else  // host name is domain name
    {
         ret = Ql_IpHelper_GetIPByHostName(contextId, 0,httpContext_p->hostname,Callback_GetIpByHostName);
         if(SOC_WOULDBLOCK != ret && SOC_SUCCESS != ret)
         {
            FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Ql_IpHelper_GetIPByHostName failed (err =%d)--> \r\n", ret);
         }
         
         //to do http_GetImagefilefromServer function in the Callback_GetIpByHostName callback function                
    }

    return 0;
}
    
void http_initialize(void)
{
    HttpMainContext_t *httpContext_p  = &httpMainContext;

    httpContext_p->downloadtimeout = 0;
    httpContext_p->httpDownloadTimer = 0;

    Ql_memset(httpContext_p->hostip,0,sizeof(httpContext_p->hostip));
    Ql_memset(httpContext_p->hostname,0,sizeof(httpContext_p->hostname));
    Ql_memset(httpContext_p->Address,0,sizeof(httpContext_p->Address));
    Ql_memset(httpContext_p->hostipAddres,0,sizeof(httpContext_p->hostipAddres));
    httpContext_p->AddressValidLegth = 0;
    httpContext_p->hostport = HTTP_SERVICE_PORT;
    httpContext_p->contextId = -1;
    httpContext_p->socketid = -1;

    httpContext_p->httpGettimeout = HTTP_GETREADTIMEROUT;
    httpContext_p->httpGetHeadTimer = HTTP_GETREADTIMERID;
    httpContext_p->httpGettimerstate = FALSE;
    
    httpContext_p->http_socketdata_p = NULL;
    httpContext_p->genhttp_dstconstsize = QUECTEL_HTTP_DATABUFFER_SIZE;
    httpContext_p->genhttp_dstpos = 0;
    httpContext_p->genhttp_dstvaliddatalen = 0;
        
    httpContext_p->ContentLength = 0;
    httpContext_p->getbody = FALSE;
    httpContext_p->receivedbodydata = 0;
    
    httpContext_p->httpversion = 1;
    
}

void http_GetImagefilefromServer()
{
    s32 ret;
    bool retValue;
    HttpMainContext_t *httpContext_p  = &httpMainContext;
    
    ret = Ql_SOC_ConnectEx(httpContext_p->socketid, (u32)(httpContext_p->hostipAddres), httpContext_p->hostport, TRUE);
    if(SOC_SUCCESS != ret)
    {
        FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Ql_SOC_ConnectEx  failed (err =%d)-->\r\n", ret);
        FOTA_DBG_PRINT("<--Ql_SOC_ConnectEx  failed -->\r\n");
        return; 
    }

    FOTA_UPGRADE_IND(UP_CONNECTED,0,retValue);
    httpContext_p->bpeersocketclose = FALSE;
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--the module connect to the server successfully !-->\r\n");
    FOTA_DBG_PRINT("<--the module connect to the server successfully !-->\r\n");

    ret = http_SendHttpGetHead( httpContext_p->socketid);
    if(TRUE != ret) // send http head failed ,close the socket and stop upgrade.
    {
        Ql_SOC_Close(httpContext_p->socketid);
        httpContext_p->socketid = -1;
        httpContext_p->bpeersocketclose = TRUE;
        if(NULL != httpContext_p->http_socketdata_p)
        {
            Ql_MEM_Free((void *)(httpContext_p->http_socketdata_p));
            httpContext_p->http_socketdata_p = NULL;
        }
        FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--http_SendHttpGetHead  failed (err =%d)-->\r\n", ret);
        FOTA_DBG_PRINT("<--http_SendHttpGetHead  failed -->\r\n");
        return;
    }
    
    FOTA_UPGRADE_IND(UP_GETTING_FILE,0,retValue);
    Ql_Timer_Register(httpContext_p->httpGetHeadTimer, http_TimerCallback, NULL);
    Ql_Timer_Start(httpContext_p->httpGetHeadTimer, httpContext_p->httpGettimeout, FALSE);
    httpContext_p->httpGettimerstate = TRUE;
    
    return;
        
}


s32 http_SendHttpGetHead( s32 socketId)
{
    s32 ret;
    s32 retValue;
    char sendBuffer[200];
    char Address[200];
    HttpMainContext_t *httpContext_p = &httpMainContext;

    // http  head:  (GET........  )
    Ql_memset(sendBuffer, 0x00, sizeof(sendBuffer));
    Ql_memset(Address, 0x00, sizeof(Address));
    Ql_strncpy(Address, httpContext_p->Address, httpContext_p->AddressValidLegth);
    Ql_sprintf(sendBuffer,HttpGetHead[0],Address);
    ret = Ql_SOC_Send(socketId, sendBuffer, Ql_strlen(sendBuffer));
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--send GET:<%s> \r\n",sendBuffer);
    FOTA_DBG_PRINT("<--send GET: \r\n");
    FOTA_DBG_PRINT(sendBuffer);
    if(ret != Ql_strlen(sendBuffer))
    {return -1;}

    // http  head:  (Host: ........ )
    Ql_memset(sendBuffer, 0x00, sizeof(sendBuffer));
    if(httpContext_p->hostip[0] != 0)
    {
        Ql_sprintf(sendBuffer,HttpGetHead[1],httpContext_p->hostip, httpContext_p->hostport);
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--httpContext_p->hostip<%s> -->\r\n",httpContext_p->hostip);
        FOTA_DBG_PRINT("<--httpContext_p->hostip: ");
        FOTA_DBG_PRINT(httpContext_p->hostip);
    }
    else
    {
        Ql_sprintf(sendBuffer,HttpGetHead[1],httpContext_p->hostname,httpContext_p->hostport);
    }
    ret =  Ql_SOC_Send(socketId, sendBuffer, Ql_strlen(sendBuffer));
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--send Host:<%s> \r\n",sendBuffer);
    FOTA_DBG_PRINT("<--send Host: ");
    FOTA_DBG_PRINT(sendBuffer);
    if(ret != Ql_strlen(sendBuffer))
    {return -1;}
    
    // http  head:  (Accept: ........ )
    Ql_memset(sendBuffer, 0x00, sizeof(sendBuffer));
    Ql_memcpy(sendBuffer, HttpGetHead[2], Ql_strlen(HttpGetHead[2]));
    ret =  Ql_SOC_Send(socketId, sendBuffer, Ql_strlen(sendBuffer));
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--send Accept:<%s> \r\n",sendBuffer);
    FOTA_DBG_PRINT("<--send Accept: ");
    FOTA_DBG_PRINT(sendBuffer);
    if(ret != Ql_strlen(sendBuffer))
    {return -1;}
    
    // http  head:  (User-Agent: ........ )
    Ql_memset(sendBuffer, 0x00, sizeof(sendBuffer));
    Ql_memcpy(sendBuffer, HttpGetHead[3], Ql_strlen(HttpGetHead[3]));
    ret =  Ql_SOC_Send(socketId, sendBuffer, Ql_strlen(sendBuffer));
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--send User-Agent:<%s> \r\n",sendBuffer);
    FOTA_DBG_PRINT("<--send User-Agent: ");
    FOTA_DBG_PRINT(sendBuffer);
    if(ret != Ql_strlen(sendBuffer))
    {return -1;}

    // http  head:  (Connection: ........ )
    Ql_memset(sendBuffer, 0x00, sizeof(sendBuffer));
    Ql_memcpy(sendBuffer, HttpGetHead[4], Ql_strlen(HttpGetHead[4]));
    ret =  Ql_SOC_Send(socketId, sendBuffer, Ql_strlen(sendBuffer));
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--send Connection:<%s> \r\n",sendBuffer);
    FOTA_DBG_PRINT("<--send Connection: ");
    FOTA_DBG_PRINT(sendBuffer);
    if(ret != Ql_strlen(sendBuffer))
    {return -1;}
    
    // http  head:  (  \r\n    )
    Ql_memset(sendBuffer, 0x00, sizeof(sendBuffer));
    Ql_memcpy(sendBuffer, HttpGetHead[5], Ql_strlen(HttpGetHead[5]));
    ret =  Ql_SOC_Send(socketId, sendBuffer, Ql_strlen(sendBuffer));  
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--send end:<%s> \r\n",sendBuffer);
    FOTA_DBG_PRINT("<--send end: ");
    FOTA_DBG_PRINT(sendBuffer);
    if(ret != Ql_strlen(sendBuffer))
    {return -1;}

    // send http head successfully , malloc a buffer for http data form the socket now!!
    httpContext_p->http_socketdata_p = (u8 *)Ql_MEM_Alloc(QUECTEL_HTTP_DATABUFFER_SIZE);
    if(NULL == httpContext_p->http_socketdata_p)
    {
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--Malloc memory buffer for http socket buffer failed !--> \r\n");
        FOTA_DBG_PRINT("<--Malloc memory buffer for http socket buffer failed !--> \r\n");
        return -1;
    }
    httpContext_p->genhttp_dstconstptr = httpContext_p->http_socketdata_p;
    
    return TRUE;
}

s8 http_RecvHttpHead(s32 socketid, bool bContinue)
{
    HttpMainContext_t *httpContext_p  = &httpMainContext;
    bool entryfunctionexe = TRUE;
    s32 iret;
    HttpHeader_t *httpheader = &httpContext_p->httpheader;

    u8 *recvbuffer;
    s32 recvlength;
    s32 willrecvlength;
    s32 decode_length;
    
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer, " <--http_RecvHttpHead entry !-->\r\n");
    FOTA_DBG_PRINT(" <--http_RecvHttpHead entry !-->\r\n");
    if(!bContinue)
    {
        httpheader->ContentLength_Exist_InHttpResponse = FALSE;
        httpheader->Transfer_Encoding_Is_chunked = FALSE;
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<-- httpRecvHttpHead bContinue= FALSE-->"); 
        FOTA_DBG_PRINT("<-- httpRecvHttpHead bContinue= FALSE-->");
    }

cuntinuerecvdata:

    if(httpContext_p->genhttp_dstpos)
    {
        Ql_memcpy(httpContext_p->genhttp_dstconstptr, httpContext_p->genhttp_dstconstptr + httpContext_p->genhttp_dstpos , httpContext_p->genhttp_dstvaliddatalen);
        httpContext_p->genhttp_dstpos =  0;
    }

    recvbuffer = httpContext_p->genhttp_dstconstptr+httpContext_p->genhttp_dstvaliddatalen;
    willrecvlength = httpContext_p->genhttp_dstconstsize  - httpContext_p->genhttp_dstvaliddatalen;

    recvlength = 0;
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--want read socket datalength=%d-->\r\n", willrecvlength);
    FOTA_DBG_PRINT("<--want read socket datalength=%d-->\r\n");
    /*receive buffer is full*/
    if(willrecvlength && (!httpContext_p->bpeersocketclose))
    {
        recvlength = Ql_SOC_Recv(httpContext_p->socketid, recvbuffer, willrecvlength);
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--socket read received length = %d -->\r\n", recvlength);
        //UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--http head:%s -->\r\n", recvbuffer);//if receive http head fail, you can debug via this message log.
        if(recvlength == SOC_WOULDBLOCK)
        {
            if(FALSE == httpContext_p->httpGettimerstate)
            {
                Ql_Timer_Start(httpContext_p->httpGetHeadTimer,httpContext_p->httpGettimeout,FALSE);
                httpContext_p->httpGettimerstate = FALSE;
            }
            return HTTP_RESULT_ERROR_WOULDBLOCK;
        }
        else if(recvlength <= 0)
        {
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--http soc_recv == 0-->");
            FOTA_DBG_PRINT("<--http soc_recv == 0-->");
            httpContext_p->bpeersocketclose = TRUE;
            return HTTP_RESULT_ERROR_SOC_CLOSE;
        }
        //else if(recvlength > 0)
        else
        {
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--http head recv dataLen = %d-->\r\n",recvlength); 
        }
    }
    else/*receive buffer is full, and not  receive http head !!*/
    {
        recvlength = 0;
        return HTTP_RESULT_ERROR_DECODEERROR;
    }
    //if(recvlength > 0)
    {
        httpContext_p->genhttp_dstvaliddatalen += recvlength;
        do
        {
            iret = http_DecodeHeader(httpheader, httpContext_p->genhttp_dstconstptr + httpContext_p->genhttp_dstpos, httpContext_p->genhttp_dstvaliddatalen, &decode_length);
            if((iret == HTTP_CODERESULT_OK) || (iret == HTTP_CODERESULT_END))
            {
                httpContext_p->genhttp_dstpos += decode_length;
                httpContext_p->genhttp_dstvaliddatalen -= decode_length;
            }
        } while(iret == HTTP_CODERESULT_OK);

        if(iret == HTTP_CODERESULT_END)
        {
            if ((httpheader->httpresponse/100) == 3)     /* Will.Shao, for http download, 2012.05.22 */
                return HTTP_RESULT_ERROR_HTTP_RELOCATION;
            else if((httpheader->httpresponse/100) != 2)
                return HTTP_RESULT_ERROR_HTTP_RESPONSE_FAILED;

            /*there is no  ContentLength ,*/
            if(!httpheader->ContentLength_Exist_InHttpResponse)
            {
                UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--httpRecvHttpHead not exist ContentLength !-->\r\n");
                FOTA_DBG_PRINT("<--httpRecvHttpHead not exist ContentLength !-->\r\n");
                httpheader->ContentLength = 0xFFFFFFFF;
            }
            else
            {
                
            }
            /*http head receive end, start to receive the body data now! */
            httpContext_p->ContentLength = httpheader->ContentLength;
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--httpRecvHttpHead() Get ContentLength= %d-->\r\n",httpContext_p->ContentLength); 
            httpContext_p->getbody = TRUE;
            if(0xFFFFFFFF ==httpContext_p->ContentLength)
            {
                iret = http_RecvHttpChunkedBody(httpContext_p->socketid, FALSE);  
                return iret;
            }
            else
            {
                iret = http_RecvHttpBody(httpContext_p->socketid, FALSE);
                return iret;
            }
            
        }
        else if(iret == HTTP_CODERESULT_DATAABSENT)
        {
            if(httpContext_p->bpeersocketclose)
            {
                if ((httpheader->httpresponse/100) == 3)   
                    return HTTP_RESULT_ERROR_HTTP_RELOCATION;
                else if((httpheader->httpresponse/100) != 2)
                    return HTTP_RESULT_ERROR_HTTP_RESPONSE_FAILED;
                
                UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--httpRecvHttpHead() bpeersocketclose-->\r\n"); 
                FOTA_DBG_PRINT("<--httpRecvHttpHead() bpeersocketclose-->\r\n");
                return HTTP_RESULT_OK;
            }
            
            /* data is not enough*/
            goto cuntinuerecvdata;
        }
        else //if(iret < 0)
        {
            return HTTP_RESULT_ERROR_DECODEERROR;
        }
    }
    
    return HTTP_RESULT_OK; 
}

s8 http_RecvHttpChunkedBody(s32 socketid, bool bContinue)
{
    bool retValue;
    FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue) ;
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Don't support http chuned encode !!-->\r\n");
    FOTA_DBG_PRINT("<--Don't support http chuned encode !!-->\r\n");
    return -1;
}
s8 http_RecvHttpBody(s32 socketid, bool bContinue)
{
    HttpMainContext_t *httpContext_p  = &httpMainContext;
    s32 ret;
    bool retValue;
    s32 fileDLpresent;
    s32 fileDLpresentReport;
    u8 *recvbuffer;
    s32 recvlength;
    s32 willrecvlength;

cuntinuerecvbodydata:

    if(httpContext_p->genhttp_dstpos)
    {
        Ql_memcpy(httpContext_p->genhttp_dstconstptr, httpContext_p->genhttp_dstconstptr + httpContext_p->genhttp_dstpos , httpContext_p->genhttp_dstvaliddatalen);
        httpContext_p->genhttp_dstpos =  0;
    }
        
    recvbuffer = httpContext_p->genhttp_dstconstptr+httpContext_p->genhttp_dstvaliddatalen;
    willrecvlength = httpContext_p->genhttp_dstconstsize  - httpContext_p->genhttp_dstvaliddatalen;

    recvlength = 0;
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--socket read body willrecvlength=%d-->\r\n", willrecvlength);

    if(willrecvlength && (!httpContext_p->bpeersocketclose))
    {
        recvlength = Ql_SOC_Recv(httpContext_p->socketid, recvbuffer, willrecvlength);
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--socket read body recvlength = %d -->\r\n", recvlength);
        if(recvlength == SOC_WOULDBLOCK)
        {
                if(FALSE == httpContext_p->httpGettimerstate)
                {
                    Ql_Timer_Start(httpContext_p->httpGetHeadTimer,httpContext_p->httpGettimeout,FALSE);
                    httpContext_p->httpGettimerstate = FALSE;
                }
                return HTTP_RESULT_ERROR_WOULDBLOCK;
        }
        else if(recvlength <= 0)
        {
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--body http soc_recv == 0-->");
            FOTA_DBG_PRINT("<--body http soc_recv == 0-->\r\n");
            httpContext_p->bpeersocketclose = TRUE;

            return HTTP_RESULT_ERROR_SOC_CLOSE;
        }
        
        httpContext_p->genhttp_dstvaliddatalen += recvlength;
        //Ql_UART_Write(UART_PORT2, httpContext_p->genhttp_dstconstptr+httpContext_p->genhttp_dstpos,httpContext_p->genhttp_dstvaliddatalen );
        /************  write the data to fota temp region*********************/
        if(httpContext_p->receivedbodydata <= httpContext_p->ContentLength)
        {
            ret = Ql_FOTA_WriteData(httpContext_p->genhttp_dstvaliddatalen,(s8*)(httpContext_p->genhttp_dstconstptr+httpContext_p->genhttp_dstpos));
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<-- Fota write, len=%d, ret=%d, totlesize=%d -->\r\n", httpContext_p->receivedbodydata +httpContext_p->genhttp_dstvaliddatalen, ret, httpContext_p->ContentLength);
            if(ret != 0)
            {
                UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--Ql_FOTA_WriteData failed(ret =%d)-->\r\n",ret); 
                FOTA_DBG_PRINT("<-- Ql_FOTA_WriteData failed -->\r\n");
                FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
                return -1;
            }
            //Ql_Sleep(100);
        }       
        /*************************************************************/
        httpContext_p->receivedbodydata += httpContext_p->genhttp_dstvaliddatalen;
        httpContext_p->genhttp_dstvaliddatalen = 0;
        httpContext_p->genhttp_dstpos =  0;
    }

    fileDLpresent = (100*(httpContext_p->receivedbodydata))/httpContext_p->ContentLength;
    if((0 == fileDLpresent%5) && (fileDLpresent != fileDLpresentReport))
    {
        fileDLpresentReport = fileDLpresent;
        FOTA_UPGRADE_IND(UP_GETTING_FILE,fileDLpresentReport,retValue);
    }
    if(httpContext_p->receivedbodydata >= httpContext_p->ContentLength)
    {
        //Ql_SOC_Close(httpContext_p->socketid);
        FOTA_DBG_PRINT("<-- Close socket -->\r\n");
        httpContext_p->socketid = -1;
        httpContext_p->bpeersocketclose = TRUE;
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--bodydata received End (%d)-->\r\n",httpContext_p->receivedbodydata);
        FOTA_DBG_PRINT("<-- bodydata received End -->\r\n");
        //FOTA_UPGRADE_IND(UP_GET_FILE_OK,100,retValue);
        FOTA_DBG_PRINT("<-- Downloading finished -->\r\n");
        if(NULL != httpContext_p->http_socketdata_p)
        {
            Ql_MEM_Free((void *)(httpContext_p->http_socketdata_p) );
            httpContext_p->http_socketdata_p = NULL;
        }
        Ql_Sleep(300);
        ret = Ql_FOTA_Finish();     //Finish the upgrade operation ending with calling this API
        if(ret !=0)
        {
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--Ql_FOTA_Finish failed(ret =%d)-->\r\n",ret); 
            FOTA_DBG_PRINT("<-- Ql_FOTA_Finish failed -->\r\n");
            FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
            return -1;
        }

        FOTA_UPGRADE_IND(UP_SYSTEM_REBOOT,100,retValue);
        if((NULL == Fota_UpgardeState) ||retValue)  //  if fota upgrade callback function return TRUE in the UP_SYSTEM_REBOOT case ,the system upgrade app at once.
        {
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Fota upgrade callback return TRUE!!, system reboot for upgrade now-->\r\n");
            FOTA_DBG_PRINT("<--Fota upgrade callback return TRUE!!, system reboot for upgrade now-->\r\n");
			Ql_Sleep(300);
			ret = Ql_FOTA_Update();  // set a flag then reboot the system , then the will  auto upgrade app.bin. 
            if(ret != 0)
            {
                FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
                UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--Ql_FOTA_Update(ret =%d)-->\r\n",ret);     
                FOTA_DBG_PRINT("<-- Ql_FOTA_Finish failed -->\r\n");
                return -1;
// fota update failed
            }
            else
            {
                ////If update OK, module will reboot automaticly
            }
        }
        else
        {
            // if  upgrade state callback function return false, you must call Ql_FOTA_Update function  before you reboot the system
            return HTTP_RESULT_OK;
        }
    }      
    goto cuntinuerecvbodydata;
}

void Httpcallback_socket_read(s32 socketId, s32 errCode, void* customParam )
{
    HttpMainContext_t *httpContext_p  = &httpMainContext;
    bool retValue;
    s8 ret = HTTP_RESULT_OK;
    
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--callback socket read (socketId=%d, errCode=%d)--> \r\n",socketId, errCode);
    if(SOC_SUCCESS != errCode)
    {
        FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
        Ql_MEM_Free(httpContext_p->http_socketdata_p);  // read socket data error ,end the foat upgrade.
        httpContext_p->http_socketdata_p = NULL;
        return;
    }
    if(NULL == httpContext_p->http_socketdata_p)
    {
        // even if http head send success, and receive data form the http socket. but memory allocation failed ,return fota upgrade too .
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer, " <--Because Malloc  failed , so exit the socket read !--> \r\n");
        return;
    }
    if(TRUE == httpContext_p->httpGettimerstate)
    {
        Ql_Timer_Stop(httpContext_p->httpGetHeadTimer);
        httpContext_p->httpGettimerstate = FALSE;
    }

    if(0 == httpContext_p->ContentLength) // head received END
    {
        ret = http_RecvHttpHead(httpContext_p->socketid, TRUE);
    }
    else if(0xFFFFFFFF == httpContext_p->ContentLength)// chunked 
    {
        ret = http_RecvHttpChunkedBody(httpContext_p->socketid, TRUE);
    }
    else
    {
        ret = http_RecvHttpBody(httpContext_p->socketid, TRUE);
    }

    // Read http head (or http Body )failed . exit the upgrade.
    if(HTTP_RESULT_OK !=ret && HTTP_RESULT_ERROR_WOULDBLOCK != ret) //receive failed 
    {
        if(NULL != httpContext_p->http_socketdata_p)
        {
            Ql_MEM_Free((void *)(httpContext_p->http_socketdata_p));
            httpContext_p->http_socketdata_p = NULL;
        }
        FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
    }
        
    return;
}

void HttpCallback_GPRS_Actived(u8 contexId, s32 errCode, void* customParam)
{
    // we used Ql_GPRS_ActivateEx function to activate the PDP,so this callback will not be invoked
    return;
}
void HttpCallBack_GPRS_Deactived(u8 contextId, s32 errCode, void* customParam )
{
    return;
}

void Httpcallback_socket_connect(s32 socketId, s32 errCode, void* customParam )
{
    // 
    return;
}

void Httpcallback_socket_close(s32 socketId, s32 errCode, void* customParam )
{
    
    return;
}
void Httpcallback_socket_accept(s32 listenSocketId, s32 errCode, void* customParam )
{
    
    return;
}
void Httpcallback_socket_write(s32 socketId, s32 errCode, void* customParam )
{
    return;    
//  http_callback_socket_notify(SOC_WRITE, socketId);
}


void Callback_GetIpByHostName(u8 contexId, u8 requestId, s32 errCode,  u32 ipAddrCnt, u32* ipAddr)
{
    u8 i=0;
    bool retValue;
    HttpMainContext_t *httpContext_p  = &httpMainContext;
    u8* pu8IpAddr = (u8*)ipAddr;
    if (errCode == SOC_SUCCESS)
    {
        for(i=0;i<ipAddrCnt;i++)
        {
            pu8IpAddr += (i*4);
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--GetIpByHostName Entry=%d, ip=%d.%d.%d.%d-->\r\n",i,pu8IpAddr[0],pu8IpAddr[1],pu8IpAddr[2],pu8IpAddr[3]);
        }
        Ql_memcpy(httpContext_p->hostipAddres,ipAddr, 4);
        http_GetImagefilefromServer();
    }
    else
    {
        FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Callback_GetIpByHostName  FAILD (err =%d)-->\r\n", errCode);
    }
}

void http_TimerCallback(u32 timerId, void* param)
{
    bool retValue;
    HttpMainContext_t *httpContext_p  = &httpMainContext;
    if(HTTP_GETREADTIMERID == timerId)// http send get head ,but server no response. or read data form http server no response
    {
        FOTA_UPGRADE_IND(UP_UPGRADFAILED,0,retValue);
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Http server no response !!-->\r\n");
        FOTA_DBG_PRINT("<--Http server no response !!-->\r\n");
        Ql_Sleep(100);
        httpContext_p->httpGettimerstate = FALSE;
        if(NULL != httpContext_p->http_socketdata_p)
        {
            Ql_MEM_Free((void *)(httpContext_p->http_socketdata_p));
            httpContext_p->http_socketdata_p = NULL;
        }
    }
}



#endif  //__OCPU_FOTA_BY_HTTP__

