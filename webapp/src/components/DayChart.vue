<template>
    <section class="tsl-chart">
        <div class="tsl-chart-head">
            <h2>{{ $t('tesla.ProductionToday') }}</h2>
            <span class="tsl-chart-peak" v-if="peak > 0">
                {{ $t('tesla.PeakToday') }} <strong>{{ $n(peak, 'decimalNoDigits') }} W</strong>
                <template v-if="peakTime"> · {{ peakTime }}</template>
            </span>
        </div>

        <svg
            v-if="hasData"
            :viewBox="`0 0 ${W} ${H}`"
            preserveAspectRatio="none"
            class="tsl-chart-svg"
            role="img"
            :aria-label="$t('tesla.ChartAria', { peak: Math.round(peak) })"
        >
            <defs>
                <linearGradient id="tslFill" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="0" stop-color="var(--tsl-accent)" stop-opacity="0.35" />
                    <stop offset="1" stop-color="var(--tsl-accent)" stop-opacity="0" />
                </linearGradient>
            </defs>
            <line v-for="g in grid" :key="g.v" x1="0" :x2="W" :y1="g.y" :y2="g.y" class="tsl-grid" />
            <path :d="areaPath" fill="url(#tslFill)" />
            <path :d="linePath" class="tsl-line" />
            <line v-if="limit > 0" x1="0" :x2="W" :y1="y(limit)" :y2="y(limit)" class="tsl-limit-line" />
        </svg>
        <div v-else class="tsl-chart-empty">{{ $t('tesla.ChartEmpty') }}</div>

        <div class="tsl-chart-axis" v-if="hasData">
            <span v-for="t in xTicks" :key="t.label" :style="{ left: t.pct + '%' }">{{ t.label }}</span>
        </div>
        <div class="tsl-chart-legend" v-if="hasData">
            <span>{{ $t('tesla.ChartScale', { max: $n(yMax, 'decimalNoDigits') }) }}</span>
            <span v-if="limit > 0"><i class="tsl-legend-dash"></i>{{ $t('tesla.TotalLimit') }}</span>
        </div>
    </section>
</template>

<script lang="ts">
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';

interface History {
    interval: number;
    current: number;
    values: (number | null)[];
}

export default defineComponent({
    props: {
        limit: { type: Number, required: false, default: 0 },
    },
    data() {
        return {
            W: 800,
            H: 200,
            history: { interval: 5, current: -1, values: [] } as History,
            timer: 0,
        };
    },
    created() {
        this.load();
        this.timer = window.setInterval(this.load, 60 * 1000);
    },
    unmounted() {
        window.clearInterval(this.timer);
    },
    computed: {
        perHour(): number {
            return 60 / (this.history.interval || 5);
        },
        range(): { from: number; to: number } {
            // show 06:00 to 20:00 by default, widen if there is production outside
            let from = 6 * this.perHour;
            let to = 20 * this.perHour;
            this.history.values.forEach((v, i) => {
                if (v !== null && v > 0) {
                    from = Math.min(from, i - this.perHour);
                    to = Math.max(to, i + this.perHour);
                }
            });
            return { from: Math.max(0, from), to: Math.min(this.history.values.length - 1, to) };
        },
        points(): { x: number; y: number; v: number; i: number }[] {
            const out: { x: number; y: number; v: number; i: number }[] = [];
            this.history.values.forEach((v, i) => {
                if (v === null || i < this.range.from || i > this.range.to) return;
                out.push({ x: this.x(i), y: this.y(v), v, i });
            });
            return out;
        },
        hasData(): boolean {
            return this.points.length > 1;
        },
        peak(): number {
            return this.history.values.reduce((m: number, v) => (v !== null && v > m ? v : m), 0);
        },
        peakTime(): string {
            const idx = this.history.values.findIndex((v) => v === this.peak);
            return idx >= 0 && this.peak > 0 ? this.slotTime(idx) : '';
        },
        yMax(): number {
            const m = Math.max(this.peak, this.limit, 100) * 1.1;
            const step = m > 1000 ? 200 : 100;
            return Math.ceil(m / step) * step;
        },
        linePath(): string {
            return this.points.map((p, n) => `${n === 0 ? 'M' : 'L'} ${p.x} ${p.y}`).join(' ');
        },
        areaPath(): string {
            if (!this.points.length) return '';
            const first = this.points[0]!;
            const last = this.points[this.points.length - 1]!;
            return `${this.linePath} L ${last.x} ${this.H} L ${first.x} ${this.H} Z`;
        },
        last(): { x: number; y: number } | undefined {
            return this.points[this.points.length - 1];
        },
        grid(): { v: number; y: number }[] {
            return [0.25, 0.5, 0.75].map((f) => ({ v: f, y: this.H - f * this.H }));
        },
        xTicks(): { label: string; pct: number }[] {
            const ticks: { label: string; pct: number }[] = [];
            const span = this.range.to - this.range.from;
            const startHour = Math.ceil(this.range.from / this.perHour);
            const endHour = Math.floor(this.range.to / this.perHour);
            const every = endHour - startHour > 12 ? 4 : 3;
            for (let h = startHour; h <= endHour; h++) {
                if (h % every !== 0) continue;
                const pct = ((h * this.perHour - this.range.from) / span) * 100;
                ticks.push({ label: String(h).padStart(2, '0') + ':00', pct });
            }
            return ticks;
        },
    },
    methods: {
        x(i: number): number {
            const span = Math.max(1, this.range.to - this.range.from);
            return ((i - this.range.from) / span) * this.W;
        },
        y(v: number): number {
            return this.H - (v / this.yMax) * this.H;
        },
        slotTime(i: number): string {
            const minutes = i * (this.history.interval || 5);
            return String(Math.floor(minutes / 60)).padStart(2, '0') + ':' + String(minutes % 60).padStart(2, '0');
        },
        load() {
            fetch('/api/history/today', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data: History) => {
                    if (data && Array.isArray(data.values)) this.history = data;
                })
                .catch(() => {});
        },
    },
});
</script>

<style scoped>
.tsl-chart {
    background: var(--tsl-surface);
    border: 1px solid var(--bs-border-color);
    border-radius: var(--tsl-radius);
    padding: 24px;
    display: flex;
    flex-direction: column;
    gap: 14px;
}
.tsl-chart-head {
    display: flex;
    flex-wrap: wrap;
    justify-content: space-between;
    align-items: baseline;
    gap: 6px;
}
.tsl-chart-head h2 {
    font-size: 18px;
    font-weight: 600;
    margin: 0;
}
.tsl-chart-peak,
.tsl-chart-legend,
.tsl-chart-empty {
    font-size: 13px;
    color: var(--tsl-muted);
}
.tsl-chart-peak strong {
    color: var(--bs-body-color);
    font-weight: 500;
}
.tsl-chart-svg {
    width: 100%;
    height: 200px;
    display: block;
    overflow: visible;
}
.tsl-grid {
    stroke: var(--bs-border-color);
    stroke-width: 1;
    vector-effect: non-scaling-stroke;
}
.tsl-line {
    fill: none;
    stroke: var(--tsl-accent);
    stroke-width: 2.5;
    stroke-linejoin: round;
    vector-effect: non-scaling-stroke;
}
.tsl-limit-line {
    stroke: #ff7a45;
    stroke-width: 1.5;
    stroke-dasharray: 6 6;
    vector-effect: non-scaling-stroke;
}
.tsl-dot {
    fill: var(--tsl-accent);
}
.tsl-chart-empty {
    height: 120px;
    display: flex;
    align-items: center;
    justify-content: center;
    text-align: center;
}
.tsl-chart-axis {
    position: relative;
    height: 16px;
    font-size: 12px;
    color: var(--tsl-muted);
}
.tsl-chart-axis span {
    position: absolute;
    transform: translateX(-50%);
    white-space: nowrap;
}
.tsl-chart-legend {
    display: flex;
    flex-wrap: wrap;
    justify-content: space-between;
    gap: 8px;
}
.tsl-legend-dash {
    display: inline-block;
    width: 16px;
    border-top: 2px dashed #ff7a45;
    vertical-align: middle;
    margin-right: 6px;
}
</style>
