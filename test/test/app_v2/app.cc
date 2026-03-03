#include "app.h"
#include <functional>
#include <dlfcn.h>
#define LIB_HANDLE void*
#define LOAD_LIB(name) dlopen(name, RTLD_LAZY)
#define GET_FUNC(handle, name) dlsym(handle, name)
#define CLOSE_LIB(handle) dlclose(handle)

// 函数指针类型
using CreateFunc = hcomm::IRemoteNotify* (*)();
using DestroyFunc = void (*)(hcomm::IRemoteNotify*);
int main()
{
    // 1. 显式加载动态库
    LIB_HANDLE handle = LOAD_LIB("./libbase.so"); // Linux示例
    if (!handle) {
        std::cerr << "Failed to load library.\n";
        return 1;
    }

    // 2. 获取工厂函数
    CreateFunc create = (CreateFunc)GET_FUNC(handle, "CreateRemoteNotify");
    DestroyFunc destroy = (DestroyFunc)GET_FUNC(handle, "DestroyRemoteNotify");

    if (!create || !destroy) {
        std::cerr << "Required symbols not found.\n";
        CLOSE_LIB(handle);
        return 1;
    }

    // 3. 创建对象（通过工厂函数）
    hcomm::IRemoteNotify* obj = create();
    if (!obj) {
        std::cerr << "Object creation failed.\n";
        CLOSE_LIB(handle);
        return 1;
    }

    // 4. 调用方法（通过虚函数接口）
    obj->Open();  // 实际调用 RemoteNotifyV2::Open

    // 5. 销毁对象
    destroy(obj);
    CLOSE_LIB(handle);
    return 0;
}
