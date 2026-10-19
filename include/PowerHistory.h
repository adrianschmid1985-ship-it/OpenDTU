// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TaskSchedulerDeclarations.h>
#include <cstdint>

#define POWER_HISTORY_INTERVAL_MIN 5
#define POWER_HISTORY_SLOTS (24 * 60 / POWER_HISTORY_INTERVAL_MIN)

/*
 * Keeps today's AC power curve in RAM (5 minute averages).
 * It is not persisted and starts empty after a restart.
 */
class PowerHistoryClass {
public:
    PowerHistoryClass();
    void init(Scheduler& scheduler);

    // -1 = no data for this slot
    int16_t getSlot(uint16_t slot) const { return _slots[slot]; }
    int16_t getCurrentSlot() const { return _currentSlot; }

private:
    void loop();

    Task _loopTask;
    int16_t _slots[POWER_HISTORY_SLOTS];
    int16_t _currentSlot = -1;
    int16_t _day = -1;
    float _sum = 0;
    uint16_t _count = 0;
};

extern PowerHistoryClass PowerHistory;
