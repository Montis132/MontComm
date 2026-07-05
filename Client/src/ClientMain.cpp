#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#include "UtilsModuleCommon.h"
#include "SCShare.h"
#include "SCMsg.pb.h"
#include "ClientWorker.h"

using namespace std;
using json = nlohmann::json;

#define CLIENT_CONF_FILE_PATH                        "ClientConf.json"
#define CLIENT_RULE_NAME                             "Client"

static ClientWorker sg_ClientWorker[1];

static void _C_MainExit(void) {
    for (size_t i=0; i<sizeof(sg_ClientWorker)/sizeof(ClientWorker); i++)
        sg_ClientWorker[i].Exit();
    Util_ModuleCommonExit();
}

static ERR_T _C_MainGetSpecailConfFromJson(
    json FileJson,
    C_WORKER_INIT_PARAM &InitParam
    ) 
{
    // init client workers
    if (FileJson.find("ServerConf") != FileJson.end()) {
        json serverArray = FileJson["ServerConf"];
        for (const auto& server : serverArray) {
            C_WORKER_SERVER_CONF serverConf;
            serverConf.Addr = server["Addr"];
            InitParam.Servers.push_back(serverConf);
        }
    } else {
        LogErr("Array 'ServerConf' not found in JSON");
        return -EIO;
    }
    if (!FileJson.contains("ClientConf") || FileJson["ClientConf"].is_null()) {
        LogErr("Parse ClientConf from conf failed!");
        return -EIO;
    }
    if (!FileJson["ClientConf"].contains("Id") || FileJson["ClientConf"]["Id"].is_null()) {
        LogErr("Parse Id from ClientConf failed!");
        return -EIO;
    }
    InitParam.ClientId = FileJson["ClientConf"]["Id"];
    if (!FileJson["ClientConf"].contains("Sm2PriPwd") || FileJson["ClientConf"]["Sm2PriPwd"].is_null()) {
        LogErr("Parse Sm2PriPwd from ClientConf failed!");
        return -EIO;
    }
    InitParam.PriKeyPwd = FileJson["ClientConf"]["Sm2PriPwd"];
    if (!FileJson["ClientConf"].contains("WorkPath") || FileJson["ClientConf"]["WorkPath"].is_null()) {
        LogErr("Parse WorkPath from ClientConf failed!");
        return -EIO;
    }
    InitParam.WorkPath = FileJson["ClientConf"]["WorkPath"];
    if (FileJson["ClientConf"].contains("CryptoSuite") && !FileJson["ClientConf"]["CryptoSuite"].is_null() && 
        FileJson["ClientConf"]["CryptoSuite"] == "SM4") {
        InitParam.CryptoSuite = SCMsg::SC_CIPHER_SUITE_SM4;
    } else {
        InitParam.CryptoSuite = SCMsg::SC_CIPHER_SUITE_NONE;
    }
    return SUCCESS;
}

static ERR_T _C_MainInit(int Argc, char* Argv[]) {
    ERR_T ret = SUCCESS;
    string roleName(CLIENT_RULE_NAME);
    string confPath(CLIENT_CONF_FILE_PATH);
    ifstream file(confPath);
    UTIL_MODULES_INIT_PARAM initParam;
    json fileJson;
    C_WORKER_INIT_PARAM clientWorkerParam;
    size_t loop = 0;
    
    if (!file.is_open()) {
        LogErr("Cannot open config file!");
        ret = -ENOENT;
        goto FileOpenErr;
    }
    try {
        file >> fileJson;
    } catch (json::parse_error& e) {
        ret = -EIO;
        LogErr("Parse json from %s failed!", confPath.c_str());
        goto CommRet;
    }
    // init param
    ret = ParseConfFromJson(initParam, confPath, roleName, Argc, Argv, _C_MainExit);
    if (ret < SUCCESS) {
        LogErr("Init param from conf failed! ret %d", ret);
        goto CommRet;
    }
    // init utils modules
    ret = Util_ModuleCommonInit(initParam);
    if (ret < SUCCESS) {
        if (ret != -ERR_EXIT_WITH_SUCCESS)
            LogErr("Init utils modules failed! ret %d", ret);
        goto CommRet;
    }
    // get client init param
    ret = _C_MainGetSpecailConfFromJson(fileJson, clientWorkerParam);
    if (ret < SUCCESS) {
        LogErr("Get client param from conf failed! ret %d", ret);
        goto CommRet;
    }
    // init client
    for (loop=0; loop<sizeof(sg_ClientWorker)/sizeof(ClientWorker); loop++) {
        usleep(500 * 1000);
        clientWorkerParam.ClientId = loop + 1;
        ret = sg_ClientWorker[loop].Init(clientWorkerParam);
        if (ret < SUCCESS) {
            LogErr("Init client%d failed! ret %d", loop, ret);
            continue;
        }
    }

CommRet:
    file.close();
FileOpenErr:
    return ret;
}
static void
_C_MainLoop(
    void
    )
{
    std::string sendMsg;
    ERR_T ret = SUCCESS;

    while(1) {
        std::cin >> sendMsg;
        ret = sg_ClientWorker->SendMsg(1, sendMsg);
        if (ret) {
            LogErr("Send msg failed! ret %d", ret);
        }
    }
}
int main(int argc, char* argv[]) {
    ERR_T ret = SUCCESS;

    ret = _C_MainInit(argc, argv);
    if (ret != SUCCESS) {
        return ret;
    }

    _C_MainLoop();

    _C_MainExit();
    return ret;
}
