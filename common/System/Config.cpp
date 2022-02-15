#include "Config.hpp"
#include <cstring>

Config* Config::sInstance;

bool Config::IsISFSPathReplaced(const char* path)
{
    // A list of paths to be replaced will be provided by the channel in the
    // future.
    if (strcmp(path, "/title/00010004/524d4345/data/" /* RMCE */) == 0)
        return true;
    if (strncmp(path, "/title/00010004/524d4350/data/" /* RMCP */,
                sizeof("/title/00010004/524d4350/data/") - 1) == 0)
        return true;
    if (strcmp(path, "/title/00010004/524d434a/data/" /* RMCJ */) == 0)
        return true;
    if (strcmp(path, "/title/00010004/524d434b/data/" /* RMCK */) == 0)
        return true;

    return false;
}

bool Config::IsFileLogEnabled()
{
    return true;
}