#pragma once

#include <string>
#include <stdint.h>

#ifndef CURRENT_VERSION
#define CURRENT_VERSION "0.0.2"
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