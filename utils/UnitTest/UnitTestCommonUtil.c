#include "UnitTest.h"
#include "UtilsCommonUtil.h"

static int
_UT_CommonUtil_ByteOrderU64(
    void
    )
{
    int ret = SUCCESS;
    uint64_t before = 0x12345678, afterShouldBe = 0x78563412, after = 0;

    after = Util_htonll(before);
    if (after != afterShouldBe || Util_ntohll(after) != before)
    {
        ret = -EIO;
        goto CommonReturn;
    }
    
CommonReturn:
    return ret;
}

static int
_UT_CommonUtil_ParseIp(
    void
    )
{
    int ret = SUCCESS;
    char* ipStr = "192.168.137.101";
    char* ipStrWithPort = "192.168.137.101:443";
    uint32_t ip = 0;
    uint16_t port = 0;

    ret = Util_ParseStringToIpv4(ipStr, strlen(ipStr), &ip);
    if (ret < SUCCESS || ip != 3232270693)
    {
        ret = -EIO;
        goto CommonReturn;
    }
    ret = Util_ParseStringToIpv4AndPort(ipStrWithPort, strlen(ipStrWithPort), &ip, &port);
    if (ret < SUCCESS || ip != 3232270693 || port != 443)
    {
        ret = -EIO;
        goto CommonReturn;
    }
    
CommonReturn:
    return ret;
}
int main()
{
    assert(SUCCESS == _UT_CommonUtil_ByteOrderU64());

    return 0;
}
