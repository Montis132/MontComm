#ifndef _UTIL_CRYPTO_H_
#define _UTIL_CRYPTO_H_

#ifdef __cplusplus
extern "C"{
#endif
#include "UtilsCommonUtil.h"

#include "gmssl/sm2.h"
#include "gmssl/sm3.h"
#include "gmssl/sm4.h"

#define UTIL_CRYPT_SM2_SIGN_LEN                           64  // sizeof(SM2_SIGNATURE)
#define UTIL_CRYPT_SM2_MAX_PLAIN_SIZE                     255 // SM2_MAX_PLAINTEXT_SIZE
#define UTIL_CRYPT_SM3_HASH_LEN                           32
#define UTIL_CRYPT_SM3_HMAC_LEN                           32  // SM3_HMAC_SIZE
#define UTIL_CRYPT_SM4_KEY_LEN                            16
#define UTIL_CRYPT_SM4_IV_LEN                             16
#define UTIL_CRYPT_SM4_BLOCK_SIZE                         16

ERR_T
Util_CryptRandBytes(
     uint8_t *RandBuf,
     size_t RandLen
    );

ERR_T
Util_CryptSm2KeyGenAndExport(
     const char *PubKeyPath,
     const char *PriKeyPath,
     const char *PriKeyPwd
    );
    
ERR_T
Util_CryptSm2ImportPubKey(
     const char *PubKeyPath,
     SM2_KEY *PubKey
    );

ERR_T
Util_CryptSm2Sign(
     const uint8_t *Plain,
     size_t PlainLen,
     const char *PriKeyPath,
     const char *PriKeyPwd,
     uint8_t *Sign,
     size_t *SignLen
    );

ERR_T
Util_CryptSm2Verify(
     const uint8_t *Plain,
     size_t PlainLen,
     const SM2_KEY *PubKey,
     const uint8_t *Sign,
     size_t SignLen
    );

ERR_T
Util_CryptSm2Encrypt(
     const uint8_t *Plain,
     size_t PlainLen,
     const SM2_KEY *PubKey,
     uint8_t *Cipher,
     size_t *CipherLen
    );

ERR_T
Util_CryptSm2Decrypt(
     const uint8_t *Cipher,
     size_t CipherLen,
     const char *PriKeyPath,
     const char *PriKeyPwd,
     uint8_t *Plain,
     size_t *PlainLen
    );

ERR_T
Util_CryptSm3Hash(
     const uint8_t *Input,
     size_t InputLen,
     uint8_t *Hash,
     size_t *HashLen
    );

ERR_T
Util_CryptSm3Hmac(
     const uint8_t *Key,
     size_t KeyLen,
     const uint8_t *Input,
     size_t InputLen,
     uint8_t *Hmac,
     size_t *HmacLen
    );

size_t
Util_CryptSm4ECBGetPaddedLen(
     size_t PlainLen
    );

ERR_T
Util_CryptSm4ECBEncrypt(
     const uint8_t *Plain,
     size_t PlainLen,
     const uint8_t *Key,
     size_t KeyLen,
     uint8_t *Cipher,
     size_t *CipherLen
    );

ERR_T
Util_CryptSm4ECBDecrypt(
     const uint8_t *Cipher,
     size_t CipherLen,
     const uint8_t *Key,
     size_t KeyLen,
     uint8_t *Plain,
     size_t *PlainLen
    );

size_t
Util_CryptSm4CBCGetPaddedLen(
     size_t PlainLen
    );

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
    );

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
    );

#ifdef __cplusplus
}
#endif

#endif /* _UTIL_CRYPTO_H_ */
