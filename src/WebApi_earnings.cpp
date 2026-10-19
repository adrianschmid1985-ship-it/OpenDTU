// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Electricity price per kWh, used by the web interface to show earnings.
 */
#include "WebApi_earnings.h"
#include "Configuration.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include <AsyncJson.h>

void WebApiEarningsClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/earnings/config", HTTP_GET, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiEarningsClass::onEarningsGet, this, _1)));
    server.on("/api/earnings/config", HTTP_POST, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiEarningsClass::onEarningsPost, this, _1)));
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
