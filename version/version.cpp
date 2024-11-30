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
    if(current.version[0] > version[0])
    {
        return false;
    }
    else if(current.version[1] > version[1])
    {
        return false;
    }
    else if(current.version[2] >= version[2])
    {
        return false;
    }
    return true;
}

Version& Version::getCurrentSWVer()
{
    static Version currentVersion(CURRENT_VERSION);
    return currentVersion;
}