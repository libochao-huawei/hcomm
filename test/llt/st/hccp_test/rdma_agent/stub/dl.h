#define RTLD_GLOBAL 1
#define RTLD_NOW 1

void *dlopen_stub(char* lib_path, int para);

void *dlsym_stub(void* handle, char* func);

int dlclose_stub(void* handle);

