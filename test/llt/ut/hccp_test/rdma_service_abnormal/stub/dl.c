#include <stdio.h>

void* dl_handle;
static int counter = 0;

void *dlopen_stub(char* lib_path, int para)
{
	return dl_handle;
}

int dlclose_stub(void* handle)
{
	return -1;
}

int ibv_set_device_err_stub(int dev_id)
{
	return -1;
}

int ibv_set_device_ok_stub(int dev_id)
{
	return 0;
}

int (*ibv_set_device_err_stub_p)(int dev_id) = ibv_set_device_err_stub;
int (*ibv_set_device_ok_stub_p)(int dev_id) = ibv_set_device_ok_stub;


void *dlsym_stub_1(void* handle, char* func)
{ 
	return ibv_set_device_err_stub_p;
}

void *dlsym_stub_2(void* handle, char* func)
{ 
	return ibv_set_device_ok_stub_p;
}

