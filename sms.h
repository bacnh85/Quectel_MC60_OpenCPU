#ifdef __CUSTOMER_CODE__

/***********************************************************************
 * STRUCT TYPE DEFINITIONS
************************************************************************/


// API
bool SMS_Initialize(void);
void SMS_Deinitialize(void);

void SMS_TextMode_Send(void);
void Hdlr_RecvNewSMS(u32 nIndex, bool bAutoReply);

#endif