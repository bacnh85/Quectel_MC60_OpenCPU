#include "ql_stdlib.h"
#include "ql_common.h"
#include "ql_type.h"
#include "ql_error.h"
#include "ql_uart.h"
#include "ql_socket.h"
#include "ql_timer.h"
#include "ql_fota.h"
#include "ql_gprs.h"
#include "ril_network.h"
#include "fota_http.h"
#include "fota_main.h"

#ifdef __OCPU_FOTA_BY_HTTP__

#if UPGRADE_APP_DEBUG_ENABLE > 0
extern char FOTA_DBGBuffer[DBG_BUF_LEN];
#endif

s32 quectel_stricmp_bylength(char *firststr, char *secondstr, int length)
{
    char firstchar;
    char secondchar;
    while (length)
    {
        firstchar = *firststr;
        secondchar = *secondstr;
        if (CHAR_IS_LOWER(firstchar))
        {
            firstchar +=  'A'-'a';
        }
        if (CHAR_IS_LOWER(secondchar))
        {
            secondchar +=  'A'-'a';
        }
        if(firstchar > secondchar)
            return 1;
        else if(firstchar < secondchar)
            return -1;
        firststr++;
        secondstr++;

        length--;
    }

    return 0;
    
}
bool http_ip_check(char *phostName, s8 len)
{
    s16 i;
    for(i=0;i<len;i++)
    {
        if((phostName[i] <'0' || phostName[i] > '9') && (phostName[i] != '.'))
        {
            return FALSE;
        }
    }
    return TRUE;

}

/*HTTP/1.1 200 OK*/
static s32 DecodeHttpResult(u8 *startpos, s32 linelength)
{
    s32 code;
    for(;linelength>0;linelength--)
    {
        if(*startpos == 0x20)
        {
            startpos++;
            code =Ql_atoi(startpos);
            return code;
        }
        startpos++;
    }
    return -1;
}

/******************************************************************************
* Function:       
* 
* Author:     
* 
* Parameters:    
* 
* Return:          
* 
* Description:
******************************************************************************/
bool http_DecodeURL(u8 *Address, u32 *AddressValidLegth, u8 *hostip, u16 hostiplength, 
                            u8 *hostname, u16 hostnamelength, u16 *hostport)
{
    u8  hstr[8];
    u32 i;
    u8 *phostnamehead;
    u8 *phostnameTail;
    u8 ipadd[4];
    bool ret = FALSE;
    bool ip_validity = FALSE;
    u8 *puri=Address;
    u32 datalen = *AddressValidLegth;

    *hostport = HTTP_SERVICE_PORT;
    Ql_memset(hostip,0,hostiplength);
    Ql_memset(hostname,0,hostnamelength);

    do
    {
        /*Decode http://*/
        Ql_memset(hstr,0,8);

        if((*AddressValidLegth) < 7)
            break;
        Ql_memcpy(hstr,Address,7);
        for(i=0;i<7;i++)
        {
           if( (hstr[i]>= 'A') && (hstr[i] <= 'Z'))
            hstr[i] = Ql_tolower(hstr[i]);
        }
        if(Ql_strcmp((char *)hstr,"http://") == 0)
        {
            puri = Address + 7;
            datalen -= 7;
        }
        else
        {
            break;
        }
        i=0;

         /*host name*/
        phostnamehead = puri;
        phostnameTail = puri;

        while(i<datalen && puri[i] != '/' && puri[i] != ':')
        {
            i++;
            phostnameTail++;
        }

        hstr[0] = *phostnameTail;
        *phostnameTail = '\0';
        if(http_ip_check(phostnamehead,Ql_strlen(phostnamehead)))
        {
            if(Ql_strlen((char*)phostnamehead)>=hostiplength)
                break;
            Ql_strcpy((char*)hostip, (char*)phostnamehead);
        }
        else
        {
            if(Ql_strlen((char*)phostnamehead)>=hostnamelength)
                break;
            Ql_strcpy((char*)hostname,(char*)phostnamehead);
        }
        *phostnameTail = hstr[0];

        /*case   http://78.129.161.20*/
        if(datalen >= i)
            datalen -= i;
        else
            datalen = 0;
        if(datalen == 0)
        {
            Address[0] = '/';
            *AddressValidLegth = 1;
            ret = TRUE;
            break;
        }

         /* port */
         puri+=i;
         i = 0;
        if(*puri==':')
        {
            datalen -= 1;
            puri++;
            phostnamehead = puri;
            phostnameTail = puri;

            while(*puri !='/' && i<datalen ){
                puri++;
                i++;
            }		
            
            phostnameTail = puri;
            hstr[0] = *phostnameTail;
            *phostnameTail = '\0';
            *hostport = (u16 )Ql_atoi(phostnamehead);
            
            *phostnameTail = hstr[0];
        }

        /*case http://78.129.161.20:1234*/
        if(datalen >= i)
            datalen -= i;
        else
            datalen = 0;
        if(datalen == 0)
        {
            Address[0] = '/';
            *AddressValidLegth = 1;
            ret = TRUE;
            break;
        }
        /*get the path*/
        if(*puri=='/')
        {
            i=0;
            while(i<datalen)
            {
                Address[i]=puri[i];
                i++;
            }		
            *AddressValidLegth = datalen;
        }
        else
        {
            Address[0] = '/';
            *AddressValidLegth = 1;
        }
        ret = TRUE;
        
    }while(FALSE);

    hstr[0] = Address[*AddressValidLegth];
    Address[*AddressValidLegth] = '\0';
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--hostname=%s, hostip=%s, hostport=%d -->\r\n",hostname,hostip,*hostport);
    UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--AddressValidLegth=%d, Address=%s-->\r\n",*AddressValidLegth, Address);
    FOTA_DBG_PRINT("<--hostname=");
    FOTA_DBG_PRINT(hostname);
    FOTA_DBG_PRINT(" hostip=");
    FOTA_DBG_PRINT(hostip);
    FOTA_DBG_PRINT(" hostport=xx -->\r\n");
    FOTA_DBG_PRINT("<--AddressValidLegth=xx, Address=");
    FOTA_DBG_PRINT(Address);
    FOTA_DBG_PRINT("-->\r\n");
    Address[*AddressValidLegth] = hstr[0];
    
    return ret;
}

#if 1
/*********************************************
*return NULL,  means can't  found  "\r\n" string
*return NO NULL, return value  print to the next line,
*linelength£¬the line length£»
***********************************************/
u8* find_linesymbol(u8 *startpos, s32 length, s32 *linelength)
{
    static u8 linesymbol[2] = {0x0d,0x0a};
    u8 *talipos = startpos + length - 1;

    *linelength = 0;
    if(length <= 1)
        return NULL;
    
    do
    {
        if(Ql_memcmp(startpos, linesymbol, 2) == 0)
        {
            (*linelength) += 2;
            return startpos+2;
        }
        
        startpos++;
        (*linelength)++;
    }while(startpos<talipos);

    *linelength = 0;
  //   UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--find_linesymbol end return !!!!;-->\r\n");   
    return NULL;
}

static bool DecodeHttpLine(u8 *startpos, s32 linelength, HttpLineValue_t *lineValue)
{
    s32 i;

    //find the HTTP head
    if((quectel_stricmp_bylength((char*)startpos, (char*)"HTTP",4) == 0) && (linelength > 4))
    {
        i = DecodeHttpResult(startpos, linelength);
        if((i==-1) || (i != 200))/*error response*/
        {
            
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--http decode not 200 OK-->\r\n");
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--http response:%s-->\r\n",startpos);
            FOTA_DBG_PRINT("<--http decode not 200 OK-->\r\n");
            FOTA_DBG_PRINT("<--http response:");
            FOTA_DBG_PRINT(startpos);
        }

        lineValue->filedtype = HTTP_HEADERFIELD_RESPONSEHEAD;
        lineValue->valuetype = 0;
        lineValue->valuelength = 4;
        *(s32*)lineValue->value = i;
        return TRUE;
    }
    
    /*Content-Length: 77*/        
    if((linelength > 15) && (quectel_stricmp_bylength((char*)startpos, "CONTENT-LENGTH:", 15) == 0))
    {
        lineValue->filedtype = HTTP_HEADERFIELD_CONTENT_LENGTH;
        lineValue->valuetype = 0;
        lineValue->valuelength = 4;
        *(s32*)lineValue->value = Ql_atoi(startpos+15);
        return TRUE;
    }

    /*Transfer-Encoding: chunked*/
    if((linelength > 26) && (quectel_stricmp_bylength((char*)startpos, "TRANSFER-ENCODING: CHUNKED", 26)== 0))
    {
        lineValue->filedtype = HTTP_HEADERFIELD_TRANSFER_ENCODING_CHUNKED;
        lineValue->valuetype = 0;
        lineValue->valuelength = 4;
        return TRUE;
    }

    /*Date: */
    if((linelength > 26) && (quectel_stricmp_bylength((char*)startpos, "DATE:", 5)== 0))
    {
        lineValue->filedtype = HTTP_HEADERFIELD_DATE;
        lineValue->valuetype = 0;
        lineValue->valuelength = linelength-5;
        Ql_strncpy((char*)(lineValue->value), (char*)(startpos+5), HTTP_VALUE_LENGTH);
        lineValue->value[HTTP_VALUE_LENGTH-1] = 0;
        if(lineValue->valuelength < HTTP_VALUE_LENGTH)
        {
            lineValue->value[lineValue->valuelength] = 0;
        }
        return TRUE;
    }
    
    return FALSE;
}

/******************************************************************************
* Function:       
* 
* Author:     
* 
* Parameters:    
* 
* Return:          
    HTTP_CODERESULT_OK                     
    HTTP_CODERESULT_END                   
    HTTP_CODERESULT_DATAABSENT   
    HTTP_CODERESULT_ERROR             
* Description:
******************************************************************************/

s32 dbg_httpresponse = 0;
u32 dbg_ContentLength = 0;
s32 http_DecodeHeader(HttpHeader_t *httpheader,  u8 *decode_buffer, s32 decode_buffer_length, s32 *decode_length)
{
    u8 *tailpos;
    s32 linelength;
    u8 *startpos;
    bool bret;
    HttpLineValue_t lineValue;
    HttpMainContext_t     *httpContext_p = &httpMainContext;

    if(decode_length)
        *decode_length = 0;

    tailpos = find_linesymbol(decode_buffer, decode_buffer_length, &linelength);
    if(tailpos == NULL)
        return HTTP_CODERESULT_DATAABSENT;

    *decode_length = linelength;

    bret = DecodeHttpLine(decode_buffer, linelength, &lineValue);
    if(bret && lineValue.filedtype == HTTP_HEADERFIELD_RESPONSEHEAD)
    {
        httpheader->httpresponse = *((s32*)(lineValue.value));
        dbg_httpresponse = httpheader->httpresponse;
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--http decode http response=%d-->\r\n",httpheader->httpresponse);
    }
    else if(bret && lineValue.filedtype == HTTP_HEADERFIELD_CONTENT_LENGTH)
    {
        httpheader->ContentLength = *((u32*)(lineValue.value));
        dbg_ContentLength = httpheader->ContentLength;
        httpheader->ContentLength_Exist_InHttpResponse = TRUE;
        httpheader->Transfer_Encoding_Is_chunked = FALSE;
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer, "<--http decode ContentLength=%d-->\r\n",httpheader->ContentLength);
    }
    else if(bret && lineValue.filedtype == HTTP_HEADERFIELD_TRANSFER_ENCODING_CHUNKED)
    {
        httpheader->ContentLength = 0xFFFFFFFF;
        dbg_ContentLength = httpheader->ContentLength;
        httpheader->ContentLength_Exist_InHttpResponse = FALSE;
        httpheader->Transfer_Encoding_Is_chunked = TRUE;
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--http decode TRANSFER_ENCODING_CHUNKED-->\r\n");
        FOTA_DBG_PRINT("<--http decode TRANSFER_ENCODING_CHUNKED-->\r\n");
    }
    else if(bret && lineValue.filedtype == HTTP_HEADERFIELD_DATE)
    {
        // not need. 
    }  

    {
        if((linelength == 2) &&((httpheader->httpresponse/100) != 1))//lii modify for http 100 continue
        {
            return HTTP_CODERESULT_END;
        }
    }
    
    return HTTP_CODERESULT_OK;
}

#endif



#endif  //__OCPU_FOTA_BY_HTTP__

