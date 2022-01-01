#include "IPCLog.hpp"
#include "main.h"
#include <Debug/Log.hpp>
#include <cstring>

IPCLog* IPCLog::sInstance;

IPCLog::IPCLog() : m_ipcQueue(8), m_responseQueue(1), m_startRequestQueue(1)
{
    s32 ret = IOS_RegisterResourceManager("/dev/stdout", m_ipcQueue.id());
    if (ret < 0)
        exitClr(YUV_WHITE);
}

void IPCLog::print(const char* buffer)
{
    IOS::Request* req = m_responseQueue.receive();
    memcpy(req->ioctl.io, buffer, printSize);
    req->reply(0);
}

void IPCLog::notify()
{
    IOS::Request* req = m_responseQueue.receive();
    req->reply(1);
}

void IPCLog::handleRequest(IOS::Request* req)
{
    switch (req->cmd) {
    case IOS::Command::Open:
        req->reply(IOSErr::OK);
        break;

    case IOS::Command::Close:
        Log::ipcLogEnabled = false;
        // Wait for any ongoing requests to finish. TODO: This could be done
        // better with a mutex maybe?
        usleep(10000);
        m_responseQueue.receive()->reply(2);
        req->reply(IOSErr::OK);
        break;

    case IOS::Command::Ioctl:
        switch (static_cast<Log::IPCLogIoctl>(req->ioctl.cmd)) {
        case Log::IPCLogIoctl::RegisterPrintHook:
            // Read from console
            if (req->ioctl.io_len != printSize) {
                req->reply(IOSErr::Invalid);
                break;
            }

            // Will reply on next print
            m_responseQueue.send(req);
            break;

        case Log::IPCLogIoctl::StartGameEvent:
            // Start game IOS command
            m_startRequestQueue.send(0);
            req->reply(IOSErr::OK);
            break;

        default:
            req->reply(IOSErr::Invalid);
            break;
        }
        break;

    default:
        req->reply(IOSErr::Invalid);
        break;
    }
}

void IPCLog::run()
{
    while (true) {
        IOS::Request* req = m_ipcQueue.receive();
        handleRequest(req);

        if (req->cmd == IOS::Command::Close)
            break;
    }
}

void IPCLog::waitForStartRequest()
{
    m_startRequestQueue.receive();
}