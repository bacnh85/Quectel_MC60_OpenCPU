#include "custom_feature_def.h"
#include "ql_common.h"
#include "ql_error.h"
#include "ql_type.h"
#include "ql_stdlib.h"
#include "ql_gprs.h"
#include "ql_fota.h"
#include "fota_ftp.h"
#include "fota_http.h"
#include "fota_main.h"

#ifdef __OCPU_FOTA_APP__  
Upgrade_State  g_FOTA_State = FOTA_STATE_END;
Callback_Upgrade_State Fota_UpgardeState = NULL ;
ST_FotaConfig  FotaConfig;
ST_GprsConfig  Fota_gprsCfg;
u8 Fota_apn[MAX_GPRS_APN_LEN] = "CMNET\0";
u8 Fota_userid[MAX_GPRS_USER_NAME_LEN] = "";
u8 Fota_passwd[MAX_GPRS_PASSWORD_LEN] = "";

#if UPGRADE_APP_DEBUG_ENABLE > 0
char FOTA_DBGBuffer[DBG_BUF_LEN];
#endif

static bool Fota_Upgrade_States(Upgrade_State state, s32 fileDLPercent);

extern ST_ExtWatchdogCfg* Ql_WTD_GetWDIPinCfg(void);
extern s32 Ql_GPRS_GetPDPCntxtState(u8 contextId);

s32 Ql_FOTA_StartUpgrade(u8* url, ST_GprsConfig* apnCfg, Callback_Upgrade_State callbcak_UpgradeState_Ind)
{
    s32 ret = 0;
    bool retValue;
    u8 contextId;
    
    ret = Ql_GPRS_GetPDPContextId();
    if (GPRS_PDP_ERROR == ret)
    {
        FOTA_DBG_PRINT("Fail to get pdp context\r\n");
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<-- Fail to get pdp context\r\n -->\r\n");
        return -1;
    }
    contextId = (u8)ret;

    /*---------------------------------------------------*/
    Ql_memset((void *)(&FotaConfig), 0, sizeof(ST_FotaConfig)); 
    FotaConfig.Q_gpio_pin1 = Ql_WTD_GetWDIPinCfg()->pinWtd1;
    FotaConfig.Q_feed_interval1 = 100;
    FotaConfig.Q_gpio_pin2 = Ql_WTD_GetWDIPinCfg()->pinWtd2;
    FotaConfig.Q_feed_interval2 = 100;
    ret=Ql_FOTA_Init(&FotaConfig);
    if(ret !=0)
    {
        FOTA_DBG_PRINT("Fail to Init FOTA\r\n");
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<-- Fota Init Failed (ret= %d  FotaConfig.Q_gpio_pin1 =%d) -->\r\n",ret,FotaConfig.Q_gpio_pin1);
        FOTA_UPGRADE_IND(UP_FOTAINITFAIL, 0,retValue);
        return -1;
    }
    /*---------------------------------------------------*/    

    if(NULL != callbcak_UpgradeState_Ind)
    {
        Fota_UpgardeState = callbcak_UpgradeState_Ind;
    }else{
        Fota_UpgardeState = Fota_Upgrade_States;    // Use the default callback
    }

#ifdef __OCPU_FOTA_BY_FTP__
    if(FTP_IsFtpServer(url))
    {
        FOTA_UPGRADE_IND(UP_START,0,retValue);
        ret = FTP_FotaMain(contextId, url);
    }
    else
#endif

#ifdef __OCPU_FOTA_BY_HTTP__
    if(HTTP_IsHttpServer(url))
    {
        FOTA_UPGRADE_IND(UP_START,0,retValue);
        ret = HTTP_FotaMain(contextId, url);
    }
    else
#endif
    {
        UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<-- The head of the URL string is incorrect or didn't define FOTA in custom_feature_def.h. -->\r\n");
        FOTA_DBG_PRINT("<-- The head of the URL string is incorrect or didn't define FOTA in custom_feature_def.h. -->\r\n");

		return -1;
    }

    return ret;
}

s32 Ql_FOTA_StopUpgrade(void)
{
    if (UP_CONNECTING == g_FOTA_State ||
        UP_CONNECTED == g_FOTA_State  ||
        UP_GETTING_FILE == g_FOTA_State  ||
        UP_GET_FILE_OK == g_FOTA_State
        )
    {
        // Stop upgrading...
    }
    return QL_RET_OK;
}

/*****************************************************************
* Function:     Fota_Upgrade_States
*
* Description:
*               This function is callback function.custom can write it by yourself. and if you want use the callback,  the UP_Callbcak pointer must point to it. if don.t need it  UP_Callbcak =NULL.
* NOtes:
*           the callback function has a return value. when in the UP_SYSTEM_REBOOT state, that means the Firmware is prepare ok, do you want reboot the system immediately.
*           if in the 'UP_SYSTEM_REBOOT' case, the callbcak return TRUE, the system will reboot ,and upgrade the firmware.
*           if in the 'UP_SYSTEM_REBOOT' case, the callback return FALSE, the system not reboot. 
*               and you must invoke the Ql_FOTA_Update(void) function before you want to reboot the system for the last step in the whole fota upgrade. 
*           
* Parameters:
*                state:
*                   the upgrade states, you can refer the Upgrade_State enum
*               fileDLPercent:
*                   the percent of the file download..
* Return:
*****************************************************************/    
static bool Fota_Upgrade_States(Upgrade_State state, s32 fileDLPercent)
{
    switch(state)
    {
        case UP_START:
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<-- Fota start to Upgrade -->\r\n");
            FOTA_DBG_PRINT("<-- Fota start to Upgrade -->\r\n");
            break;
        case UP_FOTAINITFAIL:
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<-- Fota Init failed!! -->\r\n");
            FOTA_DBG_PRINT("<-- Fota Init failed!! -->\r\n");
            break;
        case UP_CONNECTING:
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<-- connecting to the server-->\r\n");
            FOTA_DBG_PRINT("<-- connecting to the server-->\r\n");
            break; 
        case UP_CONNECTED:
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<-- conneced to the server now -->\r\n");
            FOTA_DBG_PRINT("<-- conneced to the server now -->\r\n");
            break; 
        case UP_GETTING_FILE:
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<-- getting the bin file (%d) -->\r\n", fileDLPercent);
            FOTA_DBG_PRINT("<-- getting the bin file  -->\r\n");
            break;     
        case UP_GET_FILE_OK:
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--file down OK (%d) -->\r\n", fileDLPercent);
            FOTA_DBG_PRINT("<--file down OK -->\r\n");
            break;  
        case UP_UPGRADFAILED:
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Fota upgrade failed !!!! -->\r\n");
            FOTA_DBG_PRINT("<--Fota upgrade failed !!!! -->\r\n");
            break;   


        case UP_SYSTEM_REBOOT: // If fota upgrade is in this case, this function you can  return false or true, Notes:  
        {
            // this case is important. return TRUE or FALSE, you can design by youself.
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Return TRUE, system will reboot, and upgrade  -->\r\n");
            UPGRADE_APP_DEBUG(FOTA_DBGBuffer,"<--Return FLASE, you must invoke Ql_FOTA_Update()  for upgrade !!!-->\r\n");
            FOTA_DBG_PRINT("<--Return TRUE, system will reboot, and upgrade  -->\r\n");
            FOTA_DBG_PRINT("<--Return FLASE, you must invoke Ql_FOTA_Update()  for upgrade !!!-->\r\n");
            return TRUE;// if return TRUE  the module will reboot ,and fota upgrade complete.
            //return FALSE; // if return False,  you must invoke Ql_FOTA_Update()  function before you want to reboot the system.
        }
        default:
            break;
    }
    return TRUE;
}

#endif  //__OCPU_FOTA_APP__
