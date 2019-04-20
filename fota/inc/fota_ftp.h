#ifndef __FOTA_FTP_H__
#define __FOTA_FTP_H__

#include "ql_type.h"
#include "ql_stdlib.h"

typedef enum tagATCmdType {
    FTP_ATQIFGCNT,
    FTP_ATQICSGP,
    FTP_ATQFTPOPEN,
    FTP_ATQFTPCFG,
    FTP_ATQFTPPATH,
    FTP_ATQFTPGET,
    FTP_ATQFTPCLOSE,
    FTP_FOTAUPGRADE,
}AT_FTPCmdType;

typedef struct {
    s32  id;
    u32  interval;
    bool autoRepeat;
    bool runState;
}ST_FTPTimer;

#define UP_DATA_BUFFER_LEN 512
#define FTP_SERVERADD_LEN 40
#define FTP_FILEPATH_LEN 60
#define FTP_USERNAME_LEN 20
#define FTP_PASSWORD_LEN 20
#define FTP_BINFILENAME_LEN 25
#define FTP_SERVICE_PORT 21

#define APP_BINFILE_PATH   "RAM"

bool FTP_IsFtpServer(u8* URL);
s32  FTP_FotaMain(u8 contextId, u8* URL);


#endif

