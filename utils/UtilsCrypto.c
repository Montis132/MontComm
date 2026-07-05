#include "UtilsCrypto.h"

#include "gmssl/sm2.h"
#include "gmssl/sm3.h"
#include "gmssl/sm4.h"
#include "gmssl/rand.h"
#include "UtilsLogIO.h"

ERR_T
Util_CryptRandBytes(
    __out uint8_t *RandBuf,
    __inout size_t RandLen
    )
{
    ERR_T ret = SUCCESS;

    if (!RandBuf || !RandLen) {
        ret = -EINVAL;
        goto CommRet;
    }

    if (rand_bytes(RandBuf, RandLen) != 1) {
        ret = -EIO;
        goto CommRet;
    }

CommRet:
    return ret;
}

ERR_T
Util_CryptSm2KeyGenAndExport(
    __in const char *PubKeyPath,
    __in const char *PriKeyPath,
    __in const char *PriKeyPwd
    )
{
    ERR_T ret = SUCCESS;
    SM2_KEY key;
    FILE *pubKeyFile = NULL, *priKeyFile = NULL;

    if (!PubKeyPath || !PriKeyPwd || !PriKeyPath) {
        ret = -EINVAL;
        goto CommRet;
    }

    pubKeyFile = fopen(PubKeyPath, "wb");
    priKeyFile = fopen(PriKeyPath, "wb");
    if (!priKeyFile || !pubKeyFile) {
        ret = -EIO;
        goto CommRet;
    }

    if (sm2_key_generate(&key) != 1 ||
        sm2_private_key_info_encrypt_to_pem(&key, PriKeyPwd, priKeyFile) != 1 ||
        sm2_public_key_info_to_pem(&key, pubKeyFile) != 1) {
        ret = -EIO;
        LogErr("Gen key failed!\n");
        goto CommRet;
    }

CommRet:
    if (pubKeyFile) {
        fclose(pubKeyFile);
    }
    if (priKeyFile) {
        fclose(priKeyFile);
    }
    return ret;
}

ERR_T
Util_CryptSm2ImportPubKey(
    __in const char *PubKeyPath,
    __out SM2_KEY *PubKey
    )
{
    ERR_T ret = SUCCESS;
    FILE *pubKeyFile = NULL;

    if (!PubKeyPath || !PubKey) {
        ret = -EINVAL;
        goto CommRet;
    }

    pubKeyFile = fopen(PubKeyPath, "rb");
    if (!pubKeyFile) {
        ret = -EIO;
        goto CommRet;
    }

    if (sm2_public_key_info_from_pem(PubKey, pubKeyFile) != 1) {
        ret = -EIO;
        LogErr("Gen key failed!\n");
        goto CommRet;
    }

CommRet:
    if (pubKeyFile) {
        fclose(pubKeyFile);
    }
    return ret;
}

ERR_T
Util_CryptSm2Sign(
    __in const uint8_t *Plain,
    __in size_t PlainLen,
    __in const char *PriKeyPath,
    __in const char *PriKeyPwd,
    __out uint8_t *Sign,
    __inout size_t *SignLen
    )
{
    ERR_T ret = SUCCESS;
	uint8_t dgst[UTIL_CRYPT_SM3_HASH_LEN];
	SM2_SIGNATURE sig;
	SM2_KEY sm2PriKey;
    FILE *priKeyFile = NULL;

    if (!Plain || !PlainLen || !PriKeyPath || !PriKeyPwd || !Sign || !SignLen) {
        ret = -EINVAL;
        goto CommRet;
    }
    memset(&sm2PriKey, 0, sizeof(sm2PriKey));
    memset(&sig, 0, sizeof(sig));

    // get prikey from file
    priKeyFile = fopen(PriKeyPath, "rb");
    if (!priKeyFile) {
        ret = -ENOENT;
        LogErr("Open %s failed!", PriKeyPath);
        goto CommRet;
    }
    if (sm2_private_key_info_decrypt_from_pem(&sm2PriKey, PriKeyPwd, priKeyFile) != 1) {
        ret = -EIO;
        LogErr("Get prikey from %s failed!", PriKeyPath);
        goto CommRet;
    }
    // hash for data
    sm3_digest(Plain, PlainLen, dgst);
    // sign for hash
	if (sm2_do_sign(&sm2PriKey, dgst, &sig) != 1) {
        ret = -EIO;
        LogErr("Sign for date failed!");
        goto CommRet;
    }
    if (*SignLen < sizeof(SM2_SIGNATURE)) {
        ret = -ENOBUFS;
        LogErr("Too small buff, supposed %zu bytes!", sizeof(SM2_SIGNATURE));
        goto CommRet;
    }

    memcpy(Sign, &sig, sizeof(SM2_SIGNATURE));
    *SignLen = sizeof(SM2_SIGNATURE);

CommRet:
    if (priKeyFile) {
        fclose(priKeyFile);
    }
    return ret;
}

ERR_T
Util_CryptSm2Verify(
    __in const uint8_t *Plain,
    __in size_t PlainLen,
    __in const SM2_KEY *PubKey,
    __in const uint8_t *Sign,
    __in size_t SignLen
    )
{
    ERR_T ret = SUCCESS;
	unsigned char dgst[UTIL_CRYPT_SM3_HASH_LEN];

    if (!Plain || !PlainLen || !PubKey || !Sign || !SignLen || SignLen != UTIL_CRYPT_SM2_SIGN_LEN) {
        ret = -EINVAL;
        goto CommRet;
    }

    // hash for data
    sm3_digest(Plain, PlainLen, dgst);
    // verify sign for hash
    if (sm2_do_verify(PubKey, dgst, (SM2_SIGNATURE*)Sign) != 1) { 
        ret = -EIO;
        LogErr("Verify failed!");
        goto CommRet;
	}

CommRet:
    return ret;
}

ERR_T
Util_CryptSm2Encrypt(
    __in const uint8_t *Plain,
    __in size_t PlainLen,
    __in const SM2_KEY *PubKey,
    __out uint8_t *Cipher,
    __inout size_t *CipherLen
    )
{
    ERR_T ret = SUCCESS;

    if (!Plain || !PlainLen || !PubKey || !Cipher || !CipherLen || PlainLen > UTIL_CRYPT_SM2_MAX_PLAIN_SIZE) {
        ret = -EINVAL;
        goto CommRet;
    }
    // encrypt msg
    if (sm2_encrypt(PubKey, Plain, PlainLen, Cipher, CipherLen) != 1) {
        ret = -EINVAL;
        LogErr("Encrypt failed!");
        goto CommRet;
    }

CommRet:
    return ret;
}

ERR_T
Util_CryptSm2Decrypt(
    __in const uint8_t *Cipher,
    __in size_t CipherLen,
    __in const char *PriKeyPath,
    __in const char *PriKeyPwd,
    __out uint8_t *Plain,
    __inout size_t *PlainLen
    )
{
    ERR_T ret = SUCCESS;
	SM2_KEY sm2PriKey;
    FILE* priKeyFile = NULL;

    if (!Plain || !PlainLen || !PriKeyPath || !PriKeyPwd || !Cipher || !CipherLen) {
        ret = -EINVAL;
        goto CommRet;
    }
    // get prikey from file
    priKeyFile = fopen(PriKeyPath, "rb");
    if (!priKeyFile) {
        ret = -ENOENT;
        LogErr("Open %s failed!", PriKeyPath);
        goto CommRet;
    }
    if (sm2_private_key_info_decrypt_from_pem(&sm2PriKey, PriKeyPwd, priKeyFile) != 1) {
        ret = -EIO;
        LogErr("Get prikey from %s failed!", PriKeyPath);
        goto CommRet;
    }
    // encrypt msg
    if (sm2_decrypt(&sm2PriKey, Cipher, CipherLen, Plain, PlainLen) != 1) {
        ret = -EINVAL;
        LogErr("Decrypt failed!");
        goto CommRet;
    }

CommRet:
    if (priKeyFile) {
        fclose(priKeyFile);
    }
    return ret;
}

ERR_T
Util_CryptSm3Hash(
    __in const uint8_t *Input,
    __in size_t InputLen,
    __out uint8_t *Hash,
    __inout size_t *HashLen
    )
{
    ERR_T ret = SUCCESS;
    
    if (!Input || !InputLen || !Hash || !HashLen || *HashLen < UTIL_CRYPT_SM3_HASH_LEN) {
        ret = -EINVAL;
        goto CommRet;
    }

    sm3_digest(Input, InputLen, Hash);
    *HashLen = UTIL_CRYPT_SM3_HASH_LEN;

CommRet:
    return ret;
}

size_t
Util_CryptSm4CBCGetPaddedLen(
    __in size_t PlainLen
    )
{
    return PlainLen % 16 == 0 ? (PlainLen + 16) : (PlainLen/16 + 1) * 16;
}

ERR_T
Util_CryptSm4CBCEncrypt(
    __in const uint8_t *Plain,
    __in size_t PlainLen,
    __in const uint8_t *Key,
    __in size_t KeyLen,
    __out uint8_t *Cipher,
    __inout size_t *CipherLen,
    __out uint8_t *Iv,
    __inout size_t *IvLen
    )
{
    ERR_T ret = SUCCESS;
    SM4_KEY sm4Key;

    if (!Plain || !PlainLen || !Key || KeyLen != UTIL_CRYPT_SM4_KEY_LEN || !Cipher || !CipherLen || 
        *CipherLen < Util_CryptSm4CBCGetPaddedLen(PlainLen) || !Iv || !IvLen || *IvLen < UTIL_CRYPT_SM4_IV_LEN) {
        ret = -EINVAL;
        goto CommRet;
    }
    // generate iv
    *IvLen = UTIL_CRYPT_SM4_IV_LEN;
    if (rand_bytes(Iv, *IvLen) != 1) {
        ret = -EIO;
        goto CommRet;
    }
    // set key
	sm4_set_encrypt_key(&sm4Key, Key);
    // pad data and encrypt
    if (sm4_cbc_padding_encrypt(&sm4Key, Iv, Plain, PlainLen, Cipher, CipherLen) != 1) {
        ret = -EIO;
        LogErr("sm4 cbc padding encrypt failed!");
        goto CommRet;
    }

CommRet:
    return ret;
}

ERR_T
Util_CryptSm4CBCDecrypt(
    __in const uint8_t *Cipher,
    __in size_t CipherLen,
    __in const uint8_t *Key,
    __in size_t KeyLen,
    __in uint8_t *Iv,
    __in size_t IvLen,
    __out uint8_t *Plain,
    __inout size_t *PlainLen
    )
{
    ERR_T ret = SUCCESS;
    SM4_KEY sm4Key;

    if (!Plain || !PlainLen || !Key || KeyLen != UTIL_CRYPT_SM4_KEY_LEN || !Cipher || !CipherLen ||
        !Iv || IvLen != UTIL_CRYPT_SM4_IV_LEN) {
        ret = -EINVAL;
        goto CommRet;
    }
    // set key
	sm4_set_decrypt_key(&sm4Key, Key);
    // pad data and encrypt
    if (sm4_cbc_padding_decrypt(&sm4Key, Iv, Cipher, CipherLen, Plain, PlainLen) != 1) {
        ret = -EIO;
        LogErr("sm4 cbc padding decrypt failed!");
        goto CommRet;
    }

CommRet:
    return ret;
}