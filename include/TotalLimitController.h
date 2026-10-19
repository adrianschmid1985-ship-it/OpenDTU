// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TaskSchedulerDeclarations.h>
#include <cstdint>

/*
 * Regulates the inverter limit so that the total AC output stays at the
 * configured TotalLimit, even though Hoymiles inverters split a limit evenly
 * over all inputs (also unused ones).
 *
 * Without shade compensation the limit is only scaled for unused inputs, so
 * the output can never exceed the total limit. With shade compensation the
 * limit is raised while the output is below the target and other inputs are
 * capped, and lowered again as soon as the output goes above the target.
 */
class TotalLimitControllerClass {
public:
    TotalLimitControllerClass();
    void init(Scheduler& scheduler);

    // Called when the user changed the configuration
    void reset(uint64_t serial);

private:
    void loop();

    struct State {
        uint64_t serial = 0;
        uint32_t lastStatsUpdate = 0;
        uint32_t lastSentMillis = 0;
        float lastSentLimit = 0;
    };

    Task _loopTask;
    State _state[10];

    State& stateFor(uint64_t serial);
};

extern TotalLimitControllerClass TotalLimitController;
