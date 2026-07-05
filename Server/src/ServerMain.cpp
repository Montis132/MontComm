#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#include "UtilsModuleCommon.h"
#include "SCShare.h"
#include "ServerWorker.h"

using namespace std;
using json = nlohmann::json;

#define SERVER_CONF_FILE_PATH                        "ServerConf.json"
#define SERVER_ROLE_NAME                             "Server"

static ServerWorker *sg_ServerWorkers = NULL;
static CommMngrClient *sg_MngrClient = NULL;
static int sg_ServerWorkerTotalNum = 0;

static int 
S_CmdGetAllClientMap(char* InBuff, size_t InBuffLen, char* OutBuff, size_t OutBuffLen) {
    size_t offset = 0;
    UNUSED(InBuff);
    UNUSED(InBuffLen);

    if (!sg_ServerWorkers || !sg_ServerWorkerTotalNum) {
        return SUCCESS;
    }

    for (int loop = 0; loop < sg_ServerWorkerTotalNum; loop ++) {
        offset += snprintf(OutBuff + offset, OutBuffLen - offset, "%s", sg_ServerWorkers[loop].GetStatus().c_str());
    }

    return SUCCESS;
}

extern "C" 
void 
S_MainExit(
    void
    )
{
    string cmd;
    if (sg_ServerWorkers) {
        for (sg_ServerWorkerTotalNum -= 1; sg_ServerWorkerTotalNum >= 0; sg_ServerWorkerTotalNum --) {
            sg_ServerWorkers[sg_ServerWorkerTotalNum].Exit();
        }
    }
    if (sg_MngrClient) {
        sg_MngrClient->Exit();
    }
    Util_ModuleCommonExit();
    ThirdPartyExit();
    cmd += string("killall ") + string(SERVER_ROLE_NAME);
    system(cmd.c_str());
}

static ERR_T _S_MainPreRegisterUtil(void) {
    ERR_T ret = SUCCESS;
    UTIL_CMD_EXTERNAL_CONT cmdLineCont;
    // register out special cmd line
    // show status
    memset(&cmdLineCont, 0 ,sizeof(cmdLineCont));
    cmdLineCont.Argc = 2;
    (void)snprintf(cmdLineCont.Opt, sizeof(cmdLineCont.Opt), "GetAllClientMap");
    (void)snprintf(cmdLineCont.Help, sizeof(cmdLineCont.Help), "Get server worker stats(ClientMap)");
    cmdLineCont.Cb = S_CmdGetAllClientMap;
    ret = Util_CmdExternalRegister(cmdLineCont);
    if (ret < SUCCESS) {
        LogErr("Register cmdline failed! ret %d", ret);
        return ret;
    }

    return ret;
}

static ERR_T _S_MainInit(int Argc, char* Argv[]) {
    ERR_T ret = SUCCESS;
    string confFilePath(SERVER_CONF_FILE_PATH);
    ifstream file(confFilePath);
    UTIL_MODULES_INIT_PARAM initParam;
    string RoleName(SERVER_ROLE_NAME);
    S_WORKER_INIT_PARAM workerInitParam;
    COMM_MNGR_CLIENT_INIT_PARAM mngrClientConf;
    json fileJson;

    ret = ThirdPartyInit();
    if (ret < SUCCESS) {
        LogErr("Init third party conf failed! ret %d", ret);
        goto CommErr;
    }

    if (!file.is_open()) {
        LogErr("Cannot open config file!");
        ret = -ENOENT;
        goto CommErr;
    }
    try {
        file >> fileJson;
    } catch (json::parse_error& e) {
        ret = -EIO;
        LogErr("Parse json from %s failed!", confFilePath.c_str());
        goto CommErr;
    }
    // init param
    ret = ParseConfFromJson(initParam, confFilePath, RoleName, Argc, Argv, S_MainExit);
    if (ret < SUCCESS) {
        LogErr("Init param from conf failed! ret %d", ret);
        goto CommErr;
    }
    // pre register in utils
    ret = _S_MainPreRegisterUtil();
    if (ret < SUCCESS) {
        LogErr("Register in util failed! ret %d", ret);
        goto CommErr;
    }
    // init utils modules
    ret = Util_ModuleCommonInit(initParam);
    if (ret != SUCCESS) {
        if (ret != -ERR_EXIT_WITH_SUCCESS) {
            LogErr("Init utils modules failed! ret %d", ret);
            goto CommErr;
        }else {
            goto Success;
        }
    }
    // init mngr client
    try {
        sg_MngrClient = new CommMngrClient;
        mngrClientConf.ServerAddr = fileJson["MngrClientConf"]["ServerAddr"];
        if (fileJson["MngrClientConf"].contains("TrustCert") && 
            fileJson["MngrClientConf"]["TrustCert"].is_string() &&
            !fileJson["MngrClientConf"]["TrustCert"].get<std::string>().empty()) {
            mngrClientConf.TrustedCert = fileJson["MngrClientConf"]["TrustCert"];
        }
            
        ret = sg_MngrClient->Init(mngrClientConf);
        if (ret < SUCCESS) {
            LogErr("Init mngr client failed! ret %d", ret);
            goto CommErr;
        }
    } catch (...){
        ret = -EIO;
        LogErr("Init mngr client failed!");
        goto CommErr;
    }
    // init server workers
    try {
        if (fileJson.find("ServerWorkerConf") != fileJson.end()) {
            json serverWorkerArray = fileJson["ServerWorkerConf"];
            if (serverWorkerArray.size() <= 0) {
                throw std::runtime_error("no array");
            }
            sg_ServerWorkers = new ServerWorker[serverWorkerArray.size()];
            for (const auto& serverWorker : serverWorkerArray) {
                workerInitParam.Port = serverWorker["Port"];
                workerInitParam.Load = serverWorker["Load"];
                workerInitParam.WorkPath = serverWorker["WorkPath"];
                workerInitParam.Sm2PriKeyPwd = serverWorker["Sm2PriPwd"];
                sg_ServerWorkers[sg_ServerWorkerTotalNum].MngrClient = sg_MngrClient;
                ret = sg_ServerWorkers[sg_ServerWorkerTotalNum].Init(workerInitParam);
                if (ret != SUCCESS) {
                    LogWarn("Init server worker failed! ret %d", ret);
                }
                sg_ServerWorkerTotalNum ++;
            }
        } else {
            LogErr("Array 'ServerWorkerConf' not found in JSON");
            return -EIO;
        }
    } catch (...){
        ret = -EIO;
        LogErr("Init server failed!");
        goto CommErr;
    }

    goto Success;

CommErr:
    S_MainExit();
Success:
    if (file.is_open())
        file.close();
    return ret;
}

static void
_S_MainLoop(
    void
    )
{
    while(1) {
        sleep(1000);
    }
}

int main(
    int argc,
    char* argv[]
    )
{
    ERR_T ret = SUCCESS;

    ret = _S_MainInit(argc, argv);
    if (ret != SUCCESS) {
        return ret;
    }

    _S_MainLoop();

    return ret;
}
