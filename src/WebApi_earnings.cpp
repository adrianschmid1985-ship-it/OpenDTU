// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Electricity price per kWh, used by the web interface to show earnings.
 */
#include "WebApi_earnings.h"
#include "Configuration.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "TotalLimitController.h"
#include "defaults.h"
#include "PowerHistory.h"
#include <Hoymiles.h>
#include <AsyncJson.h>

void WebApiEarningsClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/earnings/config", HTTP_GET, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiEarningsClass::onEarningsGet, this, _1)));
    server.on("/api/earnings/config", HTTP_POST, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiEarningsClass::onEarningsPost, this, _1)));
    server.on("/api/history/today", HTTP_GET, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiEarningsClass::onHistoryGet, this, _1)));
    server.on("/api/totallimit/config", HTTP_POST, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiEarningsClass::onTotalLimitPost, this, _1)));
}

void WebApiEarningsClass::onEarningsGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    root["price_per_kwh"] = Configuration.get().Earnings.PricePerKwh;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiEarningsClass::onEarningsPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    if (!root["price_per_kwh"].is<double>()) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    const double price = root["price_per_kwh"].as<double>();
    if (price < 0 || price > 100) {
        retMsg["message"] = "Price out of range!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    {
        auto guard = Configuration.getWriteGuard();
        guard.getConfig().Earnings.PricePerKwh = price;
    }

    WebApi.writeConfig(retMsg);
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiEarningsClass::onTotalLimitPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    if (!(root["serial"].is<String>() && root["total_limit"].is<uint32_t>() && root["shade_compensation"].is<bool>())) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    const uint64_t serial = strtoull(root["serial"].as<String>().c_str(), NULL, 16);
    const uint32_t limit = root["total_limit"].as<uint32_t>();
    if (limit > MAX_INVERTER_LIMIT) {
        retMsg["message"] = "Limit out of range!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    bool found = false;
    {
        auto guard = Configuration.getWriteGuard();
        auto& config = guard.getConfig();
        for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
            if (config.Inverter[i].Serial == serial && serial != 0) {
                config.Inverter[i].TotalLimit = limit;
                config.Inverter[i].ShadeCompensation = root["shade_compensation"].as<bool>();
                found = true;
            }
        }
    }

    if (!found) {
        retMsg["message"] = "Invalid inverter specified!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    TotalLimitController.reset(serial);
    WebApi.writeConfig(retMsg);
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiEarningsClass::onHistoryGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    root["interval"] = POWER_HISTORY_INTERVAL_MIN;
    root["current"] = PowerHistory.getCurrentSlot();
    JsonArray values = root["values"].to<JsonArray>();
    for (uint16_t i = 0; i < POWER_HISTORY_SLOTS; i++) {
        const int16_t v = PowerHistory.getSlot(i);
        if (v < 0) {
            values.add(nullptr);
        } else {
            values.add(v);
        }
    }

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}
