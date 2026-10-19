<template>
    <svg
        class="power-gauge"
        viewBox="0 0 300 230"
        role="img"
        :aria-label="$t('tesla.GaugeAria', { power: Math.round(clampedValue), max: Math.round(scaleMax) })"
    >
        <!-- track -->
        <path :d="arc(0, scaleMax)" class="pg-track" />
        <!-- range above limit -->
        <path v-if="limit > 0 && limit < scaleMax" :d="arc(limit, scaleMax)" class="pg-over" />
        <!-- value -->
        <path v-if="clampedValue > 0" :d="arc(0, clampedValue)" class="pg-value" />

        <!-- ticks and labels -->
        <g v-for="t in ticks" :key="t.v">
            <line
                :x1="pt(t.v, R - 22).x"
                :y1="pt(t.v, R - 22).y"
                :x2="pt(t.v, R - (t.major ? 34 : 28)).x"
                :y2="pt(t.v, R - (t.major ? 34 : 28)).y"
                :class="t.major ? 'pg-tick-major' : 'pg-tick'"
            />
            <text
                v-if="t.major"
                :x="pt(t.v, R - 50).x"
                :y="pt(t.v, R - 50).y"
                class="pg-tick-label"
                text-anchor="middle"
                dominant-baseline="middle"
            >
                {{ formatTick(t.v) }}
            </text>
        </g>

        <!-- limit marker -->
        <g v-if="limit > 0 && limit <= scaleMax">
            <line
                :x1="pt(limit, R + 10).x"
                :y1="pt(limit, R + 10).y"
                :x2="pt(limit, R - 10).x"
                :y2="pt(limit, R - 10).y"
                class="pg-limit"
            />
        </g>

        <!-- needle -->
        <g class="pg-needle" :style="{ transform: `rotate(${angle(clampedValue)}deg)` }">
            <line :x1="CX - 14" :y1="CY" :x2="CX + R - 26" :y2="CY" />
        </g>
        <circle :cx="CX" :cy="CY" r="9" class="pg-hub" />
        <circle :cx="CX" :cy="CY" r="3.5" class="pg-hub-dot" />

        <!-- readout -->
        <text :x="CX" :y="CY + 50" class="pg-value-text" text-anchor="middle">
            {{ $n(Math.round(value), 'decimalNoDigits') }}
        </text>
        <text :x="CX" :y="CY + 68" class="pg-unit-text" text-anchor="middle">W</text>
    </svg>
</template>

<script lang="ts">
import { defineComponent } from 'vue';

const START = 150; // degrees, measured clockwise from 3 o'clock
const SWEEP = 240;

export default defineComponent({
    props: {
        value: { type: Number, required: true },
        max: { type: Number, required: true },
        limit: { type: Number, required: false, default: 0 },
    },
    data() {
        return { CX: 150, CY: 140, R: 118 };
    },
    computed: {
        scaleMax(): number {
            const m = Math.max(this.max, this.limit, 100);
            const step = this.niceStep(m);
            return Math.ceil(m / step) * step;
        },
        clampedValue(): number {
            return Math.min(Math.max(this.value, 0), this.scaleMax);
        },
        ticks(): { v: number; major: boolean }[] {
            const step = this.niceStep(this.scaleMax);
            const minor = step / 4;
            const out: { v: number; major: boolean }[] = [];
            for (let v = 0; v <= this.scaleMax + 0.001; v += minor) {
                out.push({ v, major: Math.abs(v / step - Math.round(v / step)) < 0.001 });
            }
            return out;
        },
    },
    methods: {
        niceStep(m: number): number {
            const raw = m / 8;
            const mag = Math.pow(10, Math.floor(Math.log10(raw)));
            const n = raw / mag;
            const nice = n <= 1 ? 1 : n <= 2 ? 2 : n <= 2.5 ? 2.5 : n <= 5 ? 5 : 10;
            return nice * mag;
        },
        angle(v: number): number {
            return START + (SWEEP * v) / this.scaleMax;
        },
        pt(v: number, r: number): { x: number; y: number } {
            const a = (this.angle(v) * Math.PI) / 180;
            return { x: this.CX + r * Math.cos(a), y: this.CY + r * Math.sin(a) };
        },
        arc(from: number, to: number): string {
            const p1 = this.pt(from, this.R);
            const p2 = this.pt(to, this.R);
            const large = this.angle(to) - this.angle(from) > 180 ? 1 : 0;
            return `M ${p1.x} ${p1.y} A ${this.R} ${this.R} 0 ${large} 1 ${p2.x} ${p2.y}`;
        },
        formatTick(v: number): string {
            return v >= 1000 ? this.$n(v / 1000, 'decimal') + 'k' : String(Math.round(v));
        },
    },
});
</script>

<style scoped>
.power-gauge {
    width: 100%;
    max-width: 360px;
    height: auto;
    display: block;
    margin: 0 auto;
}
.pg-track {
    fill: none;
    stroke: var(--tsl-track);
    stroke-width: 12;
    stroke-linecap: round;
}
.pg-over {
    fill: none;
    stroke: var(--tsl-track);
    stroke-width: 12;
    stroke-dasharray: 2 5;
    opacity: 0.9;
    filter: brightness(1.6);
}
.pg-value {
    fill: none;
    stroke: var(--tsl-accent);
    stroke-width: 12;
    stroke-linecap: round;
    transition: d 0.8s ease;
}
.pg-tick {
    stroke: var(--tsl-muted);
    stroke-width: 1;
    opacity: 0.5;
}
.pg-tick-major {
    stroke: var(--bs-body-color);
    stroke-width: 2;
    opacity: 0.8;
}
.pg-tick-label {
    fill: var(--tsl-muted);
    font-size: 11px;
    font-weight: 500;
}
.pg-limit {
    stroke: #ff7a45;
    stroke-width: 3;
    stroke-linecap: round;
}
.pg-needle {
    transform-origin: 150px 140px;
    transition: transform 0.8s cubic-bezier(0.25, 0.8, 0.25, 1);
}
.pg-needle line {
    stroke: var(--bs-emphasis-color);
    stroke-width: 3;
    stroke-linecap: round;
}
.pg-hub {
    fill: var(--tsl-surface-2);
    stroke: var(--bs-emphasis-color);
    stroke-width: 2;
}
.pg-hub-dot {
    fill: var(--tsl-accent);
}
.pg-value-text {
    fill: var(--bs-emphasis-color);
    font-size: 34px;
    font-weight: 300;
    font-feature-settings: 'tnum';
}
.pg-unit-text {
    fill: var(--tsl-muted);
    font-size: 13px;
    letter-spacing: 0.1em;
}
</style>
