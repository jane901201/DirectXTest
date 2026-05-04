#pragma once

#ifdef MYENGINE_BUILD_DLL
#define MYENGINE_API __declspec(dllexport)
#else
#define MYENGINE_API __declspec(dllimport)
#endif

namespace MyEngine {
    class MYENGINE_API MyApp {
    public:
        int Initialize();
        void Run();
        void Shutdown();
    };
}