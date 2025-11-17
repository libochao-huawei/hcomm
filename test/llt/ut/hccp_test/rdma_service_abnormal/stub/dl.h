#include "hccp_common.h"
void *dlopen_stub(char* lib_path, int para);
int dlclose_stub(void* handle);
int ra_init_stub(struct ra_init_config *config);
void *dlsym_stub_1(void* handle, char* func);
void *dlsym_stub_2(void* handle, char* func);
