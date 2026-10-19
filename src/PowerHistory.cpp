// SPDX-License-Identifier: GPL-2.0-or-later
#include "PowerHistory.h"
#include "Datastore.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <ctime>

PowerHistoryClass PowerHistory;

PowerHistoryClass::PowerHistoryClass()
    : _loopTask(10 * TASK_SECOND, TASK_FOREVER, std::bind(&PowerHistoryClass::loop, this))
{
    for (auto& s : _slots) {
        s = -1;
    }
}

void PowerHistoryClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();
}

void PowerHistoryClass::loop()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5)) {
        return; // no valid time yet
    }

    // new day: start a fresh curve
    if (timeinfo.tm_yday != _day) {
        for (auto& s : _slots) {
            s = -1;
        }
        _day = timeinfo.tm_yday;
        _currentSlot = -1;
        _sum = 0;
        _count = 0;
    }

    const int16_t slot = (timeinfo.tm_hour * 60 + timeinfo.tm_min) / POWER_HISTORY_INTERVAL_MIN;
    if (slot != _currentSlot) {
        _currentSlot = slot;
        _sum = 0;
        _count = 0;
    }

    float power = Datastore.getTotalAcPowerEnabled();
    if (!std::isfinite(power) || power < 0) {
        power = 0;
    }
    _sum += power;
    _count++;
    _slots[slot] = static_cast<int16_t>(std::lround(std::min(_sum / _count, 30000.0f)));
}
