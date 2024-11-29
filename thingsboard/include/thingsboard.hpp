#pragma once

#include <stdint.h>
#include <string>
#include <mutex>
#include <vector>
#include <map>
#include <atomic>
#include <condition_variable>
#include "ArduinoJson.hpp"

class Topic
{
public:
    Topic(const char* topic);
    Topic(const char* topic, uint32_t strLen);
    ~Topic();

    const std::string get() const;
    bool operator==(const Topic& obj) const;

protected:
    std::vector<std::string> strs;
};

class Mqtt
{
public:
    using SubscribeCallback = std::function<void(const std::string& topic, const std::vector<char>& data)>;
    Mqtt(std::string&& uri);
    ~Mqtt();

    virtual bool connect(const std::string& user);
    bool disConnect();
    bool publish(const std::string& topic, const std::string& data);
    bool subscribe(const std::string& topic, SubscribeCallback&& callback);
    bool unsubscribe(const std::string& topic);

protected:
    virtual void onError(const void* evt);
    virtual void onData(const void* evt);

    virtual void onSubscribed(const void* evt) { };
    virtual void onUnSubscribed(const void* evt) { };
    virtual void onPublished(const void* evt) { };
    virtual void onBeforeConnected(const void* evt) { };
    virtual void onConnected(const void* evt) { };
    virtual void onDisConnected(const void* evt) { };

private:
    static const char *TAG;
    struct Filter
    {
        Topic topic;
        SubscribeCallback callback;
    };
    const std::string uri;
    volatile int32_t rcvEventId;
    volatile int32_t rcvMsgId;
    volatile bool connected;
    std::mutex flowCtrlMutex;
    std::condition_variable flowCtrlCv;
    void* mqttClientHandle;
    std::vector<Filter> filter;
    static void mqttEvtHandler(void* handlerArgs, const char* base, int32_t eventId, void* eventData) noexcept;
};

class ThingsBoard : public Mqtt
{
public:
    ThingsBoard(std::string&& uri);
    ~ThingsBoard();
    bool connect(const std::string& user) override;
    std::string provision(const std::string& deviceName, const std::string& devKey, const std::string& devSec);
    void firmwareUpdate();
    ArduinoJson::JsonDocument requestAttributes(const std::string& data);

protected:
    static const char *TAG;
    std::mutex evtMutex;
    std::condition_variable evtCv;
    uint32_t attributeReqId;
};
