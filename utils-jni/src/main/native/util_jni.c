#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "UtilsCrypto.h"

#define UTIL_JNI_VERSION "MontComm Util JNI 1.0.0"

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    return JNI_VERSION_1_2;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
}

JNIEXPORT jbyteArray JNICALL Java_CommMngr_UtilJNI_randBytes(
    JNIEnv *env, jclass this, jint len)
{
    jbyteArray ret = NULL;
    uint8_t *buf = NULL;

    if (len <= 0) {
        goto end;
    }

    buf = (uint8_t *)malloc((size_t)len);
    if (!buf) {
        goto end;
    }

    if (Util_CryptRandBytes(buf, (size_t)len) != SUCCESS) {
        goto end;
    }

    ret = (*env)->NewByteArray(env, len);
    if (!ret) {
        goto end;
    }
    (*env)->SetByteArrayRegion(env, ret, 0, len, (jbyte *)buf);

end:
    if (buf) {
        free(buf);
    }
    return ret;
}

JNIEXPORT jbyteArray JNICALL Java_CommMngr_UtilJNI_sm3Hash(
    JNIEnv *env, jclass this, jbyteArray input)
{
    jbyteArray ret = NULL;
    jbyte *inbuf = NULL;
    jsize inlen;
    uint8_t hash[32];
    size_t hashLen = sizeof(hash);

    if (!input) {
        goto end;
    }

    inbuf = (*env)->GetByteArrayElements(env, input, NULL);
    if (!inbuf) {
        goto end;
    }
    inlen = (*env)->GetArrayLength(env, input);

    if (Util_CryptSm3Hash((const uint8_t *)inbuf, (size_t)inlen, hash, &hashLen) != SUCCESS) {
        goto end;
    }

    ret = (*env)->NewByteArray(env, (jsize)hashLen);
    if (!ret) {
        goto end;
    }
    (*env)->SetByteArrayRegion(env, ret, 0, (jsize)hashLen, (jbyte *)hash);

end:
    if (inbuf) {
        (*env)->ReleaseByteArrayElements(env, input, inbuf, JNI_ABORT);
    }
    return ret;
}

JNIEXPORT jbyteArray JNICALL Java_CommMngr_UtilJNI_sm3Hmac(
    JNIEnv *env, jclass this, jbyteArray key, jbyteArray input)
{
    jbyteArray ret = NULL;
    jbyte *keybuf = NULL;
    jbyte *inbuf = NULL;
    jsize keyLen, inlen;
    uint8_t hmac[32];
    size_t hmacLen = sizeof(hmac);

    if (!key || !input) {
        goto end;
    }

    keybuf = (*env)->GetByteArrayElements(env, key, NULL);
    if (!keybuf) {
        goto end;
    }
    keyLen = (*env)->GetArrayLength(env, key);

    inbuf = (*env)->GetByteArrayElements(env, input, NULL);
    if (!inbuf) {
        goto end;
    }
    inlen = (*env)->GetArrayLength(env, input);

    if (Util_CryptSm3Hmac((const uint8_t *)keybuf, (size_t)keyLen,
        (const uint8_t *)inbuf, (size_t)inlen, hmac, &hmacLen) != SUCCESS) {
        goto end;
    }

    ret = (*env)->NewByteArray(env, (jsize)hmacLen);
    if (!ret) {
        goto end;
    }
    (*env)->SetByteArrayRegion(env, ret, 0, (jsize)hmacLen, (jbyte *)hmac);

end:
    if (keybuf) {
        (*env)->ReleaseByteArrayElements(env, key, keybuf, JNI_ABORT);
    }
    if (inbuf) {
        (*env)->ReleaseByteArrayElements(env, input, inbuf, JNI_ABORT);
    }
    return ret;
}

JNIEXPORT jbyteArray JNICALL Java_CommMngr_UtilJNI_sm4CbcEncrypt(
    JNIEnv *env, jclass this, jbyteArray plain, jbyteArray key)
{
    jbyteArray ret = NULL;
    jbyte *plainbuf = NULL;
    jbyte *keybuf = NULL;
    jsize plainLen, keyLen;
    size_t paddedLen;
    uint8_t *cipherbuf = NULL;
    uint8_t iv[16];
    size_t ivLen = sizeof(iv);
    size_t cipherLen;

    if (!plain || !key) {
        goto end;
    }

    keybuf = (*env)->GetByteArrayElements(env, key, NULL);
    if (!keybuf) {
        goto end;
    }
    keyLen = (*env)->GetArrayLength(env, key);

    plainbuf = (*env)->GetByteArrayElements(env, plain, NULL);
    if (!plainbuf) {
        goto end;
    }
    plainLen = (*env)->GetArrayLength(env, plain);

    paddedLen = Util_CryptSm4CBCGetPaddedLen((size_t)plainLen);
    cipherbuf = (uint8_t *)malloc(paddedLen);
    if (!cipherbuf) {
        goto end;
    }
    cipherLen = paddedLen;

    if (Util_CryptSm4CBCEncrypt((const uint8_t *)plainbuf, (size_t)plainLen,
        (const uint8_t *)keybuf, (size_t)keyLen,
        cipherbuf, &cipherLen, iv, &ivLen) != SUCCESS) {
        goto end;
    }

    {
        jsize outLen = (jsize)(ivLen + cipherLen);
        jbyte *outbuf = NULL;

        ret = (*env)->NewByteArray(env, outLen);
        if (!ret) {
            goto end;
        }
        outbuf = (*env)->GetByteArrayElements(env, ret, NULL);
        if (!outbuf) {
            (*env)->DeleteLocalRef(env, ret);
            ret = NULL;
            goto end;
        }
        memcpy(outbuf, iv, ivLen);
        memcpy(outbuf + ivLen, cipherbuf, cipherLen);
        (*env)->ReleaseByteArrayElements(env, ret, outbuf, 0);
    }

end:
    if (plainbuf) {
        (*env)->ReleaseByteArrayElements(env, plain, plainbuf, JNI_ABORT);
    }
    if (keybuf) {
        (*env)->ReleaseByteArrayElements(env, key, keybuf, JNI_ABORT);
    }
    if (cipherbuf) {
        free(cipherbuf);
    }
    return ret;
}

JNIEXPORT jbyteArray JNICALL Java_CommMngr_UtilJNI_sm4CbcDecrypt(
    JNIEnv *env, jclass this, jbyteArray cipher, jbyteArray key)
{
    jbyteArray ret = NULL;
    jbyte *cipherbuf = NULL;
    jbyte *keybuf = NULL;
    jsize cipherLen, keyLen;
    uint8_t iv[16];
    uint8_t *plainbuf = NULL;
    size_t plainLen;

    if (!cipher || !key) {
        goto end;
    }

    cipherbuf = (*env)->GetByteArrayElements(env, cipher, NULL);
    if (!cipherbuf) {
        goto end;
    }
    cipherLen = (*env)->GetArrayLength(env, cipher);

    keybuf = (*env)->GetByteArrayElements(env, key, NULL);
    if (!keybuf) {
        goto end;
    }
    keyLen = (*env)->GetArrayLength(env, key);

    if (cipherLen < (jsize)sizeof(iv)) {
        goto end;
    }

    memcpy(iv, cipherbuf, sizeof(iv));

    {
        size_t dataLen = (size_t)(cipherLen - sizeof(iv));
        plainbuf = (uint8_t *)malloc(dataLen);
        if (!plainbuf) {
            goto end;
        }
        plainLen = dataLen;

        if (Util_CryptSm4CBCDecrypt(
            (const uint8_t *)cipherbuf + sizeof(iv), dataLen,
            (const uint8_t *)keybuf, (size_t)keyLen,
            iv, sizeof(iv), plainbuf, &plainLen) != SUCCESS) {
            goto end;
        }

        ret = (*env)->NewByteArray(env, (jsize)plainLen);
        if (!ret) {
            goto end;
        }
        (*env)->SetByteArrayRegion(env, ret, 0, (jsize)plainLen, (jbyte *)plainbuf);
    }

end:
    if (cipherbuf) {
        (*env)->ReleaseByteArrayElements(env, cipher, cipherbuf, JNI_ABORT);
    }
    if (keybuf) {
        (*env)->ReleaseByteArrayElements(env, key, keybuf, JNI_ABORT);
    }
    if (plainbuf) {
        free(plainbuf);
    }
    return ret;
}

JNIEXPORT jbyteArray JNICALL Java_CommMngr_UtilJNI_sm4ECBEncrypt(
    JNIEnv *env, jclass this, jbyteArray plain, jbyteArray key)
{
    jbyteArray ret = NULL;
    jbyte *plainbuf = NULL;
    jbyte *keybuf = NULL;
    jsize plainLen, keyLen;
    size_t paddedLen;
    uint8_t *cipherbuf = NULL;
    size_t cipherLen;

    if (!plain || !key) {
        goto end;
    }

    keybuf = (*env)->GetByteArrayElements(env, key, NULL);
    if (!keybuf) {
        goto end;
    }
    keyLen = (*env)->GetArrayLength(env, key);

    plainbuf = (*env)->GetByteArrayElements(env, plain, NULL);
    if (!plainbuf) {
        goto end;
    }
    plainLen = (*env)->GetArrayLength(env, plain);

    paddedLen = Util_CryptSm4ECBGetPaddedLen((size_t)plainLen);
    cipherbuf = (uint8_t *)malloc(paddedLen);
    if (!cipherbuf) {
        goto end;
    }
    cipherLen = paddedLen;

    if (Util_CryptSm4ECBEncrypt((const uint8_t *)plainbuf, (size_t)plainLen,
        (const uint8_t *)keybuf, (size_t)keyLen,
        cipherbuf, &cipherLen) != SUCCESS) {
        goto end;
    }

    ret = (*env)->NewByteArray(env, (jsize)cipherLen);
    if (!ret) {
        goto end;
    }
    (*env)->SetByteArrayRegion(env, ret, 0, (jsize)cipherLen, (jbyte *)cipherbuf);

end:
    if (plainbuf) {
        (*env)->ReleaseByteArrayElements(env, plain, plainbuf, JNI_ABORT);
    }
    if (keybuf) {
        (*env)->ReleaseByteArrayElements(env, key, keybuf, JNI_ABORT);
    }
    if (cipherbuf) {
        free(cipherbuf);
    }
    return ret;
}

JNIEXPORT jbyteArray JNICALL Java_CommMngr_UtilJNI_sm4ECBDecrypt(
    JNIEnv *env, jclass this, jbyteArray cipher, jbyteArray key)
{
    jbyteArray ret = NULL;
    jbyte *cipherbuf = NULL;
    jbyte *keybuf = NULL;
    jsize cipherLen, keyLen;
    uint8_t *plainbuf = NULL;
    size_t plainLen;

    if (!cipher || !key) {
        goto end;
    }

    cipherbuf = (*env)->GetByteArrayElements(env, cipher, NULL);
    if (!cipherbuf) {
        goto end;
    }
    cipherLen = (*env)->GetArrayLength(env, cipher);

    keybuf = (*env)->GetByteArrayElements(env, key, NULL);
    if (!keybuf) {
        goto end;
    }
    keyLen = (*env)->GetArrayLength(env, key);

    plainbuf = (uint8_t *)malloc((size_t)cipherLen);
    if (!plainbuf) {
        goto end;
    }
    plainLen = (size_t)cipherLen;

    if (Util_CryptSm4ECBDecrypt((const uint8_t *)cipherbuf, (size_t)cipherLen,
        (const uint8_t *)keybuf, (size_t)keyLen,
        plainbuf, &plainLen) != SUCCESS) {
        goto end;
    }

    ret = (*env)->NewByteArray(env, (jsize)plainLen);
    if (!ret) {
        goto end;
    }
    (*env)->SetByteArrayRegion(env, ret, 0, (jsize)plainLen, (jbyte *)plainbuf);

end:
    if (cipherbuf) {
        (*env)->ReleaseByteArrayElements(env, cipher, cipherbuf, JNI_ABORT);
    }
    if (keybuf) {
        (*env)->ReleaseByteArrayElements(env, key, keybuf, JNI_ABORT);
    }
    if (plainbuf) {
        free(plainbuf);
    }
    return ret;
}

JNIEXPORT jstring JNICALL Java_CommMngr_UtilJNI_getVersion(
    JNIEnv *env, jclass this)
{
    return (*env)->NewStringUTF(env, UTIL_JNI_VERSION);
}
