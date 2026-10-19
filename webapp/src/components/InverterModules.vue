<template>
    <div class="tsl-inv-meta" v-if="ac || inv">
        <span v-if="ac?.Voltage"
            >AC <strong>{{ fmt(ac.Voltage) }}</strong></span
        >
        <span v-if="ac?.Frequency"
            >{{ $t('tesla.Frequency') }} <strong>{{ fmt(ac.Frequency) }}</strong></span
        >
        <span v-if="inv?.Temperature"
            >{{ $t('tesla.Temperature') }} <strong>{{ fmt(inv.Temperature) }}</strong></span
        >
        <span v-if="inv?.Efficiency"
            >{{ $t('tesla.Efficiency') }} <strong>{{ fmt(inv.Efficiency) }}</strong></span
        >
    </div>
    <div class="tsl-modules" v-if="modules.length">
        <div class="tsl-module" :class="{ unused: m.unused }" v-for="m in modules" :key="m.key">
            <span class="tsl-module-name">{{ m.name }}</span>
            <span class="tsl-module-power">{{ $n(Math.round(m.power), 'decimalNoDigits') }} <small>W</small></span>
            <div class="tsl-module-bar">
                <div :style="{ width: m.share + '%' }"></div>
            </div>
            <span class="tsl-module-sub" v-if="m.unused">{{ $t('tesla.NotConnected') }}</span>
            <span class="tsl-module-sub" v-else>
                {{ m.voltage }} · {{ m.current }} · {{ m.yieldDay }} {{ $t('tesla.Today').toLowerCase() }}
            </span>
        </div>
    </div>
</template>

<script lang="ts">
import type { Inverter, InverterStatistics, ValueObject } from '@/types/LiveDataStatus';
import { defineComponent, type PropType } from 'vue';

export default defineComponent({
    props: {
        inverter: { type: Object as PropType<Inverter>, required: true },
    },
    computed: {
        ac(): InverterStatistics | undefined {
            return this.inverter.AC?.[0];
        },
        inv(): InverterStatistics | undefined {
            return this.inverter.INV?.[0];
        },
        modules() {
            const dc = this.inverter.DC;
            if (!dc) return [];
            const keys = Object.keys(dc)
                .map((k) => +k)
                .sort((a, b) => a - b);
            const list = keys.map((k) => ({ k, ch: dc[k] as InverterStatistics })).filter((x) => x.ch);
            const maxPower = Math.max(
                1,
                ...list.map((x) => x.ch.Irradiation?.max || 0),
                ...list.map((x) => x.ch.Power?.v || 0)
            );
            return list.map(({ k, ch }) => {
                const power = ch.Power?.v || 0;
                const ref = ch.Irradiation?.max || maxPower;
                return {
                    key: k,
                    name: ch.name?.u || this.$t('tesla.Module', { n: k + 1 }),
                    power,
                    unused: (ch.Voltage?.v || 0) < 8 && power < 1,
                    share: Math.min(100, (power / ref) * 100),
                    voltage: ch.Voltage ? this.fmt(ch.Voltage) : '–',
                    current: ch.Current ? this.fmt(ch.Current) : '–',
                    yieldDay: ch.YieldDay ? this.fmt(ch.YieldDay) : '–',
                };
            });
        },
    },
    methods: {
        fmt(o: ValueObject): string {
            return this.$n(o.v, 'decimal', { minimumFractionDigits: o.d, maximumFractionDigits: o.d }) + ' ' + o.u;
        },
    },
});
</script>

<style scoped>
.tsl-inv-meta {
    display: flex;
    flex-wrap: wrap;
    gap: 8px 26px;
    font-size: 14px;
    color: var(--tsl-muted);
    margin-bottom: 16px;
}
.tsl-inv-meta strong {
    color: var(--bs-body-color);
    font-weight: 500;
}
.tsl-modules {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 12px;
}
.tsl-module {
    background: var(--tsl-surface-2);
    border-radius: 14px;
    padding: 18px;
    display: flex;
    flex-direction: column;
    gap: 8px;
}
.tsl-module.unused {
    opacity: 0.45;
}
.tsl-module-name,
.tsl-module-sub {
    font-size: 13px;
    color: var(--tsl-muted);
}
.tsl-module-power {
    font-size: 28px;
    font-weight: 300;
    font-feature-settings: 'tnum';
}
.tsl-module-power small {
    font-size: 14px;
    color: var(--tsl-muted);
}
.tsl-module-bar {
    height: 4px;
    border-radius: 2px;
    background: var(--tsl-track);
    overflow: hidden;
}
.tsl-module-bar div {
    height: 100%;
    background: var(--tsl-accent);
    border-radius: 2px;
    transition: width 0.8s ease;
}
</style>
