#include "encrypt.h"

int do_crypt(crypt_info* input, unsigned char* outbuf, unsigned int* outlen)
{
	return 1;
}

int crypto_gen_key_with_pbkdf2(hash_info* input, unsigned char* key, unsigned int keylen)
{
	return 0;
}

int crypto_encrypt_with_aes_gcm(crypt_info *info, unsigned char *outbuf, unsigned int *out_len)
{
	return 0;
}

int crypto_decrypt_with_aes_gcm(crypt_info *info, unsigned char *outbuf, unsigned int *out_len)
{
	*out_len = 512;
	return 0;
}
