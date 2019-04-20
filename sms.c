#ifdef __CUSTOMER_CODE__

#include "main.h"
#include "sms.h"

/***********************************************************************
 * MACRO CONSTANT DEFINITIONS
************************************************************************/

#define CON_SMS_BUF_MAX_CNT   (1)
#define CON_SMS_SEG_MAX_CHAR  (160)
#define CON_SMS_SEG_MAX_BYTE  (4 * CON_SMS_SEG_MAX_CHAR)
#define CON_SMS_MAX_SEG       (7)

typedef struct
{
    u8 aData[CON_SMS_SEG_MAX_BYTE];
    u16 uLen;
} ConSMSSegStruct;

typedef struct
{
    u16 uMsgRef;
    u8 uMsgTot;

    ConSMSSegStruct asSeg[CON_SMS_MAX_SEG];
    bool abSegValid[CON_SMS_MAX_SEG];
} ConSMSStruct;

/***********************************************************************
 * FUNCTION DECLARATIONS
************************************************************************/
static bool ConSMSBuf_IsIntact(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,u8 uIdx,ST_RIL_SMS_Con *pCon);
static bool ConSMSBuf_AddSeg(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,u8 uIdx,ST_RIL_SMS_Con *pCon,u8 *pData,u16 uLen);
static s8 ConSMSBuf_GetIndex(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,ST_RIL_SMS_Con *pCon);
static bool ConSMSBuf_ResetCtx(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,u8 uIdx);

/***********************************************************************
 * GLOBAL DATA DEFINITIONS
************************************************************************/
ConSMSStruct g_asConSMSBuf[CON_SMS_BUF_MAX_CNT];

/***********************************************************************
 * MACRO FUNCTION DEFINITIONS
************************************************************************/


/*****************************************************************************
 * FUNCTION
 *  ConSMSBuf_GetIndex
 *
 * DESCRIPTION
 *  This function is used to get available index in <pCSBuf>
 *  
 * PARAMETERS
 *  <pCSBuf>     The SMS index in storage,it starts from 1
 *  <uCSMaxCnt>  TRUE: The module should reply a SMS to the sender; FALSE: The module only read this SMS.
 *  <pCon>       The pointer of 'ST_RIL_SMS_Con' data
 *
 * RETURNS
 *  -1:   FAIL! Can not get available index
 *  OTHER VALUES: SUCCESS.
 *
 * NOTE
 *  1. This is an internal function
 *****************************************************************************/
static s8 ConSMSBuf_GetIndex(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,ST_RIL_SMS_Con *pCon)
{
	u8 uIdx = 0;
	
    if(    (NULL == pCSBuf) || (0 == uCSMaxCnt) 
        || (NULL == pCon)
      )
    {
        APP_DEBUG("Enter ConSMSBuf_GetIndex,FAIL! Parameter is INVALID. pCSBuf:%x,uCSMaxCnt:%d,pCon:%x\r\n",pCSBuf,uCSMaxCnt,pCon);
        return -1;
    }

    if((pCon->msgTot) > CON_SMS_MAX_SEG)
    {
        APP_DEBUG("Enter ConSMSBuf_GetIndex,FAIL! msgTot:%d is larger than limit:%d\r\n",pCon->msgTot,CON_SMS_MAX_SEG);
        return -1;
    }
    
	for(uIdx = 0; uIdx < uCSMaxCnt; uIdx++)  //Match all exist records
	{
        if(    (pCon->msgRef == pCSBuf[uIdx].uMsgRef)
            && (pCon->msgTot == pCSBuf[uIdx].uMsgTot)
          )
        {
            return uIdx;
        }
	}

	for (uIdx = 0; uIdx < uCSMaxCnt; uIdx++)
	{
		if (0 == pCSBuf[uIdx].uMsgTot)  //Find the first unused record
		{
            pCSBuf[uIdx].uMsgTot = pCon->msgTot;
            pCSBuf[uIdx].uMsgRef = pCon->msgRef;
            
			return uIdx;
		}
	}

    APP_DEBUG("Enter ConSMSBuf_GetIndex,FAIL! No avail index in ConSMSBuf,uCSMaxCnt:%d\r\n",uCSMaxCnt);
    
	return -1;
}

/*****************************************************************************
 * FUNCTION
 *  ConSMSBuf_AddSeg
 *
 * DESCRIPTION
 *  This function is used to add segment in <pCSBuf>
 *  
 * PARAMETERS
 *  <pCSBuf>     The SMS index in storage,it starts from 1
 *  <uCSMaxCnt>  TRUE: The module should reply a SMS to the sender; FALSE: The module only read this SMS.
 *  <uIdx>       Index of <pCSBuf> which will be stored
 *  <pCon>       The pointer of 'ST_RIL_SMS_Con' data
 *  <pData>      The pointer of CON-SMS-SEG data
 *  <uLen>       The length of CON-SMS-SEG data
 *
 * RETURNS
 *  FALSE:   FAIL!
 *  TRUE: SUCCESS.
 *
 * NOTE
 *  1. This is an internal function
 *****************************************************************************/
static bool ConSMSBuf_AddSeg(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,u8 uIdx,ST_RIL_SMS_Con *pCon,u8 *pData,u16 uLen)
{
    u8 uSeg = 1;
    
    if(    (NULL == pCSBuf) || (0 == uCSMaxCnt) 
        || (uIdx >= uCSMaxCnt)
        || (NULL == pCon)
        || (NULL == pData)
        || (uLen > (CON_SMS_SEG_MAX_CHAR * 4))
      )
    {
        APP_DEBUG("Enter ConSMSBuf_AddSeg,FAIL! Parameter is INVALID. pCSBuf:%x,uCSMaxCnt:%d,uIdx:%d,pCon:%x,pData:%x,uLen:%d\r\n",pCSBuf,uCSMaxCnt,uIdx,pCon,pData,uLen);
        return FALSE;
    }

    if((pCon->msgTot) > CON_SMS_MAX_SEG)
    {
        APP_DEBUG("Enter ConSMSBuf_GetIndex,FAIL! msgTot:%d is larger than limit:%d\r\n",pCon->msgTot,CON_SMS_MAX_SEG);
        return FALSE;
    }

    uSeg = pCon->msgSeg;
    pCSBuf[uIdx].abSegValid[uSeg-1] = TRUE;
    Ql_memcpy(pCSBuf[uIdx].asSeg[uSeg-1].aData,pData,uLen);
    pCSBuf[uIdx].asSeg[uSeg-1].uLen = uLen;
    
	return TRUE;
}

/*****************************************************************************
 * FUNCTION
 *  ConSMSBuf_IsIntact
 *
 * DESCRIPTION
 *  This function is used to check the CON-SMS is intact or not
 *  
 * PARAMETERS
 *  <pCSBuf>     The SMS index in storage,it starts from 1
 *  <uCSMaxCnt>  TRUE: The module should reply a SMS to the sender; FALSE: The module only read this SMS.
 *  <uIdx>       Index of <pCSBuf> which will be stored
 *  <pCon>       The pointer of 'ST_RIL_SMS_Con' data
 *
 * RETURNS
 *  FALSE:   FAIL!
 *  TRUE: SUCCESS.
 *
 * NOTE
 *  1. This is an internal function
 *****************************************************************************/
static bool ConSMSBuf_IsIntact(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,u8 uIdx,ST_RIL_SMS_Con *pCon)
{
    u8 uSeg = 1;
	
    if(    (NULL == pCSBuf) 
        || (0 == uCSMaxCnt) 
        || (uIdx >= uCSMaxCnt)
        || (NULL == pCon)
      )
    {
        APP_DEBUG("Enter ConSMSBuf_IsIntact,FAIL! Parameter is INVALID. pCSBuf:%x,uCSMaxCnt:%d,uIdx:%d,pCon:%x\r\n",pCSBuf,uCSMaxCnt,uIdx,pCon);
        return FALSE;
    }

    if((pCon->msgTot) > CON_SMS_MAX_SEG)
    {
        APP_DEBUG("Enter ConSMSBuf_GetIndex,FAIL! msgTot:%d is larger than limit:%d\r\n",pCon->msgTot,CON_SMS_MAX_SEG);
        return FALSE;
    }
        
	for (uSeg = 1; uSeg <= (pCon->msgTot); uSeg++)
	{
        if(FALSE == pCSBuf[uIdx].abSegValid[uSeg-1])
        {
            APP_DEBUG("Enter ConSMSBuf_IsIntact,FAIL! uSeg:%d has not received!\r\n",uSeg);
            return FALSE;
        }
	}
    
    return TRUE;
}

/*****************************************************************************
 * FUNCTION
 *  ConSMSBuf_ResetCtx
 *
 * DESCRIPTION
 *  This function is used to reset ConSMSBuf context
 *  
 * PARAMETERS
 *  <pCSBuf>     The SMS index in storage,it starts from 1
 *  <uCSMaxCnt>  TRUE: The module should reply a SMS to the sender; FALSE: The module only read this SMS.
 *  <uIdx>       Index of <pCSBuf> which will be stored
 *
 * RETURNS
 *  FALSE:   FAIL!
 *  TRUE: SUCCESS.
 *
 * NOTE
 *  1. This is an internal function
 *****************************************************************************/
static bool ConSMSBuf_ResetCtx(ConSMSStruct *pCSBuf,u8 uCSMaxCnt,u8 uIdx)
{
    if(    (NULL == pCSBuf) || (0 == uCSMaxCnt) 
        || (uIdx >= uCSMaxCnt)
      )
    {
        APP_DEBUG("Enter ConSMSBuf_ResetCtx,FAIL! Parameter is INVALID. pCSBuf:%x,uCSMaxCnt:%d,uIdx:%d\r\n",pCSBuf,uCSMaxCnt,uIdx);
        return FALSE;
    }
    
    //Default reset
    Ql_memset(&pCSBuf[uIdx],0x00,sizeof(ConSMSStruct));

    //TODO: Add special reset here
    
    return TRUE;
}

/*****************************************************************************
 * FUNCTION
 *  SMS_Initialize
 *
 * DESCRIPTION
 *  Initialize SMS environment.
 *  
 * PARAMETERS
 *  VOID
 *
 * RETURNS
 *  TRUE:  This function works SUCCESS.
 *  FALSE: This function works FAIL!
 *****************************************************************************/
bool SMS_Initialize(void)
{
    s32 iResult = 0;
    u8  nCurrStorage = 0;
    u32 nUsed = 0;
    u32 nTotal = 0;
    
    // Set SMS storage:
    // By default, short message is stored into SIM card. You can change the storage to ME if needed, or
    // you can do it again to make sure the short message storage is SIM card.
    #if 0
    {
        iResult = RIL_SMS_SetStorage(RIL_SMS_STORAGE_TYPE_SM,&nUsed,&nTotal);
        if (RIL_ATRSP_SUCCESS != iResult)
        {
            APP_DEBUG("Fail to set SMS storage, cause:%d\r\n", iResult);
            return FALSE;
        }
        APP_DEBUG("<-- Set SMS storage to SM, nUsed:%u,nTotal:%u -->\r\n", nUsed, nTotal);

        iResult = RIL_SMS_GetStorage(&nCurrStorage, &nUsed ,&nTotal);
        if(RIL_ATRSP_SUCCESS != iResult)
        {
            APP_DEBUG("Fail to get SMS storage, cause:%d\r\n", iResult);
            return FALSE;
        }
        APP_DEBUG("<-- Check SMS storage: curMem=%d, used=%d, total=%d -->\r\n", nCurrStorage, nUsed, nTotal);
    }
    #endif

    // Enable new short message indication
    // By default, the auto-indication for new short message is enalbed. You can do it again to 
    // make sure that the option is open.
    #if 0
    {
        iResult = Ql_RIL_SendATCmd("AT+CNMI=2,1",Ql_strlen("AT+CNMI=2,1"),NULL,NULL,0);
        if (RIL_AT_SUCCESS != iResult)
        {
            APP_DEBUG("Fail to send \"AT+CNMI=2,1\", cause:%d\r\n", iResult);
            return FALSE;
        }
        APP_DEBUG("<-- Enable new SMS indication -->\r\n");
    }
    #endif

    // Delete all existed short messages (if needed)
    iResult = RIL_SMS_DeleteSMS(0, RIL_SMS_DEL_ALL_MSG);
    if (iResult != RIL_AT_SUCCESS)
    {
        APP_DEBUG("Fail to delete all messages, iResult=%d,cause:%d\r\n", iResult, Ql_RIL_AT_GetErrCode());
        return FALSE;
    }
    APP_DEBUG("Delete all existed messages\r\n");
    
    return TRUE;
}

void SMS_Deinitialize(void) {
    int i;

    for(i = 0; i < CON_SMS_BUF_MAX_CNT; i++)
    {
        ConSMSBuf_ResetCtx(g_asConSMSBuf,CON_SMS_BUF_MAX_CNT,i);
    }
}

void SMS_TextMode_Read(u32 nIndex)
{
    s32 iResult;
    ST_RIL_SMS_TextInfo *pTextInfo = NULL;
    ST_RIL_SMS_DeliverParam *pDeliverTextInfo = NULL;
    ST_RIL_SMS_SubmitParam *pSubmitTextInfo = NULL;
    LIB_SMS_CharSetEnum eCharSet = LIB_SMS_CHARSET_GSM;
    
    pTextInfo = Ql_MEM_Alloc(sizeof(ST_RIL_SMS_TextInfo));
    if (NULL == pTextInfo)
    {
        return;
    }        

    Ql_memset(pTextInfo,0x00,sizeof(ST_RIL_SMS_TextInfo));
    iResult = RIL_SMS_ReadSMS_Text(nIndex, eCharSet, pTextInfo);
    if (iResult != RIL_AT_SUCCESS)
    {
        Ql_MEM_Free(pTextInfo);
        APP_DEBUG("< Fail to read PDU SMS, cause:%d >\r\n", iResult);
        return;
    }        
    if (RIL_SMS_STATUS_TYPE_INVALID == (pTextInfo->status))
    {
        APP_DEBUG("<-- SMS[index=%d] doesn't exist -->\r\n", nIndex);
        return;
    }

    // Resolve the read short message
    if (LIB_SMS_PDU_TYPE_DELIVER == (pTextInfo->type))
    {
        pDeliverTextInfo = &((pTextInfo->param).deliverParam);
        APP_DEBUG("<-- Read short message (index:%u) with charset %d -->\r\n", nIndex, eCharSet);

        if(FALSE == pDeliverTextInfo->conPres) //Normal SMS
        {
            APP_DEBUG(
                "short message info: \r\n\tstatus:%u \r\n\ttype:%u \r\n\talpha:%u \r\n\tsca:%s \r\n\toa:%s \r\n\tscts:%s \r\n\tdata length:%u\r\ncp:0,cy:0,cr:0,ct:0,cs:0\r\n",
                    (pTextInfo->status),
                    (pTextInfo->type),
                    (pDeliverTextInfo->alpha),
                    (pTextInfo->sca),
                    (pDeliverTextInfo->oa),
                    (pDeliverTextInfo->scts),
                    (pDeliverTextInfo->length)
           );
        }
        else
        {
            APP_DEBUG(
                "short message info: \r\n\tstatus:%u \r\n\ttype:%u \r\n\talpha:%u \r\n\tsca:%s \r\n\toa:%s \r\n\tscts:%s \r\n\tdata length:%u\r\ncp:1,cy:%d,cr:%d,ct:%d,cs:%d\r\n",
                    (pTextInfo->status),
                    (pTextInfo->type),
                    (pDeliverTextInfo->alpha),
                    (pTextInfo->sca),
                    (pDeliverTextInfo->oa),
                    (pDeliverTextInfo->scts),
                    (pDeliverTextInfo->length),
                    pDeliverTextInfo->con.msgType,
                    pDeliverTextInfo->con.msgRef,
                    pDeliverTextInfo->con.msgTot,
                    pDeliverTextInfo->con.msgSeg
           );
        }
        
        APP_DEBUG("\r\n\tmessage content:");
        APP_DEBUG("%s\r\n",(pDeliverTextInfo->data));
        APP_DEBUG("\r\n");
    }
    else if (LIB_SMS_PDU_TYPE_SUBMIT == (pTextInfo->type))
    {// short messages in sent-list of drafts-list
    } else {
        APP_DEBUG("<-- Unkown short message type! type:%d -->\r\n", (pTextInfo->type));
    }
    Ql_MEM_Free(pTextInfo);
}

void SMS_TextMode_Send(void)
{
    s32 iResult;
    u32 nMsgRef;
    char strPhNum[] = "+84915560511\0";
    char strTextMsg[] = "Quectel Module SMS Test\0";
    char strConMsgSeg1[] = "Quectel Module CON-SMS Test Start:<< GSM/GPRS modules are based on open standards and meet the requirements of applicable international standards and leg\0";
    char strConMsgSeg2[] = "islation. Our leading edge products and services that cherish the environment and support sustainable development are the result of our own know-how and \0";
    char strConMsgSeg3[] = "a perfect fit to the demands on the market. With compact size, low power consumption and extended temperature, our GSM/GPRS modules are used in a wide ra\0";
    char strConMsgSeg4[] = "nge of applications, such as automobile, VTS, Smart Metering, Wireless POS, security and a multitude of embedded devices in many industries. >> Test End.\0";
    char strConMsgChSeg1[] = "79FB8FDC6A215757957F77ED4FE16D4B8BD55F0059CBFF1A300A79FB8FDC901A4FE162E567099886514876844EA754C1548C670D52A1FF0C00470053004D002F004700500052005300206A21575757FA4E8E5F00653E548C7B26540856FD9645680751C66CD589C4768489816C42751F4EA78BBE8BA1FF0C517767097D2751D176845C3A5BF8\0";
    char strConMsgChSeg2[] = "30014F4E529F8017300162695C556E295EA67B494F1852BF0020300279FB8FDC901A4FE100470053004D002F00470050005200536A2157575E7F6CDB5E9475284E8E8F668F7D3001667A80FD8BA191CF30018FDC7A0B76D163A7300172694F538DDF8E2A300165E07EBF4ED86B3E30015B89516876D163A7300179FB52A88BA17B977B494F17\0";
    char strConMsgChSeg3[] = "591A884C4E1A76845D4C51655F0F8BBE59074E2D3002300B6D4B8BD57ED3675F\0";

    ST_RIL_SMS_SendExt sExt;

    //Initialize
    Ql_memset(&sExt,0x00,sizeof(sExt));

    APP_DEBUG("< Send Normal Text SMS begin... >\r\n");
    
    iResult = RIL_SMS_SendSMS_Text(strPhNum, Ql_strlen(strPhNum), LIB_SMS_CHARSET_GSM, strTextMsg, Ql_strlen(strTextMsg), &nMsgRef);
    if (iResult != RIL_AT_SUCCESS)
    {   
        APP_DEBUG("< Fail to send Text SMS, iResult=%d, cause:%d >\r\n", iResult, Ql_RIL_AT_GetErrCode());
        return;
    }
    APP_DEBUG("< Send Text SMS successfully, MsgRef:%u >\r\n", nMsgRef);

    // Just send 1 msg
    return;

    APP_DEBUG("< Send English Concatenate Text SMS begin... >\r\n");

    //The 1st segment of CON-SMS
    sExt.conPres = TRUE;
    sExt.con.msgType = 0xFF; //If it's 0xFF: use default CON-SMS-TYPE
    sExt.con.msgRef = 52; //Different CON-SMS shall have different <msgRef>
    sExt.con.msgTot = 4;
    sExt.con.msgSeg = 1;
    
    iResult = RIL_SMS_SendSMS_Text_Ext(strPhNum,Ql_strlen(strPhNum),LIB_SMS_CHARSET_GSM,strConMsgSeg1,Ql_strlen(strConMsgSeg1),&nMsgRef,&sExt);
    if (iResult != RIL_AT_SUCCESS)
    {   
        APP_DEBUG( 
            "< Fail to send Text SMS, cause:%d,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
            iResult,
            sExt.con.msgType,
            sExt.con.msgRef,
            sExt.con.msgTot,
            sExt.con.msgSeg
        );
        
        return;
    }
    APP_DEBUG(
        "< Send Text SMS successfully, MsgRef:%u,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
        nMsgRef,
        sExt.con.msgType,
        sExt.con.msgRef,
        sExt.con.msgTot,
        sExt.con.msgSeg
    ); 

    //The 2st segment of CON-SMS
    sExt.con.msgSeg = 2;
    iResult = RIL_SMS_SendSMS_Text_Ext(strPhNum,Ql_strlen(strPhNum),LIB_SMS_CHARSET_GSM,strConMsgSeg2,Ql_strlen(strConMsgSeg2),&nMsgRef,&sExt);
    if (iResult != RIL_AT_SUCCESS)
    {   
        APP_DEBUG(
            "< Fail to send Text SMS, cause:%d,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
            iResult,
            sExt.con.msgType,
            sExt.con.msgRef,
            sExt.con.msgTot,
            sExt.con.msgSeg
        );
        
        return;
    }
    APP_DEBUG(
        "< Send Text SMS successfully, MsgRef:%u,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
        nMsgRef,
        sExt.con.msgType,
        sExt.con.msgRef,
        sExt.con.msgTot,
        sExt.con.msgSeg
    ); 

    //The 3st segment of CON-SMS
    sExt.con.msgSeg = 3;
    iResult = RIL_SMS_SendSMS_Text_Ext(strPhNum,Ql_strlen(strPhNum),LIB_SMS_CHARSET_GSM,strConMsgSeg3,Ql_strlen(strConMsgSeg3),&nMsgRef,&sExt);
    if (iResult != RIL_AT_SUCCESS)
    {   
        APP_DEBUG(
            "< Fail to send Text SMS, cause:%d,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
            iResult,
            sExt.con.msgType,
            sExt.con.msgRef,
            sExt.con.msgTot,
            sExt.con.msgSeg
        );
        
        return;
    }
    APP_DEBUG(
        "< Send Text SMS successfully, MsgRef:%u,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
        nMsgRef,
        sExt.con.msgType,
        sExt.con.msgRef,
        sExt.con.msgTot,
        sExt.con.msgSeg
    ); 

    //The 4st segment of CON-SMS
    sExt.con.msgSeg = 4;
    iResult = RIL_SMS_SendSMS_Text_Ext(strPhNum,Ql_strlen(strPhNum),LIB_SMS_CHARSET_GSM,strConMsgSeg4,Ql_strlen(strConMsgSeg4),&nMsgRef,&sExt);
    if (iResult != RIL_AT_SUCCESS)
    {   
        APP_DEBUG( 
            "< Fail to send Text SMS, cause:%d,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
            iResult,
            sExt.con.msgType,
            sExt.con.msgRef,
            sExt.con.msgTot,
            sExt.con.msgSeg
        );
        
        return;
    }
    APP_DEBUG(
        "< Send Text SMS successfully, MsgRef:%u,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
        nMsgRef,
        sExt.con.msgType,
        sExt.con.msgRef,
        sExt.con.msgTot,
        sExt.con.msgSeg
    ); 

    APP_DEBUG("\r\n< Send Chinese Concatenate Text SMS begin... >\r\n");

    //The 1st segment of CH-CON-SMS
    sExt.conPres = TRUE;
    sExt.con.msgType = 0xFF; //If it's 0xFF: use default CON-SMS-TYPE
    sExt.con.msgRef = 53;  //Different CON-SMS shall have different <msgRef>
    sExt.con.msgTot = 3;
    sExt.con.msgSeg = 1;
    
    iResult = RIL_SMS_SendSMS_Text_Ext(strPhNum,Ql_strlen(strPhNum),LIB_SMS_CHARSET_UCS2,strConMsgChSeg1,Ql_strlen(strConMsgChSeg1),&nMsgRef,&sExt);
    if (iResult != RIL_AT_SUCCESS)
    {   
        APP_DEBUG(
            "< Fail to send Text SMS, cause:%d,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
            iResult,
            sExt.con.msgType,
            sExt.con.msgRef,
            sExt.con.msgTot,
            sExt.con.msgSeg
        );
        
        return;
    }
    APP_DEBUG(
        "< Send Text SMS successfully, MsgRef:%u,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
        nMsgRef,
        sExt.con.msgType,
        sExt.con.msgRef,
        sExt.con.msgTot,
        sExt.con.msgSeg
    ); 

    //The 2st segment of CH-CON-SMS
    sExt.con.msgSeg = 2;
    iResult = RIL_SMS_SendSMS_Text_Ext(strPhNum,Ql_strlen(strPhNum),LIB_SMS_CHARSET_UCS2,strConMsgChSeg2,Ql_strlen(strConMsgChSeg2),&nMsgRef,&sExt);
    if (iResult != RIL_AT_SUCCESS)
    {   
        APP_DEBUG(
            "< Fail to send Text SMS, cause:%d,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
            iResult,
            sExt.con.msgType,
            sExt.con.msgRef,
            sExt.con.msgTot,
            sExt.con.msgSeg
        );
        
        return;
    }
    APP_DEBUG(
        "< Send Text SMS successfully, MsgRef:%u,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
        nMsgRef,
        sExt.con.msgType,
        sExt.con.msgRef,
        sExt.con.msgTot,
        sExt.con.msgSeg
    ); 

    //The 3st segment of CH-CON-SMS
    sExt.con.msgSeg = 3;
    iResult = RIL_SMS_SendSMS_Text_Ext(strPhNum,Ql_strlen(strPhNum),LIB_SMS_CHARSET_UCS2,strConMsgChSeg3,Ql_strlen(strConMsgChSeg3),&nMsgRef,&sExt);
    if (iResult != RIL_AT_SUCCESS)
    {   
        APP_DEBUG(
            "< Fail to send Text SMS, cause:%d,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
            iResult,
            sExt.con.msgType,
            sExt.con.msgRef,
            sExt.con.msgTot,
            sExt.con.msgSeg
        );
        
        return;
    }
    APP_DEBUG(
        "< Send Text SMS successfully, MsgRef:%u,cp:1,cy:%d,cr:%d,ct:%d,cs:%d >\r\n", 
        nMsgRef,
        sExt.con.msgType,
        sExt.con.msgRef,
        sExt.con.msgTot,
        sExt.con.msgSeg
    ); 
    
}

void SMS_PDUMode_Read(u32 nIndex)
{
    s32 iResult;
    ST_RIL_SMS_PDUInfo *pPDUInfo = NULL;

    pPDUInfo = Ql_MEM_Alloc(sizeof(ST_RIL_SMS_PDUInfo));
    if (NULL == pPDUInfo)
    {
        return;
    }
    
    iResult = RIL_SMS_ReadSMS_PDU(nIndex, pPDUInfo);
    if (RIL_AT_SUCCESS != iResult)
    {
        Ql_MEM_Free(pPDUInfo);
        APP_DEBUG("< Fail to read PDU SMS, cause:%d >\r\n", iResult);
        return;
    }

    do
    {
        if (RIL_SMS_STATUS_TYPE_INVALID == (pPDUInfo->status))
        {
            APP_DEBUG("<-- SMS[index=%d] doesn't exist -->\r\n", nIndex);
            break;
        }

        APP_DEBUG("<-- Send Text SMS[index=%d] successfully -->\r\n", nIndex);
        APP_DEBUG("status:%u,data length:%u\r\n", (pPDUInfo->status), (pPDUInfo->length));
        APP_DEBUG("data = %s\r\n",(pPDUInfo->data));
    } while(0);
    
    Ql_MEM_Free(pPDUInfo);
}

void SMS_PDUMode_Send(void)
{
    s32 iResult;
    u32 nMsgRef;
    char pduStr[] = "1234923asdf";
    iResult = RIL_SMS_SendSMS_PDU(pduStr, sizeof(pduStr), &nMsgRef);
    if (RIL_AT_SUCCESS != iResult)
    {
        APP_DEBUG("< Fail to send PDU SMS, cause:%d >\r\n", iResult);
        return;
    }
    APP_DEBUG("< Send PDU SMS successfully, MsgRef:%u >\r\n", nMsgRef);

}

/*****************************************************************************
 * FUNCTION
 *  Hdlr_RecvNewSMS
 *
 * DESCRIPTION
 *  The handler function of new received SMS.
 *  
 * PARAMETERS
 *  <nIndex>     The SMS index in storage,it starts from 1
 *  <bAutoReply> TRUE: The module should reply a SMS to the sender; 
 *               FALSE: The module only read this SMS.
 *
 * RETURNS
 *  VOID
 *
 * NOTE
 *  1. This is an internal function
 *****************************************************************************/
void Hdlr_RecvNewSMS(u32 nIndex, bool bAutoReply)
{
    s32 iResult = 0;
    u32 uMsgRef = 0;
    ST_RIL_SMS_TextInfo *pTextInfo = NULL;
    ST_RIL_SMS_DeliverParam *pDeliverTextInfo = NULL;
    char aPhNum[RIL_SMS_PHONE_NUMBER_MAX_LEN] = {0,};
    const char aReplyCon[] = {"Module has received SMS."};
    bool bResult = FALSE;
    
    pTextInfo = Ql_MEM_Alloc(sizeof(ST_RIL_SMS_TextInfo));
    if (NULL == pTextInfo)
    {
        APP_DEBUG("%s/%d:Ql_MEM_Alloc FAIL! size:%u\r\n", sizeof(ST_RIL_SMS_TextInfo), __func__, __LINE__);
        return;
    }
    Ql_memset(pTextInfo, 0x00, sizeof(ST_RIL_SMS_TextInfo));
    iResult = RIL_SMS_ReadSMS_Text(nIndex, LIB_SMS_CHARSET_GSM, pTextInfo);
    if (iResult != RIL_AT_SUCCESS)
    {
        Ql_MEM_Free(pTextInfo);
        APP_DEBUG("Fail to read text SMS[%d], cause:%d\r\n", nIndex, iResult);
        return;
    }        
    
    if ((LIB_SMS_PDU_TYPE_DELIVER != (pTextInfo->type)) || (RIL_SMS_STATUS_TYPE_INVALID == (pTextInfo->status)))
    {
        Ql_MEM_Free(pTextInfo);
        APP_DEBUG("WARNING: NOT a new received SMS.\r\n");    
        return;
    }
    
    pDeliverTextInfo = &((pTextInfo->param).deliverParam);    

    if(TRUE == pDeliverTextInfo->conPres)  //Receive CON-SMS segment
    {
        s8 iBufIdx = 0;
        u8 uSeg = 0;
        u16 uConLen = 0;

        iBufIdx = ConSMSBuf_GetIndex(g_asConSMSBuf,CON_SMS_BUF_MAX_CNT,&(pDeliverTextInfo->con));
        if(-1 == iBufIdx)
        {
            APP_DEBUG("Enter Hdlr_RecvNewSMS,WARNING! ConSMSBuf_GetIndex FAIL! Show this CON-SMS-SEG directly!\r\n");

            APP_DEBUG(
                "status:%u,type:%u,alpha:%u,sca:%s,oa:%s,scts:%s,data length:%u,cp:1,cy:%d,cr:%d,ct:%d,cs:%d\r\n",
                    (pTextInfo->status),
                    (pTextInfo->type),
                    (pDeliverTextInfo->alpha),
                    (pTextInfo->sca),
                    (pDeliverTextInfo->oa),
                    (pDeliverTextInfo->scts),
                    (pDeliverTextInfo->length),
                    pDeliverTextInfo->con.msgType,
                    pDeliverTextInfo->con.msgRef,
                    pDeliverTextInfo->con.msgTot,
                    pDeliverTextInfo->con.msgSeg
            );
            APP_DEBUG("data = %s\r\n",(pDeliverTextInfo->data));

            Ql_MEM_Free(pTextInfo);
        
            return;
        }

        bResult = ConSMSBuf_AddSeg(
                    g_asConSMSBuf,
                    CON_SMS_BUF_MAX_CNT,
                    iBufIdx,
                    &(pDeliverTextInfo->con),
                    (pDeliverTextInfo->data),
                    (pDeliverTextInfo->length)
        );
        if(FALSE == bResult)
        {
            APP_DEBUG("Enter Hdlr_RecvNewSMS,WARNING! ConSMSBuf_AddSeg FAIL! Show this CON-SMS-SEG directly!\r\n");

            APP_DEBUG(
                "status:%u,type:%u,alpha:%u,sca:%s,oa:%s,scts:%s,data length:%u,cp:1,cy:%d,cr:%d,ct:%d,cs:%d\r\n",
                (pTextInfo->status),
                (pTextInfo->type),
                (pDeliverTextInfo->alpha),
                (pTextInfo->sca),
                (pDeliverTextInfo->oa),
                (pDeliverTextInfo->scts),
                (pDeliverTextInfo->length),
                pDeliverTextInfo->con.msgType,
                pDeliverTextInfo->con.msgRef,
                pDeliverTextInfo->con.msgTot,
                pDeliverTextInfo->con.msgSeg
            );
            APP_DEBUG("data = %s\r\n",(pDeliverTextInfo->data));

            Ql_MEM_Free(pTextInfo);
        
            return;
        }

        bResult = ConSMSBuf_IsIntact(
                    g_asConSMSBuf,
                    CON_SMS_BUF_MAX_CNT,
                    iBufIdx,
                    &(pDeliverTextInfo->con)
        );
        if(FALSE == bResult)
        {
            APP_DEBUG(
                "Enter Hdlr_RecvNewSMS,WARNING! ConSMSBuf_IsIntact FAIL! Waiting. cp:1,cy:%d,cr:%d,ct:%d,cs:%d\r\n",
                pDeliverTextInfo->con.msgType,
                pDeliverTextInfo->con.msgRef,
                pDeliverTextInfo->con.msgTot,
                pDeliverTextInfo->con.msgSeg
            );

            Ql_MEM_Free(pTextInfo);

            return;
        }

        //Show the CON-SMS
        APP_DEBUG(
            "status:%u,type:%u,alpha:%u,sca:%s,oa:%s,scts:%s",
            (pTextInfo->status),
            (pTextInfo->type),
            (pDeliverTextInfo->alpha),
            (pTextInfo->sca),
            (pDeliverTextInfo->oa),
            (pDeliverTextInfo->scts)
        );
        
        uConLen = 0;
        for(uSeg = 1; uSeg <= pDeliverTextInfo->con.msgTot; uSeg++)
        {
            uConLen += g_asConSMSBuf[iBufIdx].asSeg[uSeg-1].uLen;
        }

        APP_DEBUG(",data length:%u",uConLen);
        APP_DEBUG("\r\n"); //Print CR LF

        for(uSeg = 1; uSeg <= pDeliverTextInfo->con.msgTot; uSeg++)
        {
            APP_DEBUG("data = %s ,len = %d",
                g_asConSMSBuf[iBufIdx].asSeg[uSeg-1].aData,
                g_asConSMSBuf[iBufIdx].asSeg[uSeg-1].uLen
            );
        }

        APP_DEBUG("\r\n"); //Print CR LF

        //Reset CON-SMS context
        bResult = ConSMSBuf_ResetCtx(g_asConSMSBuf,CON_SMS_BUF_MAX_CNT,iBufIdx);
        if(FALSE == bResult)
        {
            APP_DEBUG("Enter Hdlr_RecvNewSMS,WARNING! ConSMSBuf_ResetCtx FAIL! iBufIdx:%d\r\n",iBufIdx);
        }

        Ql_MEM_Free(pTextInfo);
        
        return;
    }
    
    APP_DEBUG("<-- RIL_SMS_ReadSMS_Text OK. eCharSet:LIB_SMS_CHARSET_GSM,nIndex:%u -->\r\n",nIndex);
    APP_DEBUG("status:%u,type:%u,alpha:%u,sca:%s,oa:%s,scts:%s,data length:%u\r\n",
        pTextInfo->status,
        pTextInfo->type,
        pDeliverTextInfo->alpha,
        pTextInfo->sca,
        pDeliverTextInfo->oa,
        pDeliverTextInfo->scts,
        pDeliverTextInfo->length);
    APP_DEBUG("data = %s\r\n",(pDeliverTextInfo->data));
    
    Ql_strcpy(aPhNum, pDeliverTextInfo->oa);
    Ql_MEM_Free(pTextInfo);
    
    if (bAutoReply)
    {
        if (!Ql_strstr(aPhNum, "10086"))  // Not reply SMS from operator
        {
            APP_DEBUG("<-- Replying SMS... -->\r\n");
            iResult = RIL_SMS_SendSMS_Text(aPhNum, Ql_strlen(aPhNum),LIB_SMS_CHARSET_GSM,(u8*)aReplyCon,Ql_strlen(aReplyCon),&uMsgRef);
            if (iResult != RIL_AT_SUCCESS)
            {
                APP_DEBUG("RIL_SMS_SendSMS_Text FAIL! iResult:%u\r\n",iResult);
                return;
            }
            APP_DEBUG("<-- RIL_SMS_SendTextSMS OK. uMsgRef:%d -->\r\n", uMsgRef);
        }
    }
    return;
}

#endif