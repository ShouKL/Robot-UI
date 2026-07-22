// hello.cpp — Robot-UI C++ Plugin example
// ============================================
// 编译:
//   MSVC:  cl /EHsc /O2 /Fe:hello_cpp.exe hello.cpp
//   MinGW: g++ -O2 -o hello.exe hello.cpp -static
//
// 放入 plugins/ 目录，自动加载。

#include "../sdk/plugin_sdk.h"

PLUGIN_INFO("HelloCpp", "1.0.0", "dev")

int on_load()    { LOG_INFO("Hello World from C++ plugin!"); return 0; }
int on_enable()  { LOG_INFO("C++ plugin enabled!"); return 0; }
int on_disable() { return 0; }
int on_unload()  { return 0; }
int on_update()  { return 0; }
int on_ui_render(){ return 0; }
int on_menu_bar(){ return 0; }

REGISTER_PLUGIN()
