/* MQTT over SSL Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <chrono>
#include <fstream>
#include <filesystem>
#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_crt_bundle.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include <sys/param.h>
#include "esp_ota_ops.h"

#include "thingsboard.hpp"
#include "version.hpp"
#include "work_queue.hpp"

//-------------------------------------------------------------------
// Topic
//-------------------------------------------------------------------
Topic::Topic(const char *topic, uint32_t strLen)
{
    const char *pCh = topic;
    std::string str;
    for (int i = 0; i < strLen; i++)
    {
        if (topic[i] == '/')
        {
            strs.emplace_back(str);
            str = "";
        }
        else
        {
            str += topic[i];
        }
    }
    strs.emplace_back(str);
}

Topic::Topic(const char *topic) : Topic(topic, strlen(topic))
{
}

Topic::Topic(std::string &&topic) : Topic(topic.c_str(), topic.size())
{
}

Topic::~Topic()
{
}

const std::string Topic::get() const
{
    std::string str;
    for (int i = 0; i < strs.size(); i++)
    {
        if (i != 0)
        {
            str += '/';
        }
        str += strs[i];
    }
    return str;
}

bool Topic::operator==(const Topic &obj) const
{
    const int len = strs.size();
    const int rcvLen = obj.strs.size();
    if (rcvLen < len)
    {
        return false;
    }
    else if ((rcvLen > len) and (strs[len - 1].compare("+") != 0) and (strs[len - 1].compare("#") != 0))
    {
        return false;
    }
    for (int i = 0; i < len; i++)
    {
        if (strs[i].compare(obj.strs[i]) != 0)
        {
            if (strs[i].compare("+") == 0)
            {
                continue;
            }
            else if (strs[i].compare("#") == 0)
            {
                break;
            }
            else
            {
                return false;
            }
        }
    }
    return true;
}
//-------------------------------------------------------------------
// Mqtt::EventHandle
//-------------------------------------------------------------------

Mqtt::EventHandle::EventHandle(Mqtt& _mqtt, int _eventId, EventCallback&& cb) :
    mqtt(_mqtt),
    eventId(_eventId),
    callback(std::move(cb))
{
    mqtt.registerEventHandle(*this);
}

Mqtt::EventHandle::~EventHandle()
{
    mqtt.unRegisterEventHandle(*this);
}

//-------------------------------------------------------------------
// Mqtt
//-------------------------------------------------------------------
const char *Mqtt::TAG = "Mqtt";
Mqtt::Mqtt(std::string &&_uri) : uri(std::move(_uri)),
                                 connected(false),
                                 mqttClientHandle(nullptr)
{
}

Mqtt::~Mqtt()
{
    disConnect();
}

bool Mqtt::connect(const std::string &user)
{
    {    
        std::lock_guard _lk(mutex);
        if (connected == true)
        {
            return true;
        }
    }
    esp_mqtt_client_config_t mqtt_cfg{};

    mqtt_cfg.broker.address.uri = uri.c_str(),
    mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    mqtt_cfg.session.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
    mqtt_cfg.credentials.username = user.c_str();

    mqtt_cfg.network.disable_auto_reconnect = true;

    mqttClientHandle = (void *)esp_mqtt_client_init(&mqtt_cfg);

    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event((esp_mqtt_client_handle_t)mqttClientHandle, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqttEvtHandler, this);

    EventHandle handle(*this, MQTT_EVENT_CONNECTED, [&handle](void *eventData)
        {
            //esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)eventData;
            handle.cv.notify_all();
        });
    
    bool success = esp_mqtt_client_start((esp_mqtt_client_handle_t)mqttClientHandle) == ESP_OK;
    if (success)
    {
        std::unique_lock uk(handle.mutex);
        success = handle.cv.wait_for(uk, std::chrono::seconds(4)) == std::cv_status::no_timeout;
    }

    std::lock_guard _lk(mutex);
    connected = success;
    ESP_LOGD(TAG, "%s %s", __func__, success ? "success" : "fails");
    return success;
}

bool Mqtt::disConnect()
{
    if (connected == false)
    {
        return true;
    }
    EventHandle handle(*this, MQTT_EVENT_DISCONNECTED, [&handle](void *eventData)
    {
        //esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)eventData;
        handle.cv.notify_all();
    });
    bool success = esp_mqtt_client_stop((esp_mqtt_client_handle_t)mqttClientHandle) == ESP_OK;
    if (success)
    {
        std::unique_lock uk(handle.mutex);
        success = handle.cv.wait_for(uk, std::chrono::seconds(4)) == std::cv_status::no_timeout;
    }

    std::lock_guard _lk(mutex);
    connected = false;
    ESP_LOGD(TAG, "%s %s", __func__, success ? "success" : "fails");
    return success;
}

bool Mqtt::publish(const std::string &topic, const std::string &data)
{
    int qos = 0;
    ESP_LOGD(TAG, "%s", __func__);
    int msgId = 0;
    EventHandle handle(*this, MQTT_EVENT_PUBLISHED, [&handle, &msgId](void *eventData)
    {
        std::unique_lock uk(handle.mutex);
        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)eventData;
        if(event->msg_id == msgId)
        {
            uk.unlock();
            handle.cv.notify_all();
        }
    });

    msgId = esp_mqtt_client_publish((esp_mqtt_client_handle_t)mqttClientHandle, topic.c_str(), data.c_str(), 0, qos, 0);
    bool result = msgId == 0;
    if (qos > 0)
    {
        std::unique_lock uk(handle.mutex);
        result = handle.cv.wait_for(uk, std::chrono::seconds(4)) == std::cv_status::no_timeout;
    }

    ESP_LOGD(TAG, "%s %s", __func__, result ? "success" : "fails");
    return result;
}

bool Mqtt::subscribe(const std::string &topic, SubscribeCallback &&callback)
{
    Topic tp(topic.c_str());
    ESP_LOGD(TAG, "%s %s", __func__, tp.get().c_str());
    
    {
        std::lock_guard _lk(mutex);
        filter.emplace_back(Filter{std::move(tp), std::move(callback)});
        if (tp.get().compare("#") == 0)
        {
            return true;
        }
    }

    int msgId = 0;
    EventHandle handle(*this, MQTT_EVENT_SUBSCRIBED, [&handle, &msgId](void *eventData)
    {
        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)eventData;
        std::unique_lock uk(handle.mutex);
        if(event->msg_id == msgId)
        {
            uk.unlock();
            handle.cv.notify_all();
        }
    });

    msgId = esp_mqtt_client_subscribe((esp_mqtt_client_handle_t)mqttClientHandle, tp.get().c_str(), 0);
    std::unique_lock uk(handle.mutex);
    bool result = handle.cv.wait_for(uk, std::chrono::seconds(4)) == std::cv_status::no_timeout;

    ESP_LOGD(TAG, "%s %s", __func__, result ? "success" : "fails");
    return result;
}

bool Mqtt::unsubscribe(const std::string &topic)
{
    Topic tp(topic.c_str());
    ESP_LOGD(TAG, "%s %s", __func__, tp.get().c_str());
    
    {
        std::lock_guard _lk(mutex);
        auto it = std::find_if(filter.begin(), filter.end(), [&tp](const auto &element)
                            { return element.topic == tp; });
        if (it == filter.end())
        {
            return true;
        }
        filter.erase(it);
        if (tp.get().compare("#") == 0)
        {
            return true;
        }
    }

    int msgId = 0;
    EventHandle handle(*this, MQTT_EVENT_UNSUBSCRIBED, [&handle, &msgId](void *eventData)
    {
        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)eventData;
        std::unique_lock uk(handle.mutex);
        if(event->msg_id == msgId)
        {
            uk.unlock();
            handle.cv.notify_all();
        }
    });
    msgId = esp_mqtt_client_subscribe((esp_mqtt_client_handle_t)mqttClientHandle, tp.get().c_str(), 0);
    std::unique_lock uk(handle.mutex);
    bool result = handle.cv.wait_for(uk, std::chrono::seconds(4)) == std::cv_status::no_timeout;

    ESP_LOGD(TAG, "%s %s", __func__, result ? "success" : "fails");
    return result;
}

void Mqtt::onError(const void *evt)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)evt;
    ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
    {
        ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
        ESP_LOGE(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
        ESP_LOGE(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                 strerror(event->error_handle->esp_transport_sock_errno));
    }
    else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
    {
        ESP_LOGE(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
    }
    else
    {
        ESP_LOGE(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
    }
}

void Mqtt::onData(const void *evt)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)evt;
    ESP_LOGD(TAG, "TOPIC=%.*s, data Len %d", event->topic_len, event->topic, event->data_len);
    Topic rcvTopic(event->topic, event->topic_len);
    std::vector<char> data(event->data, event->data + event->data_len);

    std::lock_guard lk(mutex);
    for (const auto &element : filter)
    {
        if (element.topic == rcvTopic)
        {
            element.callback(rcvTopic, data);
        }
    }
}

void Mqtt::registerEventHandle(const EventHandle& handle)
{
    std::lock_guard lk(mutex);
    evtHandles.emplace_back(&handle);
}

void Mqtt::unRegisterEventHandle(const EventHandle& handle)
{
    std::lock_guard lk(mutex);
    evtHandles.remove(&handle);
}

void Mqtt::mqttEvtHandler(void *handlerArgs, const char *base, int32_t eventId, void *eventData) noexcept
{
    Mqtt &mqtt = *(Mqtt *)handlerArgs;
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, eventId=%" PRIi32, base, eventId);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)eventData;

    {
        std::lock_guard lk(mqtt.mutex);
        for(const auto& evt : mqtt.evtHandles)
        {
            if(evt->eventId == eventId)
            {
                evt->callback(eventData);
                return;
            }
        }
    }

    switch ((esp_mqtt_event_id_t)eventId)
    {
    case MQTT_EVENT_ERROR:
        mqtt.onError(event);
        break;
    case MQTT_EVENT_CONNECTED:
        ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
        mqtt.onConnected(event);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt.onDisConnected(event);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        mqtt.onSubscribed(event);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        mqtt.onUnSubscribed(event);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        mqtt.onPublished(event);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGD(TAG, "MQTT_EVENT_DATA");
        mqtt.onData(event);
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGD(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        mqtt.onBeforeConnected(event);
        break;
    default:
        ESP_LOGD(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

//-------------------------------------------------------------------
// ThingsBoard
//-------------------------------------------------------------------
const char *ThingsBoard::TAG = "ThingsBoard";
ThingsBoard::ThingsBoard(std::string &&uri) : Mqtt(std::move(uri)),
                                              attributeReqId(0)
{
}

ThingsBoard::~ThingsBoard()
{
}

bool ThingsBoard::connect(const std::string &user)
{
    bool result = Mqtt::connect(user);
    if (result)
    {
        // subscribe("v1/devices/me/attributes", [](const Topic& topic, const std::vector<char>& data){
        //         ESP_LOGD(TAG, "topic[%s] data[%s]", topic.get().c_str(), data.data());
        //     });
        
        Topic rpcReqTopic(std::string("v1/devices/me/rpc/request/+"));
        subscribe(rpcReqTopic.get().c_str(), 
        [this](const Topic& topic, const std::vector<char>& data)
        {
            WorkQueue& wq = WorkQueue::get();
            wq.invoke([this, data, topic]
            {
                std::unique_lock uk(rpcMutex);
                ArduinoJson::JsonDocument doc;
                ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, data);
                std::string method = doc["method"];
                for(const auto& filter : rpcFilter)
                {
                    ESP_LOGD(TAG, "RPC compare rcv{%s} que{%s}", method.c_str(), filter.function.c_str());
                    if(method.compare(filter.function) == 0)
                    {
                        int reqId = std::stoi(topic.strs[topic.strs.size() - 1]);
                        filter.callback(reqId, doc);
                    }
                }
            });
        });
    }
    return result;
}

ArduinoJson::JsonDocument ThingsBoard::request(Topic &&pubTopic, Topic &&subTopic, const ArduinoJson::JsonDocument &data)
{
    struct Sync
    {
        std::mutex mutex;
        std::condition_variable cv;
    };
    Sync sync;

    std::string json;
    ArduinoJson::serializeJson(data, json);

    ESP_LOGI(TAG, "%s: reqTopic[%s] rcvTopic[%s] data[%s]", __func__, subTopic.get().c_str(), pubTopic.get().c_str(), json.c_str());
    std::unique_lock uk(sync.mutex);
    ArduinoJson::JsonDocument doc;

    bool result = subscribe(subTopic.get(), [&sync, &doc](const Topic &topic, const std::vector<char> &data)
        {
            std::unique_lock uk(sync.mutex);
            ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, data);
            ESP_LOGI(TAG, "%s: rcv[%s]", __func__, data.data());
            uk.unlock();
            sync.cv.notify_one(); 
        });

    if (result)
    {
        result = publish(pubTopic.get(), json);
    }
    if (result)
    {
        sync.cv.wait_for(uk, std::chrono::seconds(4), [&doc]
                         { return not doc.isNull(); });
    }

    unsubscribe(subTopic.get());
    return doc;
}

std::string ThingsBoard::provision(const std::string &deviceName, const std::string &devKey, const std::string &devSec)
{
    ESP_LOGI(TAG, "%s", __func__);
    // Connect to the ThingsBoard server as a client wanting to provision a new device
    if (!Mqtt::connect("provision"))
    {
        ESP_LOGE(TAG, "Failed to connect to ThingsBoard server with provision account");
        return std::string();
    }
    ArduinoJson::JsonDocument doc;
    doc["provisionDeviceKey"] = devKey;
    doc["provisionDeviceSecret"] = devSec;
    doc["deviceName"] = deviceName;
    doc = request(Topic("/provision/request"), Topic("#"), doc);

    ESP_LOGI(TAG, "Rcv toekn: %s", std::string(doc["credentialsValue"]).c_str());
    Mqtt::disConnect();
    return std::string(doc["credentialsValue"]);
}

void ThingsBoard::firmwareUpdate()
{
    constexpr uint32_t chunkSize = 512;
    ESP_LOGD(TAG, "%s", __func__);

    ArduinoJson::JsonDocument doc;
    doc["sharedKeys"] = "fw_checksum,fw_checksum_algorithm,fw_size,fw_title,fw_version";
    doc = requestAttributes(doc);

    ESP_LOGI(TAG, "fw_title: %s", std::string(doc["shared"]["fw_title"]).c_str());
    ESP_LOGI(TAG, "fw_size: %s", std::string(doc["shared"]["fw_size"]).c_str());
    ESP_LOGI(TAG, "fw_version: %s", std::string(doc["shared"]["fw_version"]).c_str());
    ESP_LOGI(TAG, "fw_checksum_algorithm: %s", std::string(doc["shared"]["fw_checksum_algorithm"]).c_str());
    ESP_LOGI(TAG, "fw_checksum: %s", std::string(doc["shared"]["fw_checksum"]).c_str());

    std::string ver = std::string(doc["shared"]["fw_version"]);
    Version rcvVer(ver.c_str());

    if (not rcvVer.isHigherVersion())
    {
        ESP_LOGI(TAG, "Cannot find new verion server{%s}, current{%s}:", rcvVer.get().c_str(), Version::getCurrentSWVer().get().c_str());
        return;
    }
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running)
    {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08" PRIx32 ", but running from offset 0x%08" PRIx32,
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08" PRIx32 ")",
             running->type, running->subtype, running->address);

    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
    {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }

    const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
    esp_app_desc_t invalid_app_info;
    if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
    {
        ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);
    }
    esp_err_t err;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%" PRIx32,
             update_partition->subtype, update_partition->address);
    
    struct Sync
    {
        std::mutex mutex;
        std::condition_variable cv;
        uint32_t rcvSize = 0;
        esp_ota_handle_t update_handle = 0;
        int currentChunk = 0;
    };
    Sync sync;
    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &sync.update_handle);
    std::unique_lock uk(sync.mutex);
    int reqId = 0;
    bool result = false;
    const uint32_t fwSize = doc["shared"]["fw_size"];
    subscribe("v2/fw/response/+",
        [&sync](const Topic &topic, const std::vector<char> &data)
        {
            std::unique_lock uk(sync.mutex);
            esp_err_t err = esp_ota_write(sync.update_handle, (const void *)data.data(), data.size());
            sync.rcvSize += data.size();
            sync.currentChunk++;
            uk.unlock();
            sync.cv.notify_one();
        });
    do
    {
        char buf[300]{};
        int reqChunk = sync.currentChunk;
        snprintf(buf, 300, "v2/fw/request/%d/chunk/%d", reqId, sync.currentChunk);
        publish(buf, std::to_string(chunkSize));
        result = sync.cv.wait_for(uk, std::chrono::seconds(4), 
            [&sync, &reqChunk]
            {
                return sync.currentChunk > reqChunk;
            });
        if (sync.rcvSize % (100 * 1024) == 0)
        {
            ESP_LOGI(TAG, "RcvSize %d chunkSize %d", (int)sync.rcvSize, (int)chunkSize);
        }
    } while (sync.rcvSize < fwSize and result);

    ESP_LOGI(TAG, "%s %s fwSize: %d, rcvSize: %d", __func__, result ? "success" : "fails", (int)fwSize, (int)sync.rcvSize);

    if (result)
    {
        err = esp_ota_end(sync.update_handle);
        if (err != ESP_OK)
        {
            if (err == ESP_ERR_OTA_VALIDATE_FAILED)
            {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            else
            {
                ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
            }
            return;
        }
    }
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
    }
    esp_restart();
}

ArduinoJson::JsonDocument ThingsBoard::requestAttributes(const ArduinoJson::JsonDocument &doc)
{
    const int id = attributeReqId++;
    Topic pubTopic(std::string("v1/devices/me/attributes/request/") + std::to_string(id));
    Topic subTopic("v1/devices/me/attributes/response/+");
    ArduinoJson::JsonDocument _doc = request(std::move(pubTopic), std::move(subTopic), doc);
    ESP_LOGD(TAG, "%s %s", __func__, _doc.isNull() ? "fails" : "success");
    return _doc;
}

bool ThingsBoard::sendTelemetry(const ArduinoJson::JsonDocument &doc)
{
    std::string json;
    ArduinoJson::serializeJson(doc, json);
    return publish("v1/devices/me/telemetry", json.c_str());
}

bool ThingsBoard::registerRpcCallback(const std::string& function, RpcCallback&& callback)
{
    std::unique_lock uk(rpcMutex);
    rpcFilter.emplace_back(RpcFilter(function, std::move(callback)));

    return true;
}