int kmc_enc(char *secu_path, char *store_path, char *pln, unsigned int pln_len)
{
	return 0;
}

int kmc_dec(char *secu_path, char *store_path, void *pln, unsigned int pln_len)
{
	return 0;
}

int kmc_dec_data(struct kmc_enc_info *enc_info, unsigned char *outbuf, unsigned int *size_out)
{
	return 0;
}

int kmc_enc_data(unsigned char *inbuf, unsigned int size_in, struct kmc_enc_info *enc_info)
{
	return 0;
}
