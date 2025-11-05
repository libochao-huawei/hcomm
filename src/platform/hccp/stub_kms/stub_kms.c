#include "wsecv2_type.h"

unsigned long SdpDecryptEx(WsecUint32 domain, const unsigned char *ciphertext, WsecUint32 ciphertextLen,
    unsigned char *plainText, WsecUint32 *plaintextLen)
{
    return 0;
}

unsigned long SdpEncryptEx(WsecUint32 domain, WsecUint32 algId, const unsigned char *plainText, WsecUint32 plaintextLen,
    unsigned char *ciphertext, WsecUint32 *ciphertextLen)
{
    return 0;
}

unsigned long SdpGetCipherDataLenEx(WsecUint32 plaintextLen, WsecUint32 *ciphertextLenOut)
{
    return 0;
}

unsigned long WsecFinalizeEx(void)
{
    return 0;
}

unsigned long WsecInitializeEx(
    WsecUint32 roleType, const KmcKsfName *filePathName, WsecBool useImportKey, WsecVoid *exParam)
{
    return 0;
}

unsigned long WsecRegFuncEx(const WsecCallbacks *allCallbacks)
{
    return 0;
}