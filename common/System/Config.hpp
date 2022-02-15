#pragma once

// Config is currently hardcoded

class Config
{
public:
    static Config* sInstance;

    bool IsISFSPathReplaced(const char* path);
    bool IsFileLogEnabled();
};