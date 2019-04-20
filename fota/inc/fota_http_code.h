#ifndef __FOTA_HTTP_CODE_H__
#define __FOTA_HTTP_CODE_H__

#include "ql_type.h"
#include "ql_stdlib.h"


#define HTTP_VALUE_LENGTH 50

#define CHAR_IS_LOWER( alpha_char )   \
  ( ( (alpha_char >= 'a') && (alpha_char <= 'z') ) ?  1 : 0 )

#define CHAR_IS_UPPER( alpha_char )   \
   ( ( (alpha_char >= 'A') && (alpha_char <= 'Z') ) ? 1 : 0 )
   
 typedef enum HttpHeaderFieldTag
{
    HTTP_HEADERFIELD_END=0,
    HTTP_HEADERFIELD_RESPONSEHEAD,/*http responsehead ·*/
    HTTP_HEADERFIELD_GETMOTHED,
    HTTP_HEADERFIELD_POSTMOTHED,
    HTTP_HEADERFIELD_URL,
    HTTP_HEADERFIELD_PROTOL,
    HTTP_HEADERFIELD_HOST, 
    HTTP_HEADERFIELD_ACCEPT, 
    HTTP_HEADERFIELD_ACCEPT_CHARSET,
    HTTP_HEADERFIELD_CACHE_CONTROL,
    HTTP_HEADERFIELD_PRAGMA,
    HTTP_HEADERFIELD_CONNECTION, 
    HTTP_HEADERFIELD_USERAGENT, 
    HTTP_HEADERFIELD_ACCEPT_LANGUAGE,
    HTTP_HEADERFIELD_X_WAP_PROFILE,
    HTTP_HEADERFIELD_LINESYMBOL, 
    HTTP_HEADERFIELD_CONTENT_TYPE, 
    HTTP_HEADERFIELD_CONTENT_LENGTH, 
    HTTP_HEADERFIELD_TRANSFER_ENCODING_CHUNKED, 
    HTTP_HEADERFIELD_DATE, 
    END_OF_HTTP_HEADERFIELD
} HttpHeaderField_e;
typedef struct HttpLineValueTag
{
    HttpHeaderField_e   filedtype;
    u8            valuetype;
    u32          valuelength;
    u8            value[HTTP_VALUE_LENGTH];
}HttpLineValue_t;

typedef enum HttpCodeResultTag
{
    HTTP_CODERESULT_OK                     =0,  
    HTTP_CODERESULT_END                    =1, /*end*/
    HTTP_CODERESULT_BUFFERFULL     = 2, /*buffer full*/
    HTTP_CODERESULT_DATAABSENT    = 3, /*data */
    HTTP_CODERESULT_ERROR              = -1
} HttpCodeResult_e;

typedef struct HttpHeaderTag
{
    s32   httpresponse;/*HTTP response*/
    u8   *urladdress;/*print to  the url address, */
    u16   urladdress_length;/*not include the \0 */
    u8   hostnameorip[50];
    u8   *Accept;
    u8   *Content_Type;
    u16  hostport;
    u32  ContentLength; /*Content length*/

    bool  ContentLength_Exist_InHttpResponse;
    bool  Transfer_Encoding_Is_chunked; 
    
    u8   *Connection;    //  Keep-Alive
    u8   *User_Agent;   // QUECTEL_MODULE
    u8   *Accept_Charset;//"utf-8, us-ascii"
    u8   *Cache_Control;//"no-cache"
    u8   *x_wap_profile;
    u8   *Pragma;//"no-cache"
    u8   *Accept_Language; //"en"
}HttpHeader_t;


s32 http_DecodeHeader(HttpHeader_t *httpheader,  u8 *decode_buffer, s32 decode_buffer_length, s32 *decode_length);

#endif 

