#pragma once
#include <System/Types.hpp>
#include <System/OS.hpp>

class IPCLog
{
public:
    static IPCLog* sInstance;
    
    static constexpr int printSize = 256;

    IPCLog();
    ~IPCLog();
    void run();
    void print(const char* buffer);
    void notify();

    void waitForStartRequest();

protected:
    void handleRequest(IOS::Request* req);

    Queue<IOS::Request*> m_ipcQueue;
    Queue<IOS::Request*> m_responseQueue;
    Queue<int> m_startRequestQueue;
};