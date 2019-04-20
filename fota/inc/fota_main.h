
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
 *   fota_main.h 
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 * This file is for upgrading application processing by FOTA.
 *
 * Author:
 * -------
 * -------
 *
 *============================================================================
 *             HISTORY
 *----------------------------------------------------------------------------
 * 
 ****************************************************************************/
#ifndef __FOTA_MAIN_H__
#define __FOTA_MAIN_H__
#include "ql_type.h"
#include "ql_uart.h"
#include "ql_gpio.h"

/******************************************************************************
* Macros
******************************************************************************/
#define UPGRADE_APP_DEBUG_ENABLE 0
#if UPGRADE_APP_DEBUG_ENABLE > 0
#define UPGRADE_APP_DEBUG_PORT  UART_PORT1
#define DBG_BUF_LEN   512
#define UPGRADE_APP_DEBUG(BUF,...) QL_TRACE_LOG(UPGRADE_APP_DEBUG_PORT,BUF,DBG_BUF_LEN,__VA_ARGS__)
#else
#define UPGRADE_APP_DEBUG(BUF,...) 
#endif

#define FOTA_DBG_SMPL_PRINT_EN  1
#if FOTA_DBG_SMPL_PRINT_EN > 0
#define FOTA_DBG_PRINT_PORT  UART_PORT1
#define FOTA_DBG_PRINT(STR)  Ql_UART_Write(FOTA_DBG_PRINT_PORT, (u8*)(STR), Ql_strlen((const char *)(STR)))
#else
#define FOTA_DBG_PRINT(STR)
#endif

#define MSG_ID_RESET_MODULE_REQ     (MSG_ID_URC_INDICATION + 3)
#define MSG_ID_FTP_RESULT_IND       (MSG_ID_URC_INDICATION + 4)

#define FTP_RESULT_SUCCEED      0
#define FTP_RESULT_FAILED      -1

typedef enum{
    UP_FOTAINITFAIL, // Fota init failed.
    UP_START,//Fota upgrade start£¬
    UP_URLDECODEFAIL,
    UP_CONNECTING,// connect to the server
    UP_CONNECTED,// connect ok
    UP_GETTING_FILE,// download the bin file
    UP_GET_FILE_OK,// the bin file get ok,
    UP_SYSTEM_REBOOT,//before system reboot£¬reporting this state.
    UP_UPGRADFAILED,
    FOTA_STATE_END
}Upgrade_State;

typedef bool (* Callback_Upgrade_State)(Upgrade_State state, s32 fileDLPercent);
extern Upgrade_State  g_FOTA_State;
extern Callback_Upgrade_State Fota_UpgardeState;
#define FOTA_UPGRADE_IND(x,y,z)  if(NULL != Fota_UpgardeState) {z=Fota_UpgardeState(x,y);}

/*****************************************************************
* Function:     Ql_FOTA_StartUpgrade
*
* Description:
*               This function launches the upgrading application processing by FOTA. 
*               User may use FTP or HTTP as the GPRS data transmission channel.
*
* Parameters:
*               url:
*                   the URL address of the destination bin file. 
*    
*                   The URL format for http is:   http://hostname/filePath/fileName:port
*                   NOTE:  if ":port" is be ignored, it means the port is the default port of http (80 port)
*
*                   The URL format for ftp is:   ftp://hostname/filePath/fileName:port@username:password
*                   NOTE:1. If ":port" is be ignored, it means the port is the default port of ftp (21 port)
*                           If no username and password, "@username:password" can be ignored.
*                        2. you must make sure there is no '@' char before the "@username:password" string.
*
*                    eg1: ftp://www.jjj.com/filePath/xxx.bin:8021@username:password 
*                    eg2: ftp://www.jjj.com/filePath/xxx.bin@username:password     
*                    eg3: ftp://192.168.10.10/filePath/APP.bin
*                    eg4: http://23.11.67.89/filePath/xxx.bin 
*                    eg5: http://www.quectel.com:8080/filePath/xxx.bin
*                   
*                apnCfg:
*                   the APN related parameters.
*
*                callbcak_UpgradeState_Ind:
*                   callback function that reports the upgrading state.
*                   If it's NULL, a default callback function "Fota_Upgrade_States" will be adopted.
* Return:
*                0 indicates this function successes.
*               -1 indicates this function failure.
*****************************************************************/
s32 Ql_FOTA_StartUpgrade(u8* url, ST_GprsConfig* apnCfg, Callback_Upgrade_State callbcak_UpgradeState_Ind);

/*****************************************************************
* Function:     Ql_FOTA_StopUpgrade
*
* Description:
*               This function stop upgrading application processing by FOTA. 
*
* Parameters:
*               None.
* Return:
*               
*****************************************************************/
s32 Ql_FOTA_StopUpgrade(void);

#endif  //__FOTA_MAIN_H__
