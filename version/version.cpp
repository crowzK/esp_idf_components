#include "version.hpp"
#include <string.h>

Version::Version(const char* versionStr) :
    version{}
{
    std::string ver;
    int index = 0;
    while(*versionStr)
    {
        if(*versionStr == '.')
        {
            version[index] = std::stoi(ver);
            index++;
            if(index >= 3)
            {
                break;
            }
        }
        ver += *versionStr;
        versionStr += 1;
    }
    if(index <= 2)
    {
        version[index] = std::stoi(ver);
    }
}

Version::~Version()
{

}

std::string Version::get() const
{
    std::string ver;
    ver += std::to_string(version[0]);
    ver += ".";
    ver += std::to_string(version[1]);
    ver += ".";
    ver += std::to_string(version[2]);
    return ver;
}

bool Version::isHigherVersion() const
{
    Version& current = getCurrentSWVer();
    if(version[0] > current.version[0])
    {
        return true;
    }
    else if(version[0] < current.version[0])
    {
        return false;
    }
    else if(version[1] > current.version[1])
    {
        return true;
    }
    else if(version[1] < current.version[1])
    {
        return false;
    }
    else if(version[2] > current.version[2])
    {
        return true;
    }
    return false;
}

Version& Version::getCurrentSWVer()
{
    static Version currentVersion(CURRENT_VERSION);
    return currentVersion;
}