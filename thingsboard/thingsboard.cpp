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

#include "thingsboard.hpp"


//-------------------------------------------------------------------
// Topic
//-------------------------------------------------------------------
Topic::Topic(const char* topic, uint32_t strLen)
{
    const char* pCh = topic;
    std::string str;
    for(int i = 0 ; i < strLen; i++)
    {
        if(topic[i] == '/')
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

Topic::Topic(const char* topic) :
    Topic(topic, strlen(topic))
{
}

Topic::~Topic()
{
    
}

const std::string Topic::get() const
{
    std::string str;
    for(int i = 0; i < strs.size(); i++)
    {
        if(i != 0)
        {
            str += '/';
        }
        str += strs[i];
    }
    return str;
}

bool Topic::operator==(const Topic& obj) const
{
    const int len = strs.size();
    const int rcvLen = obj.strs.size();
    if(rcvLen < len)
    {
        return false;
    }
    else if((rcvLen > len) and (strs[len-1].compare("+") != 0) and (strs[len-1].compare("#") != 0))
    {
        return false;
    }
    for(int i = 0; i < len; i++)
    {
        if(strs[i].compare(obj.strs[i]) != 0)
        {
            if(strs[i].compare("+") == 0)
            {
                continue;
            }
            else if(strs[i].compare("#") == 0)
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
// Mqtt
//-------------------------------------------------------------------
const char *Mqtt::TAG = "Mqtt";
Mqtt::Mqtt(std::string&& _uri) :
    uri(std::move(_uri)),
    rcvEventId(MQTT_EVENT_ANY),
    rcvMsgId(MQTT_EVENT_ANY),
    connected(false),
    mqttClientHandle(nullptr)
{
}

Mqtt::~Mqtt()
{
    disConnect();
}

bool Mqtt::connect(const std::string& user)
{
    std::unique_lock uk(flowCtrlMutex);
    assert(rcvEventId == MQTT_EVENT_ANY);
    assert(rcvMsgId == MQTT_EVENT_ANY);
    if(connected == true)
    {
        return true;
    }
    
    esp_mqtt_client_config_t mqtt_cfg{};

    mqtt_cfg.broker.address.uri = uri.c_str(),
    mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    mqtt_cfg.session.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
    mqtt_cfg.credentials.username = user.c_str();
    
    mqtt_cfg.network.disable_auto_reconnect = true;

    mqttClientHandle = (void*)esp_mqtt_client_init(&mqtt_cfg);
    
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event((esp_mqtt_client_handle_t)mqttClientHandle, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqttEvtHandler, this);

    bool success = esp_mqtt_client_start((esp_mqtt_client_handle_t)mqttClientHandle) == ESP_OK;
    if(success)
    {
        success = flowCtrlCv.wait_for(uk, std::chrono::seconds(4), [this]{ return rcvEventId == MQTT_EVENT_CONNECTED;});
    }

    connected = success;
    rcvMsgId = MQTT_EVENT_ANY;
    rcvEventId = MQTT_EVENT_ANY;
    ESP_LOGI(TAG, "%s %s", __func__, success ? "success" : "fails");
    return success;
}

bool Mqtt::disConnect()
{
    std::unique_lock uk(flowCtrlMutex);
    assert(rcvEventId == MQTT_EVENT_ANY);
    assert(rcvMsgId == MQTT_EVENT_ANY);
    if(connected == false)
    {
        return true;
    }
    bool success = esp_mqtt_client_stop((esp_mqtt_client_handle_t)mqttClientHandle) == ESP_OK;
    if(success)
    {
        success = flowCtrlCv.wait_for(uk, std::chrono::seconds(4), [this]{ return rcvEventId == MQTT_EVENT_DISCONNECTED;});
    }

    connected = false;
    rcvMsgId = MQTT_EVENT_ANY;
    rcvEventId = MQTT_EVENT_ANY;
    ESP_LOGI(TAG, "%s %s", __func__, success ? "success" : "fails");
    return success;
}

bool Mqtt::publish(const std::string& topic, const std::string& data)
{
    int qos = 0;
    std::unique_lock uk(flowCtrlMutex);
    assert(rcvEventId == MQTT_EVENT_ANY);
    assert(rcvMsgId == MQTT_EVENT_ANY);

    int msgId = esp_mqtt_client_publish((esp_mqtt_client_handle_t)mqttClientHandle, topic.c_str(), data.c_str(), 0, qos, 0);
    ESP_LOGI(TAG, "%s msgId %d", __func__, msgId);
    bool result = msgId == 0;
    if(qos > 0)
    {
        result = flowCtrlCv.wait_for(uk, std::chrono::seconds(4), [this, msgId]{ return rcvMsgId == msgId;});
    }

    rcvMsgId = MQTT_EVENT_ANY;
    rcvEventId = MQTT_EVENT_ANY;
    return result;
}

bool Mqtt::subscribe(const std::string& topic, SubscribeCallback&& callback)
{
    Topic tp(topic.c_str());
    std::unique_lock uk(flowCtrlMutex);
    filter.emplace_back(Filter{std::move(tp), std::move(callback)});
    ESP_LOGI(TAG, "%s %s", "subscribe", tp.get().c_str());
    if(tp.get().compare("#") == 0)
    {
        return true;
    }
    assert(rcvEventId == MQTT_EVENT_ANY);
    assert(rcvMsgId == MQTT_EVENT_ANY);

    int msgId = esp_mqtt_client_subscribe((esp_mqtt_client_handle_t)mqttClientHandle, tp.get().c_str(), 0);
    bool result = flowCtrlCv.wait_for(uk, std::chrono::seconds(4), [this, msgId]{ return rcvMsgId == msgId;});
    
    rcvMsgId = MQTT_EVENT_ANY;
    rcvEventId = MQTT_EVENT_ANY;
    ESP_LOGI(TAG, "%s %s", __func__, result ? "success" : "fails");
    return result;
}

void Mqtt::onError(const void* evt)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)evt;
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
        ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
        ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                    strerror(event->error_handle->esp_transport_sock_errno));
    } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
    } else {
        ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
    }
}

void Mqtt::onData(const void* evt)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)evt;
    ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
    ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
    Topic rcvTopic(event->topic, event->topic_len);
    for(const auto& element : filter)
    {
        if(element.topic == rcvTopic)
        {
            element.callback(event->data, event->data_len);
        }
    }
}

void Mqtt::mqttEvtHandler(void* handlerArgs, const char* base, int32_t eventId, void* eventData) noexcept
{
    Mqtt& mqtt = *(Mqtt*)handlerArgs;
    ESP_LOGI(TAG, "Event dispatched from event loop base=%s, eventId=%" PRIi32, base, eventId);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)eventData;

    std::unique_lock uk(mqtt.flowCtrlMutex);
    mqtt.rcvEventId = eventId;
    mqtt.rcvMsgId = event->msg_id;

    switch ((esp_mqtt_event_id_t)eventId) {
    case MQTT_EVENT_ERROR:
        mqtt.onError(event);
        break;
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqtt.onConnected(event);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt.onDisConnected(event);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        mqtt.onSubscribed(event);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        mqtt.onUnSubscribed(event);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        mqtt.onPublished(event);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        mqtt.onData(event);
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        mqtt.onBeforeConnected(event);
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }

    uk.unlock();
    mqtt.flowCtrlCv.notify_one();
}

//-------------------------------------------------------------------
// ThingsBoard
//-------------------------------------------------------------------

#if 0
void Cloud::provision()
{
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open("credentials", NVS_READWRITE, &handle));

    size_t len = 200;
    char str[len] = {};
    nvs_get_str(handle, "username", str, &len);
    if(strlen(str) == 0)
    {
        // Send a claiming request without any device name (random string will be used as the device name)
        // if the string is empty or null, automatically checked by the sendProvisionRequest method
        std::string device_name;

        // Check if passed DEVICE_NAME was empty,
        // and if it was get the mac address of the wifi chip as fallback and use that one instead
        uint8_t mac[6];
        esp_wifi_get_mac(WIFI_IF_STA, mac);
        char mac_str[18];
        snprintf(mac_str, 18U, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        device_name = mac_str;

        ESP_LOGI(TAG, "connect");
        // Connect to the ThingsBoard server as a client wanting to provision a new device
        if (!tb.connect("iot.crowz.kr", "provision", 8883)) {
            ESP_LOGE(TAG, "Failed to connect to ThingsBoard server with provision account");
            return;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        // Prepare and send the provision request
        const Provision_Callback provisionCallback(Access_Token(), &processProvisionResponse, PROVISION_DEVICE_KEY, PROVISION_DEVICE_SECRET, device_name.c_str(), REQUEST_TIMEOUT_MICROSECONDS, &requestTimedOut);
        provisionRequestSent = prov.Provision_Request(provisionCallback);

        // Wait for the provisioning response to be processed
        while (!provisionResponseProcessed) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        // Disconnect from the cloud client connected to the provision account
        // because the device has been provisioned and can reconnect with new credentials
        if (tb.connected()) {
            tb.disconnect();
        }
        nvs_set_str(handle, "client_id", credentials.client_id.c_str());
        nvs_set_str(handle, "username", credentials.username.c_str());
        nvs_set_str(handle, "password", credentials.password.c_str());
    }
    else
    {
        credentials.username = std::string(str);
        nvs_get_str(handle, "client_id", str, &len);
        credentials.client_id = std::string(str);
        nvs_get_str(handle, "password", str, &len);
        credentials.password = std::string(str);
    }
    nvs_close(handle);

    ESP_LOGI(TAG, "connect");
    // Connect to the ThingsBoard server, as the provisioned client
    tb.connect(THINGSBOARD_SERVER, credentials.username.c_str(), THINGSBOARD_PORT, credentials.client_id.c_str(), credentials.password.c_str());
    serverVersionCheck();
}
#endif

const char *ThingsBoard::TAG = "ThingsBoard";
ThingsBoard::ThingsBoard(std::string&& uri) :
    Mqtt(std::move(uri))
{

}

ThingsBoard::~ThingsBoard()
{

}

bool ThingsBoard::connect(const std::string& user)
{
    bool result = Mqtt::connect(user);
    if(result)
    {
        subscribe("v1/devices/me/attributes/response/+", [](const char* data, uint32_t dataLen){
            ESP_LOGI(TAG, "topic[%s] data[%.*s]", "v1/devices/me/attributes/response/+", (int)dataLen, data);
            });
        subscribe("v1/devices/me/attributes", [](const char* data, uint32_t dataLen){
                ESP_LOGI(TAG, "topic[%s] data[%.*s]", "v1/devices/me/attributes", (int)dataLen, data);
            });
        subscribe("v2/fw/response/+", [](const char* data, uint32_t dataLen){
                ESP_LOGI(TAG, "topic[%s] data[%.*s]", "v2/fw/response/+", (int)dataLen, data);
            });
    }
    return true;
}

std::string ThingsBoard::provision(const std::string& deviceName, const std::string& devKey, const std::string& devSec)
{
    ESP_LOGI(TAG, "provision");
    // Connect to the ThingsBoard server as a client wanting to provision a new device
    if (!Mqtt::connect("provision")) {
        ESP_LOGE(TAG, "Failed to connect to ThingsBoard server with provision account");
        return std::string();
    }
    std::mutex mutex;
    std::condition_variable cv;
    std::unique_lock uk(mutex);
    volatile bool rcv = false;
    std::string token;
    bool success = subscribe("#", [&mutex, &cv, &token, &rcv](const char* data, uint32_t dataLen)
        {    
            ESP_LOGI(TAG, "topic[%s] data[%.*s]", "provision/response", (int)dataLen, data);
            std::unique_lock uk(mutex);
            token = std::string(data, data + dataLen);
            rcv = true;
            uk.unlock();
            cv.notify_one();
        });
    if(success)
    {
        const char* buff = "{'provisionDeviceKey': 'tnq5nvcloyfr80awwr93', 'provisionDeviceSecret': '1q6fed37nr963nuik3r2', 'deviceName': 'F0:F5:BD:75:C7:D8'}";
        ESP_LOGI(TAG, "%s", buff);
        publish("/provision/request", buff);
        bool result = cv.wait_for(uk, std::chrono::seconds(4), [&rcv]{return rcv;});
        if(result)
        {
            ESP_LOGI(TAG, "RCV Token [%s]", token.c_str());
        }
    }
    return std::string();
}