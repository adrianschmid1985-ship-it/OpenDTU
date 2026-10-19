// SPDX-License-Identifier: GPL-2.0-or-later
#include "TotalLimitController.h"
#include "Configuration.h"
#include <Hoymiles.h>
#include <algorithm>
#include <cmath>

#undef TAG
static const char* TAG = "totallimit";

TotalLimitControllerClass TotalLimitController;

// minimum time between two limit commands, gives the inverter time to settle
static constexpr uint32_t MIN_COMMAND_INTERVAL_MS = 15 * 1000;
// stats must be this much younger than the last command before we react
static constexpr uint32_t SETTLE_TIME_MS = 8 * 1000;
// a DC input counts as connected above this voltage
static constexpr float ACTIVE_INPUT_VOLTAGE = 8.0f;
// a DC input counts as capped when it delivers this share of its cap
static constexpr float CAPPED_SHARE = 0.92f;

TotalLimitControllerClass::TotalLimitControllerClass()
    : _loopTask(1000 * TASK_MILLISECOND, TASK_FOREVER, std::bind(&TotalLimitControllerClass::loop, this))
{
}

void TotalLimitControllerClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();
}

TotalLimitControllerClass::State& TotalLimitControllerClass::stateFor(uint64_t serial)
{
    for (auto& s : _state) {
        if (s.serial == serial) {
            return s;
        }
    }
    for (auto& s : _state) {
        if (s.serial == 0) {
            s = State();
            s.serial = serial;
            return s;
        }
    }
    _state[0] = State();
    _state[0].serial = serial;
    return _state[0];
}

void TotalLimitControllerClass::reset(uint64_t serial)
{
    auto& s = stateFor(serial);
    s.lastSentLimit = 0;
    s.lastSentMillis = 0;
    s.lastStatsUpdate = 0;
}

void TotalLimitControllerClass::loop()
{
    const CONFIG_T& config = Configuration.get();

    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        const auto& inv_cfg = config.Inverter[i];
        if (inv_cfg.Serial == 0 || inv_cfg.TotalLimit == 0) {
            continue;
        }

        auto inv = Hoymiles.getInverterBySerial(inv_cfg.Serial);
        if (inv == nullptr || !inv->isReachable() || !inv->getEnableCommands()) {
            continue;
        }

        const float maxPower = inv->DevInfo()->getMaxPower();
        if (maxPower <= 0) {
            continue; // device info not yet known
        }

        auto& st = stateFor(inv_cfg.Serial);
        const uint32_t now = millis();

        // only act on fresh statistics
        const uint32_t statsUpdate = inv->Statistics()->getLastUpdate();
        if (statsUpdate == 0 || statsUpdate == st.lastStatsUpdate) {
            continue;
        }
        st.lastStatsUpdate = statsUpdate;

        if (inv->SystemConfigPara()->getLastLimitCommandSuccess() == CMD_PENDING) {
            continue;
        }
        // inverter restarted or limit changed elsewhere: start over
        if (st.lastSentLimit > 0 && now - st.lastSentMillis > 60 * 1000) {
            const float actual = inv->SystemConfigPara()->getLimitPercent() * maxPower / 100.0f;
            if (std::fabs(actual - st.lastSentLimit) > std::max(10.0f, maxPower * 0.01f)) {
                st.lastSentLimit = 0;
                st.lastSentMillis = 0;
            }
        }

        if (st.lastSentMillis != 0) {
            if (now - st.lastSentMillis < MIN_COMMAND_INTERVAL_MS) {
                continue;
            }
            if (statsUpdate - st.lastSentMillis < SETTLE_TIME_MS) {
                continue; // stats still reflect the old limit
            }
        }

        // analyse DC inputs
        const auto channels = inv->Statistics()->getChannelsByType(TYPE_DC);
        const uint8_t inputs = channels.size();
        if (inputs == 0) {
            continue;
        }
        uint8_t active = 0;
        float maxChannelPower = 0;
        for (auto c : channels) {
            if (inv->Statistics()->getChannelFieldValue(TYPE_DC, c, FLD_UDC) > ACTIVE_INPUT_VOLTAGE) {
                active++;
                maxChannelPower = std::max(maxChannelPower, inv->Statistics()->getChannelFieldValue(TYPE_DC, c, FLD_PDC));
            }
        }
        if (active == 0) {
            continue;
        }

        const float target = std::min<float>(inv_cfg.TotalLimit, maxPower);
        const float base = std::min(maxPower, target * inputs / active);
        const float current = st.lastSentLimit > 0 ? st.lastSentLimit
                                                   : inv->SystemConfigPara()->getLimitPercent() * maxPower / 100.0f;
        const float pac = inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC);
        const float tolerance = std::max(10.0f, target * 0.02f);

        float desired = base;

        if (inv_cfg.ShadeCompensation && st.lastSentLimit > 0) {
            const float cap = current / inputs;
            bool anyCapped = false;
            for (auto c : channels) {
                if (inv->Statistics()->getChannelFieldValue(TYPE_DC, c, FLD_UDC) > ACTIVE_INPUT_VOLTAGE
                    && inv->Statistics()->getChannelFieldValue(TYPE_DC, c, FLD_PDC) >= cap * CAPPED_SHARE) {
                    anyCapped = true;
                }
            }

            if (pac > target + tolerance) {
                // too much: scale down right away
                desired = current * target / pac;
            } else if (pac < target - tolerance && anyCapped) {
                // other inputs could deliver more: raise step by step
                desired = std::min(current * target / std::max(pac, 1.0f), current + maxPower * 0.25f);
            } else if (!anyCapped) {
                // nothing capped: keep only a small headroom above the strongest input
                desired = std::max(base, std::min(current, inputs * maxChannelPower * 1.15f));
            } else {
                desired = current; // on target
            }
        }

        desired = std::clamp(desired, maxPower * 0.03f, maxPower);
        desired = std::round(desired);

        const float minStep = std::max(10.0f, maxPower * 0.01f);
        if (st.lastSentLimit > 0 && std::fabs(desired - st.lastSentLimit) < minStep) {
            continue;
        }

        ESP_LOGI(TAG, "%s: target %.0f W, output %.0f W, %u/%u inputs, limit %.0f -> %.0f W",
            inv->name(), target, pac, active, inputs, current, desired);

        // non persistent: avoids wearing out the inverter's flash
        if (inv->sendActivePowerControlRequest(desired, PowerLimitControlType::AbsolutNonPersistent)) {
            st.lastSentLimit = desired;
            st.lastSentMillis = now;
        }
    }
}
