<template>
    <div class="tsl-overview">
        <section class="tsl-hero">
            <div class="tsl-hero-text">
                <span class="tsl-label">{{ $t('tesla.CurrentProduction') }}</span>
                <div class="tsl-hero-value">
                    <span class="tsl-big">{{ $n(Math.round(totalPower), 'decimalNoDigits') }}</span>
                    <span class="tsl-big-unit">W</span>
                </div>
                <span class="tsl-hero-sub">
                    <template v-if="limitWatt > 0">
                        {{
                            $t('tesla.OfLimit', {
                                percent: $n(limitShare, 'decimalNoDigits'),
                                limit: $n(limitWatt, 'decimalNoDigits'),
                            })
                        }}
                        ·
                    </template>
                    {{ $t('tesla.InvertersOnline', { online: onlineCount, total: inverters.length }) }}
                </span>
            </div>
            <div class="tsl-hero-gauge">
                <PowerGauge :value="totalPower" :max="maxPower" :limit="limitWatt" />
                <div class="tsl-gauge-legend" v-if="limitWatt > 0">
                    <span class="tsl-legend-mark"></span
                    >{{ hasTotalLimit ? $t('tesla.TotalLimit') : $t('tesla.LimitMarker') }}
                </div>
            </div>
        </section>

        <section class="tsl-kpis">
            <div class="tsl-tile">
                <span class="tsl-tile-label">{{ $t('tesla.YieldToday') }}</span>
                <span class="tsl-tile-value">{{ $n(yieldDayKwh, 'decimalTwoDigits') }} <small>kWh</small></span>
            </div>
            <div class="tsl-tile">
                <span class="tsl-tile-label">{{ $t('tesla.YieldTotal') }}</span>
                <span class="tsl-tile-value">{{ $n(yieldTotalKwh, 'decimalNoDigits') }} <small>kWh</small></span>
            </div>
            <div class="tsl-tile">
                <span class="tsl-tile-label">{{
                    hasTotalLimit ? $t('tesla.TotalLimit') : $t('tesla.PowerLimit')
                }}</span>
                <span class="tsl-tile-value" v-if="limitWatt > 0">
                    {{ $n(limitWatt, 'decimalNoDigits') }}
                    <small v-if="hasTotalLimit">W</small>
                    <small v-else>W · {{ $n(limitPercent, 'decimalNoDigits') }} %</small>
                </span>
                <span class="tsl-tile-value" v-else>–</span>
            </div>
        </section>

        <DayChart :limit="hasTotalLimit ? limitWatt : 0" />

        <section class="tsl-card">
            <div class="tsl-card-head">
                <h2>{{ $t('tesla.EarningsTitle') }}</h2>
                <span class="tsl-muted">{{ $t('tesla.EarningsSub') }}</span>
            </div>
            <div class="tsl-money-grid">
                <div class="tsl-money">
                    <span class="tsl-tile-label">{{ $t('tesla.Today') }}</span>
                    <span class="tsl-money-value">{{ chf(yieldDayKwh * price) }}</span>
                    <span class="tsl-muted small">
                        {{ $n(yieldDayKwh, 'decimalTwoDigits') }} kWh × {{ chf(price, 2) }}
                    </span>
                </div>
                <div class="tsl-money">
                    <span class="tsl-tile-label">{{ $t('tesla.SinceStart') }}</span>
                    <span class="tsl-money-value tsl-accent">{{ chf(yieldTotalKwh * price) }}</span>
                    <span class="tsl-muted small">
                        {{ $n(yieldTotalKwh, 'decimalTwoDigits') }} kWh × {{ chf(price, 2) }}
                    </span>
                </div>
            </div>
            <form class="tsl-price-row" @submit.prevent="savePrice">
                <label for="tslPrice">{{ $t('tesla.PricePerKwh') }}</label>
                <div class="tsl-price-input">
                    <span>CHF</span>
                    <input
                        id="tslPrice"
                        type="number"
                        step="0.01"
                        min="0"
                        max="100"
                        inputmode="decimal"
                        v-model.number="priceInput"
                        :disabled="!isLogged"
                    />
                </div>
                <button type="submit" class="btn btn-primary" :disabled="!isLogged || saving">
                    {{ saving ? $t('tesla.Saving') : $t('tesla.Save') }}
                </button>
                <span class="tsl-muted small" v-if="!isLogged">{{ $t('tesla.LoginToChange') }}</span>
                <span class="small" :class="saveOk ? 'tsl-ok' : 'text-danger'" v-if="saveMessage">{{
                    saveMessage
                }}</span>
            </form>
        </section>
    </div>
</template>

<script lang="ts">
import DayChart from '@/components/DayChart.vue';
import PowerGauge from '@/components/PowerGauge.vue';
import type { Inverter, Total } from '@/types/LiveDataStatus';
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent, type PropType } from 'vue';

export default defineComponent({
    components: { DayChart, PowerGauge },
    props: {
        totalData: { type: Object as PropType<Total>, required: true },
        inverters: { type: Array as PropType<Inverter[]>, required: true },
        isLogged: { type: Boolean, required: true },
    },
    data() {
        return {
            price: 0,
            priceInput: 0 as number | string,
            saving: false,
            saveMessage: '',
            saveOk: true,
        };
    },
    created() {
        this.loadPrice();
    },
    computed: {
        totalPower(): number {
            return this.totalData?.Power?.v || 0;
        },
        yieldDayKwh(): number {
            const y = this.totalData?.YieldDay;
            if (!y) return 0;
            return y.u === 'Wh' ? y.v / 1000 : y.v;
        },
        yieldTotalKwh(): number {
            const y = this.totalData?.YieldTotal;
            if (!y) return 0;
            return y.u === 'Wh' ? y.v / 1000 : y.v;
        },
        maxPower(): number {
            // derived per inverter from the absolute and relative limit
            let sum = 0;
            this.inverters.forEach((inv) => {
                if (inv.limit_absolute > 0 && inv.limit_relative > 0) {
                    sum += Math.round((inv.limit_absolute * 100) / inv.limit_relative / 10) * 10;
                }
            });
            return sum > 0 ? sum : Math.max(this.totalPower * 1.25, 800);
        },
        limitWatt(): number {
            // a configured total limit wins over the limit sent to the inverter
            let sum = 0;
            this.inverters.forEach((inv) => {
                if (inv.total_limit > 0) sum += inv.total_limit;
                else if (inv.limit_absolute > 0) sum += inv.limit_absolute;
            });
            return sum;
        },
        hasTotalLimit(): boolean {
            return this.inverters.some((i) => i.total_limit > 0);
        },
        limitPercent(): number {
            return this.maxPower > 0 ? (this.limitWatt / this.maxPower) * 100 : 0;
        },
        limitShare(): number {
            return this.limitWatt > 0 ? Math.min((this.totalPower / this.limitWatt) * 100, 999) : 0;
        },
        onlineCount(): number {
            return this.inverters.filter((i) => i.reachable).length;
        },
    },
    methods: {
        chf(v: number, digits = 2): string {
            return new Intl.NumberFormat('de-CH', {
                style: 'currency',
                currency: 'CHF',
                minimumFractionDigits: digits,
                maximumFractionDigits: digits,
            }).format(isFinite(v) ? v : 0);
        },
        loadPrice() {
            fetch('/api/earnings/config', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.price = Number(data.price_per_kwh) || 0;
                    this.priceInput = this.price;
                })
                .catch(() => {});
        },
        savePrice() {
            const value = Number(String(this.priceInput).replace(',', '.'));
            if (!isFinite(value) || value < 0 || value > 100) {
                this.saveOk = false;
                this.saveMessage = this.$t('tesla.InvalidPrice');
                return;
            }
            this.saving = true;
            this.saveMessage = '';
            const formData = new FormData();
            formData.append('data', JSON.stringify({ price_per_kwh: value }));
            fetch('/api/earnings/config', { method: 'POST', headers: authHeader(), body: formData })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    this.saving = false;
                    this.saveOk = response.type === 'success';
                    if (this.saveOk) {
                        this.price = value;
                        this.saveMessage = this.$t('tesla.Saved');
                    } else {
                        this.saveMessage = response.message;
                    }
                })
                .catch(() => {
                    this.saving = false;
                    this.saveOk = false;
                    this.saveMessage = this.$t('tesla.SaveFailed');
                });
        },
    },
});
</script>

<style scoped>
.tsl-overview {
    display: flex;
    flex-direction: column;
    gap: 20px;
    margin-bottom: 28px;
}
.tsl-hero {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    justify-content: space-between;
    gap: 24px;
    padding: 8px 4px 0;
}
.tsl-hero-text {
    flex: 1 1 320px;
    display: flex;
    flex-direction: column;
    gap: 8px;
}
.tsl-hero-value {
    display: flex;
    align-items: baseline;
    gap: 10px;
}
.tsl-big {
    font-size: clamp(64px, 12vw, 108px);
    font-weight: 200;
    line-height: 1;
    letter-spacing: -0.03em;
    font-feature-settings: 'tnum';
}
.tsl-big-unit {
    font-size: 30px;
    color: var(--tsl-muted);
}
.tsl-hero-sub {
    color: var(--tsl-muted);
    font-size: 15px;
}
.tsl-hero-gauge {
    flex: 0 1 360px;
    width: 100%;
}
.tsl-gauge-legend {
    text-align: center;
    color: var(--tsl-muted);
    font-size: 12px;
    margin-top: -6px;
}
.tsl-legend-mark {
    display: inline-block;
    width: 14px;
    height: 3px;
    border-radius: 2px;
    background: #ff7a45;
    vertical-align: middle;
    margin-right: 6px;
}
.tsl-kpis {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 14px;
}
.tsl-tile,
.tsl-card {
    background: var(--tsl-surface);
    border: 1px solid var(--bs-border-color);
    border-radius: var(--tsl-radius);
}
.tsl-tile {
    padding: 20px 22px;
    display: flex;
    flex-direction: column;
    gap: 6px;
}
.tsl-tile-label {
    font-size: 13px;
    color: var(--tsl-muted);
}
.tsl-tile-value {
    font-size: 32px;
    font-weight: 300;
    font-feature-settings: 'tnum';
}
.tsl-tile-value small {
    font-size: 15px;
    color: var(--tsl-muted);
}
.tsl-card {
    padding: 24px;
    display: flex;
    flex-direction: column;
    gap: 20px;
}
.tsl-card-head {
    display: flex;
    flex-wrap: wrap;
    justify-content: space-between;
    gap: 6px;
}
.tsl-card-head h2 {
    font-size: 18px;
    font-weight: 600;
    margin: 0;
}
.tsl-muted {
    color: var(--tsl-muted);
}
.tsl-money-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: 14px;
}
.tsl-money {
    background: var(--tsl-surface-2);
    border-radius: 14px;
    padding: 20px;
    display: flex;
    flex-direction: column;
    gap: 6px;
}
.tsl-money-value {
    font-size: 40px;
    font-weight: 300;
    letter-spacing: -0.01em;
    font-feature-settings: 'tnum';
}
.tsl-accent {
    color: var(--tsl-accent);
}
.tsl-ok {
    color: var(--tsl-ok);
}
.tsl-price-row {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 12px;
}
.tsl-price-input {
    display: flex;
    align-items: center;
    gap: 8px;
    border: 1px solid var(--bs-border-color);
    background: var(--bs-body-bg);
    border-radius: 10px;
    padding: 0 14px;
    min-height: 44px;
}
.tsl-price-input span {
    color: var(--tsl-muted);
    font-size: 14px;
}
.tsl-price-input input {
    width: 90px;
    background: transparent;
    border: none;
    color: var(--bs-body-color);
    font-size: 16px;
    outline: none;
}
.tsl-price-row .btn {
    min-height: 44px;
    padding: 0 20px;
}
</style>
