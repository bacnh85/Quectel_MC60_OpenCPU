# Quectel_MC60_OpenCPU
Quectel_MC60_OpenCPU documents and example. 

In order to build application faster, I made some modification to create diffirent header/souces for each purpose (SMS, GPRS, GPS, .. ) based on SDK example, so you can customize to fit best into your application. 

## Prepare

### Install tools

Download GCC compiler, SDK, documents from official Quectel links: https://www.quectel.com/ProductDownload/MC60_OpenCPU_SDK.html

In order to install GCC in Windows 10, pls install as Administrator and with Win7 compability mode. 

### Flash based firmware

Flash firmware *MC60CAR01A10* which is coresponed with provided SDK V1.6.

## Usage

Clone this repo to overwrite *custom* folder inside SDK.

Next step, I will create independent folder with SDK folder and create needed build script for your application. 

Then, build the app as usual: 
```
make clean
make new
```


## Feature

### UART Module

UART module is initialized in the main task and used as bridge between UART1 and base band core, so you can type AT command and get response from UART1.

### GPRS Module

GPRS_TCP_Program() is called in main task once device connected to network, to create PDP context and socket ID to connect to server. 

There are callback functions which are very useful to handle GPRS connections:
- Callback_GPRS_Deactived
- Callback_Socket_Close
- Callback_Socket_Read
- Callback_Socket_Write

@todo: improve API to handle multiple socket connections. 

### SMS Module

SMS_Initialize() is called in main task after SMS module is ready. 

Hdlr_RecvNewSMS() is called to handle new received SMS message once URC_NEW_SMS_IND is received in MainTask. 

In side sms.c there are needed SMS API to handle SMS messages. 
