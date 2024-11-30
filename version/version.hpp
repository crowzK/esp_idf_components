#pragma once

#include <string>
#include <stdint.h>

#define CURRENT_VERSION "0.0.1"

class Version
{
public:
    Version(const char* versionStr);
    ~Version();

    std::string get() const;
    bool isHigherVersion() const;
    static Version& getCurrentSWVer();
protected:
    int version[3];
};