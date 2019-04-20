
/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2013
*
*****************************************************************************/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   main.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This app demonstrates how to send AT command with RIL API, and transparently
 *   transfer the response through MAIN UART. And how to use UART port.
 *   Developer can program the application based on this example.
 * 
 ****************************************************************************/
#ifdef __CUSTOMER_CODE__
#include "main.h"
#include "uart.h"
#include "sms.h"
#include "gprs.h"

static s32 m_GprsActState    = 0;   // GPRS PDP activation state, 0= not activated, 1=activated

void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;

    UART_Initialize();

    APP_DEBUG("OpenCPU: GPRS/SMS application note\r\n");

    // START MESSAGE LOOP OF THIS TASK
    while(TRUE)
    {
        Ql_memset(&msg, 0x0, sizeof(ST_MSG));
        Ql_OS_GetMessage(&msg);

        switch(msg.message)
        {
        case MSG_ID_RIL_READY:
            APP_DEBUG("<-- RIL is ready -->\r\n");
            Ql_RIL_Initialize();

            SMS_Deinitialize();

            break;
        case MSG_ID_URC_INDICATION:
            //APP_DEBUG("<-- Received URC: type: %d, -->\r\n", msg.param1);
            switch (msg.param1)
            {
            case URC_SYS_INIT_STATE_IND:
                {
                    APP_DEBUG("<-- Sys Init Status %d -->\r\n", msg.param2);
                    if (SYS_STATE_SMSOK == msg.param2)
                    {
                        APP_DEBUG("\r\n<-- SMS module is ready -->\r\n");
                        APP_DEBUG("\r\n<-- Initialize SMS-related options -->\r\n");

                        ret = SMS_Initialize();         
                        if (!ret)
                        {
                            APP_DEBUG("Fail to initialize SMS\r\n");
                        }

                        SMS_TextMode_Send();
                    }
                    break;
                }
                break;
            case URC_SIM_CARD_STATE_IND:
                 if (SIM_STAT_READY == msg.param2)
                {
                    APP_DEBUG("<-- SIM card is ready -->\r\n");
                }else{
                    APP_DEBUG("<-- SIM card is not available, cause:%d -->\r\n", msg.param2);
                    /* cause: 0 = SIM card not inserted
                     *        2 = Need to input PIN code
                     *        3 = Need to input PUK code
                     *        9 = SIM card is not recognized
                     */
                }
                break;
            case URC_GSM_NW_STATE_IND:
                 if (NW_STAT_REGISTERED == msg.param2 || NW_STAT_REGISTERED_ROAMING == msg.param2)
                {
                    APP_DEBUG("<-- Module has registered to GSM network -->\r\n");
                }else{
                    APP_DEBUG("<-- GSM network status:%d -->\r\n", msg.param2);
                    /* status: 0 = Not registered, module not currently search a new operator
                     *         2 = Not registered, but module is currently searching a new operator
                     *         3 = Registration denied 
                     */
                }

                break;
            case URC_GPRS_NW_STATE_IND:
                if (NW_STAT_REGISTERED == msg.param2 || NW_STAT_REGISTERED_ROAMING == msg.param2)
                {
                    APP_DEBUG("<-- Module has registered to GPRS network -->\r\n");
                    
                    // Module has registered to GPRS network, and app may start to activate PDP and program TCP
                    GPRS_TCP_Program();
                }else{
                    APP_DEBUG("<-- GPRS network status:%d -->\r\n", msg.param2);
                    /* status: 0 = Not registered, module not currently search a new operator
                     *         2 = Not registered, but module is currently searching a new operator
                     *         3 = Registration denied 
                     */

                    // If GPRS drops down and currently socket connection is on line, app should close socket
                    // and check signal strength. And try to reset the module.
                    if (NW_STAT_NOT_REGISTERED == msg.param2 && m_GprsActState)
                    {// GPRS drops down
                        u32 rssi;
                        u32 ber;
                        s32 nRet = RIL_NW_GetSignalQuality(&rssi, &ber);
                        APP_DEBUG("<-- Signal strength:%d, BER:%d -->\r\n", rssi, ber);
                    }
                }    
                break;
            case URC_CFUN_STATE_IND:
                APP_DEBUG("<-- CFUN Status:%d -->\r\n", msg.param2);
                break;
            case URC_COMING_CALL_IND:
                {
                    ST_ComingCall* pComingCall = (ST_ComingCall*)msg.param2;
                    APP_DEBUG("<-- Coming call, number:%s, type:%d -->\r\n", pComingCall->phoneNumber, pComingCall->type);
                    break;
                }
            case URC_CALL_STATE_IND:
                APP_DEBUG("<-- Call state:%d\r\n", msg.param2);
                break;
            case URC_NEW_SMS_IND:
                APP_DEBUG("\r\n<-- New SMS Arrives: index=%d\r\n", msg.param2);
                Hdlr_RecvNewSMS((msg.param2), FALSE);
                break;
            case URC_MODULE_VOLTAGE_IND:
                APP_DEBUG("<-- VBatt Voltage Ind: type=%d\r\n", msg.param2);
                break;
            default:
                APP_DEBUG("<-- Other URC: type=%d\r\n", msg.param1);
                break;
            }
            break;
        default:
            break;
        }
    }
}

#endif // __CUSTOMER_CODE__
