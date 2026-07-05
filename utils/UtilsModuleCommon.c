#include "UtilsModuleCommon.h"

static
void
_EventLogCallBack(
    int Severity,
    const char *Msg
    )
{
    if (Msg)
    {
        switch (Severity)
        {
            case _EVENT_LOG_DEBUG:
                /* Ignore massive debug logs*/
                break;
            case _EVENT_LOG_MSG:
                LogInfo("[LibEvent] %s\n", Msg);
                break;
            case _EVENT_LOG_WARN:
                LogWarn("[LibEvent] %s\n", Msg);
                break;
            case _EVENT_LOG_ERR:
                LogErr("[LibEvent] %s\n", Msg);
                break;
            default:
                LogErr("[LibEvent] %s\n", Msg);
                break; /* never reached */
        }
    }
}

static int 
_ModuleCommonThirdPartyInit(
    void
    )
{
    int ret = SUCCESS;
// event init 
    (void)event_enable_debug_logging(FALSE);
    (void)event_set_log_callback(_EventLogCallBack);

    return ret;
}

int
Util_ModuleCommonInit(
    UTIL_MODULES_INIT_PARAM ModuleInitParam 
    )
{
    int ret = SUCCESS;
    
    if (ModuleInitParam.LogArg)
    {
        ret = Util_LogModuleInit(ModuleInitParam.LogArg);
        if (ret)
        {
            LogErr("Init log failed! %d %s", ret, StrErr(ret));
            goto CommonReturn;
        }
        //LogInfo("Log start...");
    }
    
    ret = Util_MemModuleInit();
    if (ret)
    {
        LogErr("Mem module init failed! %d %s", ret, StrErr(ret));
        goto CommonReturn;
    }

    ret = _ModuleCommonThirdPartyInit();
    if (ret)
    {
        LogErr("Third party init failed! %d %s", ret, StrErr(ret));
        goto CommonReturn;
    }

    if (ModuleInitParam.CmdLineArg)
    {
        ret = Util_CmdLineModuleInit(ModuleInitParam.CmdLineArg);
        if (ret)
        {
            if (-ERR_EXIT_WITH_SUCCESS != ret)
            {
                LogErr("Cmd line init failed! %d %s", ret, StrErr(ret));
            }
            goto CommonReturn;
        }
    }
    LogInfo("");
    LogInfo("-----------------------------------------------------------");
    LogInfo("|--------------------%12s ------------------------|", ModuleInitParam.LogArg->RoleName);
    LogInfo("-----------------------------------------------------------");
    
    if (ModuleInitParam.InitMsgModule)
    {
        ret = Util_MsgModuleInit();
        if (ret)
        {
            LogErr("Msg module init failed! %d %s", ret, StrErr(ret));
            goto CommonReturn;
        }
        LogInfo("---------------- Msg Module init success -----------------");
    }

    if (ModuleInitParam.TPoolArg)
    {
        ret = Util_TPoolModuleInit(ModuleInitParam.TPoolArg);
        if (ret)
        {
            LogErr("Init TPool failed! %d %s", ret, StrErr(ret));
            goto CommonReturn;
        }
        LogInfo("---------------- TPool Module init success ---------------");
    }

    if (ModuleInitParam.HealthArg)
    {
        ret = Util_HealthModuleInit(ModuleInitParam.HealthArg);
        if (ret)
        {
            LogErr("Init HealthModule failed! %d %s", ret, StrErr(ret));
            goto CommonReturn;
        }
        LogInfo("---------------- Health Module init success --------------");
    }

    if (ModuleInitParam.InitTimerModule)
    {
        ret = Util_TimerModuleInit();
        if (ret)
        {
            LogErr("Init Timer module failed! %d %s", ret, StrErr(ret));
            goto CommonReturn;
        }
        LogInfo("---------------- Timer Module init success ---------------");
    }

CommonReturn:
    return ret;
}

void
Util_ModuleCommonExit(
    void
    )
{
    Util_CmdLineModuleExit();

    (void)Util_MsgModuleExit();
    LogInfo("-------------------- Msg Module exit ---------------------");

    (void)Util_TPoolModuleExit();
    LogInfo("-------------------- TPool Module exit -------------------");

    (void)Util_HealthModuleExit();
    LogInfo("-------------------- Health Module exit ------------------");

    (void)Util_TimerModuleExit();
    LogInfo("-------------------- Timer Module exit -------------------");

    (void)Util_MemModuleExit();
    LogInfo("-------------------- Mem Module exit ---------------------");
    
    LogInfo("----------------------------------------------------------");
    Util_LogModuleExit();
}
