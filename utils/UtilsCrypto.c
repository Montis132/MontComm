#include "UtilsCrypto.h"

#include "gmssl/sm2.h"
#include "gmssl/sm3.h"
#include "gmssl/sm4.h"
#include "gmssl/rand.h"
#include "UtilsLogIO.h"

ERR_T
Util_CryptRandBytes(
     uint8_t *RandBuf,
     size_t RandLen
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
     const char *PubKeyPath,
     const char *PriKeyPath,
     const char *PriKeyPwd
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
     const char *PubKeyPath,
     SM2_KEY *PubKey
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
     const uint8_t *Plain,
     size_t PlainLen,
     const char *PriKeyPath,
     const char *PriKeyPwd,
     uint8_t *Sign,
     size_t *SignLen
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
     const uint8_t *Plain,
     size_t PlainLen,
     const SM2_KEY *PubKey,
     const uint8_t *Sign,
     size_t SignLen
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
     const uint8_t *Plain,
     size_t PlainLen,
     const SM2_KEY *PubKey,
     uint8_t *Cipher,
     size_t *CipherLen
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
     const uint8_t *Cipher,
     size_t CipherLen,
     const char *PriKeyPath,
     const char *PriKeyPwd,
     uint8_t *Plain,
     size_t *PlainLen
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
     const uint8_t *Input,
     size_t InputLen,
     uint8_t *Hash,
     size_t *HashLen
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

ERR_T
Util_CryptSm3Hmac(
     const uint8_t *Key,
     size_t KeyLen,
     const uint8_t *Input,
     size_t InputLen,
     uint8_t *Hmac,
     size_t *HmacLen
    )
{
    ERR_T ret = SUCCESS;

    if (!Key || !KeyLen || !Input || !InputLen || !Hmac || !HmacLen || *HmacLen < UTIL_CRYPT_SM3_HMAC_LEN) {
        ret = -EINVAL;
        goto CommRet;
    }

    sm3_hmac(Key, KeyLen, Input, InputLen, Hmac);
    *HmacLen = UTIL_CRYPT_SM3_HMAC_LEN;

CommRet:
    return ret;
}

size_t
Util_CryptSm4ECBGetPaddedLen(
     size_t PlainLen
    )
{
    return PlainLen % SM4_BLOCK_SIZE == 0 ? PlainLen : (PlainLen / SM4_BLOCK_SIZE + 1) * SM4_BLOCK_SIZE;
}

static void _sm4_ecb_padding_encrypt(const SM4_KEY *key, const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen)
{
    size_t blocks = (inlen + SM4_BLOCK_SIZE - 1) / SM4_BLOCK_SIZE;
    size_t i;
    uint8_t block[SM4_BLOCK_SIZE];
    size_t paddedLen = blocks * SM4_BLOCK_SIZE;

    for (i = 0; i < blocks; i++) {
        size_t remain = inlen - i * SM4_BLOCK_SIZE;
        if (remain >= SM4_BLOCK_SIZE) {
            sm4_encrypt(key, in + i * SM4_BLOCK_SIZE, out + i * SM4_BLOCK_SIZE);
        } else {
            memset(block, 0, SM4_BLOCK_SIZE);
            memcpy(block, in + i * SM4_BLOCK_SIZE, remain);
            block[remain] = 0x80;
            sm4_encrypt(key, block, out + i * SM4_BLOCK_SIZE);
        }
    }
    *outlen = paddedLen;
}

static int _sm4_ecb_padding_decrypt(const SM4_KEY *key, const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen)
{
    size_t i, plainLen;

    if (inlen % SM4_BLOCK_SIZE != 0 || inlen == 0) {
        return -1;
    }

    for (i = 0; i < inlen / SM4_BLOCK_SIZE; i++) {
        sm4_encrypt(key, in + i * SM4_BLOCK_SIZE, out + i * SM4_BLOCK_SIZE);
    }

    plainLen = inlen;
    for (i = inlen - 1; i > 0; i--) {
        if (out[i] == 0x80) {
            plainLen = i;
            break;
        }
        if (out[i] != 0x00) {
            break;
        }
    }
    if (plainLen == inlen) {
        return -1;
    }

    *outlen = plainLen;
    return 1;
}

ERR_T
Util_CryptSm4ECBEncrypt(
     const uint8_t *Plain,
     size_t PlainLen,
     const uint8_t *Key,
     size_t KeyLen,
     uint8_t *Cipher,
     size_t *CipherLen
    )
{
    ERR_T ret = SUCCESS;
    SM4_KEY sm4Key;

    if (!Plain || !PlainLen || !Key || KeyLen != UTIL_CRYPT_SM4_KEY_LEN || !Cipher || !CipherLen ||
        *CipherLen < Util_CryptSm4ECBGetPaddedLen(PlainLen)) {
        ret = -EINVAL;
        goto CommRet;
    }

    sm4_set_encrypt_key(&sm4Key, Key);
    _sm4_ecb_padding_encrypt(&sm4Key, Plain, PlainLen, Cipher, CipherLen);

CommRet:
    return ret;
}

ERR_T
Util_CryptSm4ECBDecrypt(
     const uint8_t *Cipher,
     size_t CipherLen,
     const uint8_t *Key,
     size_t KeyLen,
     uint8_t *Plain,
     size_t *PlainLen
    )
{
    ERR_T ret = SUCCESS;
    SM4_KEY sm4Key;

    if (!Plain || !PlainLen || !Key || KeyLen != UTIL_CRYPT_SM4_KEY_LEN || !Cipher || !CipherLen) {
        ret = -EINVAL;
        goto CommRet;
    }

    sm4_set_decrypt_key(&sm4Key, Key);
    if (_sm4_ecb_padding_decrypt(&sm4Key, Cipher, CipherLen, Plain, PlainLen) != 1) {
        ret = -EIO;
        LogErr("sm4 ecb padding decrypt failed!");
        goto CommRet;
    }

CommRet:
    return ret;
}

size_t
Util_CryptSm4CBCGetPaddedLen(
     size_t PlainLen
    )
{
    return PlainLen % 16 == 0 ? (PlainLen + 16) : (PlainLen/16 + 1) * 16;
}

ERR_T
Util_CryptSm4CBCEncrypt(
     const uint8_t *Plain,
     size_t PlainLen,
     const uint8_t *Key,
     size_t KeyLen,
     uint8_t *Cipher,
     size_t *CipherLen,
     uint8_t *Iv,
     size_t *IvLen
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
     const uint8_t *Cipher,
     size_t CipherLen,
     const uint8_t *Key,
     size_t KeyLen,
     uint8_t *Iv,
     size_t IvLen,
     uint8_t *Plain,
     size_t *PlainLen
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