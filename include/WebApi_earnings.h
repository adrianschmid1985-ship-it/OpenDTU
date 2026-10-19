// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

class WebApiEarningsClass {
public:
    void init(AsyncWebServer& server, Scheduler& scheduler);

private:
    void onEarningsGet(AsyncWebServerRequest* request);
    void onEarningsPost(AsyncWebServerRequest* request);
    void onTotalLimitPost(AsyncWebServerRequest* request);
};
