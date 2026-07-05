#include <iostream>
#include <fstream>
#include <string>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <nlohmann/json.hpp>

#include "UtilsModuleCommon.h"

using namespace std;
using json = nlohmann::json;

ERR_T ParseConfFromJson(UTIL_MODULES_INIT_PARAM &InitParam, string ConfFilePath, string RoleName, int Argc, char **Argv, ExitHandle ExitHandle) {
    ERR_T ret = SUCCESS;
    ifstream file(ConfFilePath);
    json fileJson;
    
    // load fileJson
    if (!file.is_open()) {
        LogErr("Cannot open config file!");
        ret = -ENOENT;
        goto FileOpenErr;
    }
    try {
        file >> fileJson;
    } catch (json::parse_error& e) {
        ret = -EIO;
        LogErr("Parse json from %s failed!", ConfFilePath.c_str());
        goto CommErr;
    }
    if (!fileJson.contains("UtilConf") || fileJson["UtilConf"].is_null()) {
        ret = -EIO;
        LogErr("Parse utilconf from %s failed!", ConfFilePath.c_str());
        goto CommErr;
    }
    memset(&InitParam, 0, sizeof(InitParam));
    // init utils modules params
    try {
        if (fileJson["UtilConf"].contains("CmdLineInit") && fileJson["UtilConf"]["CmdLineInit"]) {
            InitParam.CmdLineArg = new UTIL_CMDLINE_MODULE_INIT_ARG;
            memset(InitParam.CmdLineArg, 0, sizeof(UTIL_CMDLINE_MODULE_INIT_ARG));
        }
        if (fileJson["UtilConf"].contains("LogConf") && !fileJson["UtilConf"]["LogConf"].is_null() && fileJson["UtilConf"]["LogConf"]["Init"]) {
            InitParam.LogArg = new UTIL_LOG_MODULE_INIT_ARG;
            memset(InitParam.LogArg, 0, sizeof(UTIL_LOG_MODULE_INIT_ARG));
        }
        if (fileJson["UtilConf"].contains("TPoolConf") && !fileJson["UtilConf"]["TPoolConf"].is_null() && fileJson["UtilConf"]["TPoolConf"]["Init"]) {
            InitParam.TPoolArg = new UTIL_TPOOL_MODULE_INIT_ARG;
            memset(InitParam.TPoolArg, 0, sizeof(UTIL_TPOOL_MODULE_INIT_ARG));
        }
        if (fileJson["UtilConf"].contains("HealthCheckConf") && !fileJson["UtilConf"]["HealthCheckConf"].is_null() && fileJson["UtilConf"]["HealthCheckConf"]["Init"]) {
            InitParam.HealthArg = new UTIL_HEALTH_MODULE_INIT_ARG;
            memset(InitParam.HealthArg, 0, sizeof(UTIL_HEALTH_MODULE_INIT_ARG));
        }
        if (fileJson["UtilConf"].contains("MsgModInit") && fileJson["UtilConf"]["MsgModInit"]) {
            InitParam.InitMsgModule = TRUE;
        }
        if (fileJson["UtilConf"].contains("TimerModInit") && fileJson["UtilConf"]["TimerModInit"]) {
            InitParam.InitTimerModule = TRUE;
        }
    } catch(const std::exception& e) {
        ret = -EIO;
        LogErr("New conf failed!");
        goto CommErr;
    }
    if (InitParam.CmdLineArg) {
        InitParam.CmdLineArg->Argc = Argc;
        InitParam.CmdLineArg->Argv = Argv;
        InitParam.CmdLineArg->ExitFunc = ExitHandle;
        copy(RoleName.begin(), RoleName.end(), InitParam.CmdLineArg->RoleName);
    }
    if (InitParam.LogArg) {
        auto& logConf = fileJson["UtilConf"]["LogConf"];
        try {
            if (logConf.contains("LogPath")) {
                string tmpPath = logConf["LogPath"];
                copy(tmpPath.begin(), tmpPath.end(), InitParam.LogArg->LogFilePath);
            }
            if (logConf.contains("LogLevel")) InitParam.LogArg->LogLevel = logConf["LogLevel"];
            if (logConf.contains("LogSize")) InitParam.LogArg->LogMaxSize = logConf["LogSize"];
            if (logConf.contains("LogMaxNum")) InitParam.LogArg->LogMaxNum = logConf["LogMaxNum"];
            copy(RoleName.begin(), RoleName.end(), InitParam.LogArg->RoleName);
        } catch (const std::exception& e){
            ret = -EIO;
            LogErr("Init Log conf failed!");
            goto CommErr;
        }
    }
    if (InitParam.TPoolArg) {
        auto& tPoolConf = fileJson["UtilConf"]["TPoolConf"];
        try {
            if (tPoolConf.contains("TPoolQueueMaxLen")) InitParam.TPoolArg->TaskListMaxLength = tPoolConf["TPoolQueueMaxLen"];
            if (tPoolConf.contains("TPoolSize")) InitParam.TPoolArg->ThreadPoolSize = tPoolConf["TPoolSize"];
            if (tPoolConf.contains("TPoolTimeout")) InitParam.TPoolArg->Timeout = tPoolConf["TPoolTimeout"];
        } catch (const std::exception& e){
            ret = -EIO;
            LogErr("Init TPool conf failed!");
            goto CommErr;
        }
    }
    if (InitParam.HealthArg) {
        auto& healthCheckConf = fileJson["UtilConf"]["HealthCheckConf"];
        try {
            if (healthCheckConf.contains("CmdLineHealthIntervalS")) InitParam.HealthArg->CmdLineHealthIntervalS = healthCheckConf["CmdLineHealthIntervalS"];
            if (healthCheckConf.contains("LogHealthIntervalS")) InitParam.HealthArg->LogHealthIntervalS = healthCheckConf["LogHealthIntervalS"];
            if (healthCheckConf.contains("MemHealthIntervalS")) InitParam.HealthArg->MemHealthIntervalS = healthCheckConf["MemHealthIntervalS"];
            if (healthCheckConf.contains("MHHealthIntervalS")) InitParam.HealthArg->MHHealthIntervalS = healthCheckConf["MHHealthIntervalS"];
            if (healthCheckConf.contains("MsgHealthIntervalS")) InitParam.HealthArg->MsgHealthIntervalS = healthCheckConf["MsgHealthIntervalS"];
            if (healthCheckConf.contains("TimerHealthIntervalS")) InitParam.HealthArg->TimerHealthIntervalS = healthCheckConf["TimerHealthIntervalS"];
            if (healthCheckConf.contains("TPoolHealthIntervalS")) InitParam.HealthArg->TPoolHealthIntervalS = healthCheckConf["TPoolHealthIntervalS"];
        } catch (const std::exception& e){
            ret = -EIO;
            LogErr("Init Health conf failed!");
            goto CommErr;
        }
    }

CommErr:
    file.close();
FileOpenErr:
    return ret;
}

ERR_T
ThirdPartyInit(
    void
    )
{
    ERR_T ret = SUCCESS;

    // ssl init
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    return ret;
}

void
ThirdPartyExit(
    void
    )
{
    // ssl exit
    ERR_free_strings();
    EVP_cleanup();
}

ERR_T 
SSLErrorShow(
    SSL* SSL,
    int Res
    )
{
    ERR_T ret = SUCCESS;

    int err = SSL_get_error(SSL, Res);
    switch (err) {
        case SSL_ERROR_ZERO_RETURN:
            ret = -ERR_PEER_CLOSED;
            break;
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            // try again
            ret = -EAGAIN;
            LogInfo("SSL/TLS send/recv neet to try again");
            break;
        case SSL_ERROR_NONE:
            // No error
            ret = SUCCESS;
            LogInfo("Not a error.");
            break;
        case SSL_ERROR_SSL:
            // General SSL/TLS error
            LogErr("SSL/TLS error: %s", ERR_error_string(ERR_get_error(), NULL));
            ret = -EIO;
            break;
        case SSL_ERROR_SYSCALL:
            // System call error
            LogErr("System call error: %s", strerror(errno));
            ret = -EINTR;
            break;
        default:
            ret = -EINVAL;
            // Handle other error codes as needed
            unsigned long error;
            while ((error = ERR_get_error()) != 0) {
                LogErr("OpenSSL error: %s", ERR_error_string(error, NULL));
            }
            break;
    }

    return ret;
}
