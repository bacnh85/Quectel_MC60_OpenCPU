#ifdef __CUSTOMER_CODE__

#include "main.h"
#include "gprs.h"

/************************************************************************/
/* Definition for GPRS PDP context                                      */
/************************************************************************/
static ST_GprsConfig m_GprsConfig = {
    "v-internet",    // APN name
    "",         // User name for APN
    "",         // Password for APN
    0,
    NULL,
    NULL,
};

/************************************************************************/
/* Definition for Server IP Address and Socket Port Number              */
/************************************************************************/
static u8  m_SrvADDR[20] = "14.232.38.62\0";
static u32 m_SrvPort = 2001;

#define  SOC_RECV_BUFFER_LEN  1460
static s32 m_GprsActState    = 0;   // GPRS PDP activation state, 0= not activated, 1=activated
static s32 m_SocketId        = -1;  // Store socket Id that returned by Ql_SOC_Create()
static s32 m_SocketConnState = 0;   // Socket connection state, 0= disconnected, 1=connected
static u8  m_SocketRcvBuf[SOC_RECV_BUFFER_LEN];


/************************************************************************/
/* Declarations for GPRS and TCP socket callback                        */
/************************************************************************/
//
// This callback function is invoked when GPRS drops down.
static void Callback_GPRS_Deactived(u8 contextId, s32 errCode, void* customParam );
//
// This callback function is invoked when the socket connection is disconnected by server or network.
static void Callback_Socket_Close(s32 socketId, s32 errCode, void* customParam );
//
// This callback function is invoked when socket data arrives.
static void Callback_Socket_Read(s32 socketId, s32 errCode, void* customParam );
//
// This callback function is invoked in the following case:
// The return value is less than the data length to send when calling Ql_SOC_Send(), which indicates
// the socket buffer is full. Application should stop sending socket data till this callback function
// is invoked, which indicates application can continue to send data to socket.
static void Callback_Socket_Write(s32 socketId, s32 errCode, void* customParam );

void timer_Initialize(void);
int GPRS_SendData(char* data);

//
//
// Activate GPRS and program TCP.
//
void GPRS_TCP_Program(void)
{
    s32 ret;
    s32 pdpCntxtId;
    ST_PDPContxt_Callback callback_gprs_func = {
        NULL,
        Callback_GPRS_Deactived
    };
    ST_SOC_Callback callback_soc_func = {
        NULL,
        Callback_Socket_Close,
        NULL,
        Callback_Socket_Read,    
        Callback_Socket_Write
    };

    //1. Register GPRS callback
    pdpCntxtId = Ql_GPRS_GetPDPContextId();
    if (GPRS_PDP_ERROR == pdpCntxtId)
    {
        APP_DEBUG("No PDP context is available\r\n");
        return ret;
    }
    ret = Ql_GPRS_Register(pdpCntxtId, &callback_gprs_func, NULL);
    if (GPRS_PDP_SUCCESS == ret)
    {
        APP_DEBUG("<-- Register GPRS callback function -->\r\n");
    }else{
        APP_DEBUG("<-- Fail to register GPRS, cause=%d. -->\r\n", ret);
        return;
    }

    //2. Configure PDP
    ret = Ql_GPRS_Config(pdpCntxtId, &m_GprsConfig);
    if (GPRS_PDP_SUCCESS == ret)
    {
        APP_DEBUG("<-- Configure PDP context -->\r\n");
    }else{
        APP_DEBUG("<-- Fail to configure GPRS PDP, cause=%d. -->\r\n", ret);
        return;
    }

    //3. Activate GPRS PDP context
    APP_DEBUG("<-- Activating GPRS... -->\r\n");
    ret = Ql_GPRS_ActivateEx(pdpCntxtId, TRUE);
    if (ret == GPRS_PDP_SUCCESS)
    {
        m_GprsActState = 1;
        APP_DEBUG("<-- Activate GPRS successfully. -->\r\n\r\n");
    }else{
        APP_DEBUG("<-- Fail to activate GPRS, cause=%d. -->\r\n\r\n", ret);
        return;
    }

    //4. Register Socket callback
    ret = Ql_SOC_Register(callback_soc_func, NULL);
    if (SOC_SUCCESS == ret)
    {
        APP_DEBUG("<-- Register socket callback function -->\r\n");
    }else{
        APP_DEBUG("<-- Fail to register socket callback, cause=%d. -->\r\n", ret);
        return;
    }

    //5. Create socket
    m_SocketId = Ql_SOC_Create(pdpCntxtId, SOC_TYPE_TCP);
    if (m_SocketId >= 0)
    {
        APP_DEBUG("<-- Create socket successfully, socket id=%d. -->\r\n", m_SocketId);
    }else{
        APP_DEBUG("<-- Fail to create socket, cause=%d. -->\r\n", m_SocketId);
        return;
    }		

    //6. Connect to server
    {
        //6.1 Convert IP format
        u8 m_ipAddress[4]; 
        Ql_memset(m_ipAddress,0,5);
        ret = Ql_IpHelper_ConvertIpAddr(m_SrvADDR, (u32 *)m_ipAddress);
        if (SOC_SUCCESS == ret) // ip address is xxx.xxx.xxx.xxx
        {
            APP_DEBUG("<-- Convert Ip Address successfully,m_ipaddress=%d,%d,%d,%d -->\r\n",m_ipAddress[0],m_ipAddress[1],m_ipAddress[2],m_ipAddress[3]);
        }else{
            APP_DEBUG("<-- Fail to convert IP Address --> \r\n");
            return;
        }

        //6.2 Connect to server
        APP_DEBUG("<-- Connecting to server(IP:%d.%d.%d.%d, port:%d)... -->\r\n", m_ipAddress[0],m_ipAddress[1],m_ipAddress[2],m_ipAddress[3], m_SrvPort);
        ret = Ql_SOC_ConnectEx(m_SocketId,(u32) m_ipAddress, m_SrvPort, TRUE);
        if (SOC_SUCCESS == ret)
        {
            m_SocketConnState = 1;
            APP_DEBUG("<-- Connect to server successfully -->\r\n");
        }else{
            APP_DEBUG("<-- Fail to connect to server, cause=%d -->\r\n", ret);
            APP_DEBUG("<-- Close socket.-->\r\n");
            Ql_SOC_Close(m_SocketId);
            m_SocketId = -1;
            return;
        }
    }

    //7. Send data to socket
    ret = GPRS_SendData("A B C D E F G");

    // Enable Timer
    timer_Initialize();

/*
    //8. Close socket
    ret = Ql_SOC_Close(m_SocketId);
    APP_DEBUG("<-- Close socket[%d], cause=%d --> \r\n", m_SocketId, ret);

    //9. Deactivate GPRS
    APP_DEBUG("<-- Deactivating GPRS... -->\r\n");
    ret = Ql_GPRS_DeactivateEx(pdpCntxtId, TRUE);
    APP_DEBUG("<-- Deactivated GPRS, cause=%d -->\r\n\r\n", ret);
*/
    APP_DEBUG("< Finish >\r\n");
}

int GPRS_SendData(char* data)
{
    s32  dataLen = 0;
    u64  ackNum = 0;
    s32 ret;
    s64 old_ack;

    ret = Ql_SOC_GetAckNumber(m_SocketId, &ackNum);
    old_ack = ackNum;

    dataLen = strlen(data);
    APP_DEBUG("<-- Sending data(len=%d): %s-->\r\n", dataLen, data);

    ret = Ql_SOC_Send(m_SocketId, (u8*)data, dataLen);
    if (ret == dataLen)
    {
        APP_DEBUG("<-- Send socket data successfully. --> \r\n");
    }else{
        APP_DEBUG("<-- Fail to send socket data. --> \r\n");
        Ql_SOC_Close(m_SocketId);
        return -1;
    }

    //7.2 Check ACK number
    do 
    {
        ret = Ql_SOC_GetAckNumber(m_SocketId, &ackNum);
        APP_DEBUG("<-- Current ACK Number:%llu/%d --> \r\n", ackNum, dataLen);
        Ql_Sleep(500);
    } while (ackNum - old_ack != dataLen);

    APP_DEBUG("<-- Server has received all data --> \r\n");
    return 0;
}
//
//
// This callback function is invoked when GPRS drops down. The cause is in "errCode".
//
static void Callback_GPRS_Deactived(u8 contextId, s32 errCode, void* customParam )
{
    if (errCode == SOC_SUCCESS)
    {
        APP_DEBUG("<--CallBack: deactivated GPRS successfully.-->\r\n"); 
    }else{
        APP_DEBUG("<--CallBack: fail to deactivate GPRS, cause=%d)-->\r\n", errCode); 
    }
    if (1 == m_GprsActState)
    {
        m_GprsActState = 0;
        APP_DEBUG("<-- GPRS drops down -->\r\n"); 
    }
}
//
//
// This callback function is invoked when the socket connection is disconnected by server or network.
//
static void Callback_Socket_Close(s32 socketId, s32 errCode, void* customParam )
{
    if (errCode == SOC_SUCCESS)
    {
        APP_DEBUG("<--CallBack: close socket successfully.-->\r\n"); 
    }
    else if(errCode == SOC_BEARER_FAIL)
    {   
        APP_DEBUG("<--CallBack: fail to close socket,(socketId=%d,error_cause=%d)-->\r\n", socketId, errCode); 
    }else{
        APP_DEBUG("<--CallBack: close socket failure,(socketId=%d,error_cause=%d)-->\r\n", socketId, errCode); 
    }
    if (1 == m_SocketConnState)
    {
        APP_DEBUG("<-- Socket connection is disconnected -->\r\n"); 
        APP_DEBUG("<-- Close socket at module side -->\r\n"); 
        Ql_SOC_Close(socketId);
        m_SocketConnState = 0;
    }
}
//
//
// This callback function is invoked when socket data arrives.
// The program should call Ql_SOC_Recv to read all data out of the socket buffer.
//
static void Callback_Socket_Read(s32 socketId, s32 errCode, void* customParam )
{
    s32 ret;
    s32 offset = 0;
    if (errCode)
    {
        APP_DEBUG("<-- Close socket -->\r\n");
        Ql_SOC_Close(socketId);
        m_SocketId = -1;
        return;
    }

    Ql_memset(m_SocketRcvBuf, 0, SOC_RECV_BUFFER_LEN);
    do
    {
        ret = Ql_SOC_Recv(socketId, m_SocketRcvBuf + offset, SOC_RECV_BUFFER_LEN - offset);
        if((ret < SOC_SUCCESS) && (ret != SOC_WOULDBLOCK))
        {
            APP_DEBUG("<-- Fail to receive data, cause=%d.-->\r\n",ret);
            APP_DEBUG("<-- Close socket.-->\r\n");
            Ql_SOC_Close(socketId);
            m_SocketId = -1;
            break;
        }
        else if(SOC_WOULDBLOCK == ret)  // Read finish
        {
            APP_DEBUG("<-- Receive data from server,len(%d):%s\r\n", offset, m_SocketRcvBuf);
            break;
        }
        else // Continue to read...
        {
            if (SOC_RECV_BUFFER_LEN == offset)  // buffer if full
            {
                APP_DEBUG("<-- Receive data from server,len(%d):%s\r\n", offset, m_SocketRcvBuf);
                Ql_memset(m_SocketRcvBuf, 0, SOC_RECV_BUFFER_LEN);
                offset = 0;
            }else{
                offset += ret;
            }
            continue;
        }
    } while (TRUE);
}
//
//
// This callback function is invoked in the following case:
// The return value is less than the data length to send when calling Ql_SOC_Send(), which indicates
// the socket buffer is full. Application should stop sending socket data till this callback function
// is invoked, which indicates application can continue to send data to socket.
static void Callback_Socket_Write(s32 socketId, s32 errCode, void* customParam)
{
    if (errCode < 0)
    {
        APP_DEBUG("<-- Socket error(error code:%d), close socket.-->\r\n", errCode);
        Ql_SOC_Close(socketId);
        m_SocketId = -1;        
    }else{
        APP_DEBUG("<-- You can continue to send data to socket -->\r\n");
    }
}


// TIMER
#define TIMEOUT_COUNT 2
static u32 Stack_timer = 0x102; // timerId =99; timerID is Specified by customer, but must ensure the timer id is unique in the opencpu task
static u32 ST_Interval = 30000;
static s32 m_param1 = 0;

static void Timer_handler(u32 timerId, void* param);

void timer_Initialize(void)
{
    s32 ret;
 
    //register  a timer
    ret = Ql_Timer_Register(Stack_timer, Timer_handler, &m_param1);
    if(ret <0)
    {
        APP_DEBUG("\r\n<--failed!!, Ql_Timer_Register: timer(%d) fail ,ret = %d -->\r\n",Stack_timer,ret);
    }
    APP_DEBUG("\r\n<--Register: timerId=%d, param = %d,ret = %d -->\r\n", Stack_timer ,m_param1,ret); 

   
    //start a timer,repeat=true;
    ret = Ql_Timer_Start(Stack_timer,ST_Interval,TRUE);
    if(ret < 0)
    {
        APP_DEBUG("\r\n<--failed!! stack timer Ql_Timer_Start ret=%d-->\r\n",ret);        
    }
    APP_DEBUG("\r\n<--stack timer Ql_Timer_Start(ID=%d,Interval=%d,) ret=%d-->\r\n",Stack_timer,ST_Interval,ret);  
}

char tx_buffer[200];

// timer callback function
void Timer_handler(u32 timerId, void* param)
{
    *((s32*)param) +=1;
    if(Stack_timer == timerId)
    {
        APP_DEBUG("<-- stack Timer_handler, param:%d -->\r\n", *((s32*)param));

        Ql_sprintf(tx_buffer,"Data ID = %d ", *((s32*)param));        
        GPRS_SendData(tx_buffer);
    }   
}

#endif