#include "ina3221.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char *TAG = "INA3221";

// INA3221 Register Addresses
constexpr uint8_t INA3221_REG_CONFIG = 0x00;
constexpr uint8_t INA3221_REG_SHUNTVOLTAGE_1 = 0x01;
constexpr uint8_t INA3221_REG_BUSVOLTAGE_1 = 0x02;
constexpr uint8_t INA3221_REG_SHUNTVOLTAGE_2 = 0x03;
constexpr uint8_t INA3221_REG_BUSVOLTAGE_2 = 0x04;
constexpr uint8_t INA3221_REG_SHUNTVOLTAGE_3 = 0x05;
constexpr uint8_t INA3221_REG_BUSVOLTAGE_3 = 0x06;
constexpr uint8_t INA3221_REG_MANUFACTURER_ID = 0xFE;

constexpr uint16_t INA3221_MANUFACTURER_ID_VALUE = 0x5449; // 'TI'
constexpr float INA3221_BUS_VOLTAGE_LSB = 0.008f;          // 8 mV
constexpr float INA3221_SHUNT_VOLTAGE_LSB = 0.04f;         // 40 µV in mV

Ina3221::Ina3221(I2cMaster &i2c_master, uint8_t address) : _i2c_master(i2c_master), _address(address) {}

esp_err_t Ina3221::begin()
{
    uint16_t man_id;
    esp_err_t err = get_manufacturer_id(man_id);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read manufacturer ID.");
        return err;
    }
    if (man_id != INA3221_MANUFACTURER_ID_VALUE)
    {
        ESP_LOGE(TAG, "Invalid manufacturer ID: 0x%04X. Expected 0x%04X", man_id, INA3221_MANUFACTURER_ID_VALUE);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "INA3221 at 0x%X initialized successfully.", _address);
    return ESP_OK;
}

esp_err_t Ina3221::get_bus_voltage_V(Ina3221Channel channel, float &voltage)
{
    uint8_t reg = INA3221_REG_BUSVOLTAGE_1 + ((channel - 1) * 2);
    int16_t raw_voltage;
    esp_err_t err = read_register(reg, raw_voltage);
    if (err != ESP_OK)
        return err;
    voltage = (raw_voltage >> 3) * INA3221_BUS_VOLTAGE_LSB;
    return ESP_OK;
}

esp_err_t Ina3221::get_shunt_voltage_mV(Ina3221Channel channel, float &voltage)
{
    uint8_t reg = INA3221_REG_SHUNTVOLTAGE_1 + ((channel - 1) * 2);
    int16_t raw_voltage;
    esp_err_t err = read_register(reg, raw_voltage);
    if (err != ESP_OK)
        return err;
    voltage = (raw_voltage >> 3) * INA3221_SHUNT_VOLTAGE_LSB;
    return ESP_OK;
}

esp_err_t Ina3221::get_current_mA(Ina3221Channel channel, float shunt_resistance_ohm, float &current)
{
    float shunt_voltage_mv;
    esp_err_t err = get_shunt_voltage_mV(channel, shunt_voltage_mv);
    if (err != ESP_OK)
        return err;
    current = shunt_voltage_mv / shunt_resistance_ohm;
    return ESP_OK;
}

esp_err_t Ina3221::get_manufacturer_id(uint16_t &id)
{
    int16_t raw_id;
    esp_err_t err = read_register(INA3221_REG_MANUFACTURER_ID, raw_id);
    id = (uint16_t)raw_id;
    return err;
}

esp_err_t Ina3221::write_register(uint8_t reg, uint16_t value)
{
    uint8_t buffer[3];
    buffer[0] = reg;
    buffer[1] = (value >> 8) & 0xFF;
    buffer[2] = value & 0xFF;
    return _i2c_master.write(_address, buffer, 3);
}

esp_err_t Ina3221::read_register(uint8_t reg, int16_t &value)
{
    uint8_t write_buf[] = {reg};
    uint8_t read_buf[2];

    esp_err_t err = _i2c_master.write(_address, write_buf, 1);
    if (err != ESP_OK)
    {
        return err;
    }

    err = _i2c_master.read(_address, read_buf, 2);
    if (err != ESP_OK)
    {
        return err;
    }

    value = (read_buf[0] << 8) | read_buf[1];
    return ESP_OK;
}
