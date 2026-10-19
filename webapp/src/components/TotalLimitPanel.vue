<template>
    <div class="tsl-tl">
        <div class="tsl-tl-head">
            <div>
                <h3>{{ $t('tesla.TotalLimit') }}</h3>
                <p class="tsl-tl-hint">{{ $t('tesla.TotalLimitHint') }}</p>
            </div>
            <span class="tsl-tl-state" :class="{ on: inverter.total_limit > 0 }">
                {{ inverter.total_limit > 0 ? $t('tesla.Active') : $t('tesla.Off') }}
            </span>
        </div>

        <form class="tsl-tl-form" @submit.prevent="save">
            <label :for="'tl-' + inverter.serial">{{ $t('tesla.MaxOutput') }}</label>
            <div class="tsl-tl-input">
                <input
                    :id="'tl-' + inverter.serial"
                    type="number"
                    min="0"
                    max="2250"
                    step="10"
                    inputmode="numeric"
                    v-model.number="limit"
                    :disabled="!isLogged"
                />
                <span>W</span>
            </div>

            <div class="form-check form-switch tsl-tl-switch">
                <input
                    class="form-check-input"
                    type="checkbox"
                    role="switch"
                    :id="'tlsc-' + inverter.serial"
                    v-model="shade"
                    :disabled="!isLogged"
                />
                <label class="form-check-label" :for="'tlsc-' + inverter.serial">
                    {{ $t('tesla.ShadeCompensation') }}
                </label>
            </div>

            <button type="submit" class="btn btn-primary" :disabled="!isLogged || saving">
                {{ saving ? $t('tesla.Saving') : $t('tesla.Save') }}
            </button>
        </form>

        <p class="tsl-tl-status" v-if="inverter.total_limit > 0 && inverter.limit_absolute > 0">
            {{
                $t('tesla.TotalLimitStatus', {
                    target: $n(inverter.total_limit, 'decimalNoDigits'),
                    sent: $n(inverter.limit_absolute, 'decimalNoDigits'),
                })
            }}
        </p>
        <p class="tsl-tl-status" v-if="!isLogged">{{ $t('tesla.LoginToChange') }}</p>
        <p class="tsl-tl-status" :class="ok ? 'tsl-ok' : 'text-danger'" v-if="message">{{ message }}</p>
    </div>
</template>

<script lang="ts">
import type { Inverter } from '@/types/LiveDataStatus';
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent, type PropType } from 'vue';

export default defineComponent({
    props: {
        inverter: { type: Object as PropType<Inverter>, required: true },
        isLogged: { type: Boolean, required: true },
    },
    data() {
        return {
            limit: (this.inverter.total_limit || 0) as number | string,
            shade: this.inverter.shade_compensation ?? true,
            saving: false,
            message: '',
            ok: true,
        };
    },
    methods: {
        save() {
            const value = Math.round(Number(this.limit) || 0);
            if (value < 0 || value > 2250) {
                this.ok = false;
                this.message = this.$t('tesla.InvalidLimit');
                return;
            }
            this.saving = true;
            this.message = '';
            const formData = new FormData();
            formData.append(
                'data',
                JSON.stringify({ serial: this.inverter.serial, total_limit: value, shade_compensation: this.shade })
            );
            fetch('/api/totallimit/config', { method: 'POST', headers: authHeader(), body: formData })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    this.saving = false;
                    this.ok = response.type === 'success';
                    this.message = this.ok
                        ? value > 0
                            ? this.$t('tesla.TotalLimitSaved')
                            : this.$t('tesla.TotalLimitOff')
                        : response.message;
                })
                .catch(() => {
                    this.saving = false;
                    this.ok = false;
                    this.message = this.$t('tesla.SaveFailed');
                });
        },
    },
});
</script>

<style scoped>
.tsl-tl {
    margin-top: 16px;
    background: var(--tsl-surface-2);
    border-radius: 14px;
    padding: 20px;
    display: flex;
    flex-direction: column;
    gap: 14px;
}
.tsl-tl-head {
    display: flex;
    justify-content: space-between;
    align-items: flex-start;
    gap: 12px;
}
.tsl-tl-head h3 {
    font-size: 16px;
    font-weight: 600;
    margin: 0 0 4px;
}
.tsl-tl-hint,
.tsl-tl-status {
    font-size: 13px;
    color: var(--tsl-muted);
    margin: 0;
}
.tsl-tl-state {
    font-size: 12px;
    font-weight: 600;
    padding: 5px 11px;
    border-radius: 999px;
    background: rgba(161, 163, 168, 0.14);
    color: var(--tsl-muted);
    white-space: nowrap;
}
.tsl-tl-state.on {
    background: rgba(242, 177, 61, 0.16);
    color: var(--tsl-accent);
}
.tsl-tl-form {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 12px 16px;
}
.tsl-tl-form > label {
    font-size: 14px;
}
.tsl-tl-input {
    display: flex;
    align-items: center;
    gap: 8px;
    border: 1px solid var(--bs-border-color);
    background: var(--bs-body-bg);
    border-radius: 10px;
    padding: 0 14px;
    min-height: 44px;
}
.tsl-tl-input input {
    width: 80px;
    background: transparent;
    border: none;
    color: var(--bs-body-color);
    font-size: 16px;
    outline: none;
}
.tsl-tl-input span {
    color: var(--tsl-muted);
    font-size: 14px;
}
.tsl-tl-switch {
    margin: 0;
    min-height: 44px;
    display: flex;
    align-items: center;
    gap: 8px;
}
.tsl-tl-form .btn {
    min-height: 44px;
    padding: 0 20px;
}
.tsl-ok {
    color: var(--tsl-ok);
}
</style>
