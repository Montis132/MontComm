#ifndef __SC_SHARE_H_
#define __SC_SHARE_H_

#include <openssl/err.h>
#include <openssl/ssl.h>
#include "UtilsModuleCommon.h"
ERR_T ParseConfFromJson(UTIL_MODULES_INIT_PARAM &InitParam, std::string ConfFilePath, std::string RoleName, int Argc, char **Argv, ExitHandle ExitHandle) ;
ERR_T ThirdPartyInit(void);
void ThirdPartyExit(void);
ERR_T SSLErrorShow(SSL* SSL, int Res);
#endif /* __SC_SHARE_H_ */