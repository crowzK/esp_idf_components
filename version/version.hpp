#pragma once

#include <string>
#include <stdint.h>

#ifndef CONFIG_SW_VERSION
#define CONFIG_SW_VERSION "0.0.0"
#endif

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