#include "hccp_common.h"
void *dlopen_stub(char *lib_path, int para);
void *dlsym_stub(void *handle, char *func);
int dlclose_stub(void *handle);
int ra_init_stub(struct RaInitConfig *config);
