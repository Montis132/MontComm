#include "UnitTest.h"
#include "UtilsMem.h"

#define UT_MEM_REGISTER_NAME                        "UT_MEM"

static int 
_UT_Mem_ForwardT(
    void
    )
{
    int ret = SUCCESS;
    int memId = UTIL_MEM_MODULE_INVALID_ID;
    void* ptr = NULL;

    ret = Util_MemModuleInit();
    if (ret)
    {
        UTLog("Init fail\n");
        goto CommonReturn;
    }

    ret = Util_MemRegister(&memId, (char*)UT_MEM_REGISTER_NAME);
    if (ret)
    {
        UTLog("Register fail\n");
        goto CommonReturn;
    }

    ptr = Util_MemCalloc(memId, sizeof(int));
    if (!ptr)
    {
        ret = -ENOMEM;
        UTLog("calloc fail\n");
        goto CommonReturn;
    }
    
    if (ptr)
    {
        Util_MemFree(memId, ptr);
    }
    if (!Util_MemLeakSafetyCheckWithId(memId))
    {
        ret = -EIO;
        UTLog("mem leak check fail\n");
        goto CommonReturn;
    }
    ret = Util_MemUnRegister(&memId);
CommonReturn:
    if (Util_MemModuleExit())
    {
        ret = -EIO;
    }
    return ret;
}

int main()
{
    assert(SUCCESS == _UT_Mem_ForwardT());

    return 0;
}

