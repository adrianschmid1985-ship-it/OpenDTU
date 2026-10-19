<template>
    <svg
        class="power-gauge"
        viewBox="0 0 300 270"
        role="img"
        :aria-label="$t('tesla.GaugeAria', { power: Math.round(clampedValue), max: Math.round(scaleMax) })"
    >
        <defs>
            <linearGradient :id="gid" gradientUnits="userSpaceOnUse" x1="40" y1="230" x2="260" y2="40">
                <stop offset="0" stop-color="#ff8a3d" />
                <stop offset="0.55" stop-color="var(--tsl-accent)" />
                <stop offset="1" stop-color="#ffe4a8" />
            </linearGradient>
            <filter :id="gid + 'glow'" x="-30%" y="-30%" width="160%" height="160%">
                <feGaussianBlur stdDeviation="7" />
            </filter>
        </defs>

        <!-- scale dots and labels -->
        <circle
            v-for="t in ticks"
            :key="'t' + t.v"
            :cx="pt(t.v, R + 17).x"
            :cy="pt(t.v, R + 17).y"
            :r="t.major ? 2 : 1.1"
            :class="t.v <= clampedValue ? 'pg-dot-on' : 'pg-dot'"
        />
        <text
            v-for="t in labels"
            :key="'l' + t"
            :x="pt(t, R + 34).x"
            :y="pt(t, R + 34).y"
            class="pg-label"
            text-anchor="middle"
            dominant-baseline="middle"
        >
            {{ formatTick(t) }}
        </text>

        <!-- track -->
        <circle
            :cx="CX"
            :cy="CY"
            :r="R"
            class="pg-track"
            :stroke-dasharray="`${arcLen} ${circ}`"
            :transform="`rotate(${START} ${CX} ${CY})`"
        />

        <!-- glow + value arc -->
        <circle
            :cx="CX"
            :cy="CY"
            :r="R"
            class="pg-value pg-glow"
            :stroke="`url(#${gid})`"
            :filter="`url(#${gid}glow)`"
            :stroke-dasharray="`${valueLen} ${circ}`"
            :transform="`rotate(${START} ${CX} ${CY})`"
        />
        <circle
            :cx="CX"
            :cy="CY"
            :r="R"
            class="pg-value"
            :stroke="`url(#${gid})`"
            :stroke-dasharray="`${valueLen} ${circ}`"
            :transform="`rotate(${START} ${CX} ${CY})`"
        />

        <!-- knob -->
        <g class="pg-knob" :style="rot(clampedValue)">
            <circle :cx="CX + R" :cy="CY" r="11" class="pg-knob-ring" />
            <circle :cx="CX + R" :cy="CY" r="5" class="pg-knob-core" />
        </g>

        <!-- limit marker -->
        <g v-if="limit > 0 && limit < scaleMax" class="pg-limit" :style="rot(limit)">
            <line :x1="CX + R + 13" :y1="CY" :x2="CX + R + 28" :y2="CY" />
        </g>

        <!-- readout -->
        <text :x="CX" :y="CY + 6" class="pg-value-text" text-anchor="middle">
            {{ $n(Math.round(value), 'decimalNoDigits') }}
        </text>
        <text :x="CX" :y="CY + 32" class="pg-unit-text" text-anchor="middle">WATT</text>
        <text :x="CX" :y="CY + 80" class="pg-sub-text" text-anchor="middle">
            {{ $n(Math.round((clampedValue / scaleMax) * 100), 'decimalNoDigits') }} % ·
            {{ $t('tesla.OfMax', { max: $n(scaleMax, 'decimalNoDigits') }) }}
        </text>
    </svg>
</template>

<script lang="ts">
import { defineComponent } from 'vue';

const START = 135; // degrees, clockwise from 3 o'clock
const SWEEP = 270;
let uid = 0;

export default defineComponent({
    props: {
        value: { type: Number, required: true },
        max: { type: Number, required: true },
        limit: { type: Number, required: false, default: 0 },
    },
    data() {
        return { CX: 150, CY: 145, R: 100, START, gid: 'pg' + uid++ };
    },
    computed: {
        scaleMax(): number {
            // the scale ends at the inverter's maximum power
            return Math.max(Math.round(this.max / 10) * 10, 100);
        },
        clampedValue(): number {
            return Math.min(Math.max(this.value, 0), this.scaleMax);
        },
        circ(): number {
            return 2 * Math.PI * this.R;
        },
        arcLen(): number {
            return (this.circ * SWEEP) / 360;
        },
        valueLen(): number {
            return (this.arcLen * this.clampedValue) / this.scaleMax;
        },
        labelStep(): number {
            const raw = this.scaleMax / 4;
            if (raw % 50 === 0) return raw; // e.g. 1600 W -> 0, 400, 800, 1200, 1600
            const mag = Math.pow(10, Math.floor(Math.log10(raw)));
            const n = raw / mag;
            const nice = n <= 1 ? 1 : n <= 2 ? 2 : n <= 2.5 ? 2.5 : n <= 5 ? 5 : 10;
            return nice * mag;
        },
        labels(): number[] {
            const out: number[] = [];
            for (let v = 0; v < this.scaleMax - this.labelStep * 0.3; v += this.labelStep) out.push(v);
            out.push(this.scaleMax);
            return out;
        },
        ticks(): { v: number; major: boolean }[] {
            const minor = this.labelStep / 4;
            const out: { v: number; major: boolean }[] = [];
            for (let v = 0; v <= this.scaleMax + 0.001; v += minor) {
                out.push({ v, major: this.labels.some((l) => Math.abs(l - v) < 0.001) });
            }
            return out;
        },
    },
    methods: {
        angle(v: number): number {
            return START + (SWEEP * v) / this.scaleMax;
        },
        pt(v: number, r: number): { x: number; y: number } {
            const a = (this.angle(v) * Math.PI) / 180;
            return { x: this.CX + r * Math.cos(a), y: this.CY + r * Math.sin(a) };
        },
        rot(v: number) {
            return { transform: `rotate(${this.angle(v)}deg)`, transformOrigin: `${this.CX}px ${this.CY}px` };
        },
        formatTick(v: number): string {
            return this.$n(v, 'decimalNoDigits');
        },
    },
});
</script>

<style scoped>
.power-gauge {
    width: 100%;
    max-width: 380px;
    height: auto;
    display: block;
    margin: 0 auto;
    overflow: visible;
}
.pg-track {
    fill: none;
    stroke: var(--tsl-track);
    stroke-width: 16;
    stroke-linecap: round;
}
.pg-value {
    fill: none;
    stroke-width: 16;
    stroke-linecap: round;
    transition: stroke-dasharray 0.9s cubic-bezier(0.25, 0.8, 0.25, 1);
}
.pg-glow {
    opacity: 0.55;
}
.pg-dot {
    fill: var(--tsl-muted);
    opacity: 0.45;
}
.pg-dot-on {
    fill: var(--tsl-accent);
}
.pg-label {
    fill: var(--tsl-muted);
    font-size: 11px;
    font-weight: 500;
    font-feature-settings: 'tnum';
}
.pg-limit line {
    stroke: #ff3b3b;
    stroke-width: 4;
    stroke-linecap: round;
}
.pg-limit,
.pg-knob {
    transition: transform 0.9s cubic-bezier(0.25, 0.8, 0.25, 1);
}
.pg-knob-ring {
    fill: var(--bs-body-bg);
    stroke: #ffffff;
    stroke-width: 3;
}
.pg-knob-core {
    fill: var(--tsl-accent);
}
.pg-value-text {
    fill: var(--bs-emphasis-color);
    font-size: 54px;
    font-weight: 200;
    letter-spacing: -0.02em;
    font-feature-settings: 'tnum';
}
.pg-unit-text {
    fill: var(--tsl-muted);
    font-size: 11px;
    letter-spacing: 0.32em;
}
.pg-sub-text {
    fill: var(--tsl-muted);
    font-size: 12px;
}
</style>
