import {
  Thermometer,
  Droplets,
  Sprout,
  FlaskConical,
  Gauge,
  Wind,
  Sun,
  Waves,
  Activity,
  Zap,
} from 'lucide-react';

// Known metric keys get a friendly label, unit, and icon (matching the
// wireframe's iconography). Everything else falls back to a generic row, so the
// dashboard stays hardware-agnostic: new sensor keys still render, just plainly.
const META = {
  temperature: { label: 'Temperature', unit: '°C', Icon: Thermometer },
  temp: { label: 'Temperature', unit: '°C', Icon: Thermometer },
  humidity: { label: 'Humidity', unit: '%', Icon: Droplets },
  soil_moisture: { label: 'Soil Moisture', unit: '%', Icon: Sprout },
  moisture: { label: 'Moisture', unit: '%', Icon: Sprout },
  ph: { label: 'pH', unit: '', Icon: FlaskConical },
  pressure: { label: 'Pressure', unit: 'hPa', Icon: Gauge },
  co2: { label: 'CO₂', unit: 'ppm', Icon: Wind },
  light: { label: 'Light', unit: 'lux', Icon: Sun },
  water_level: { label: 'Water Level', unit: '%', Icon: Waves },
  flow_rate: { label: 'Flow Rate', unit: 'L/min', Icon: Activity },
  voltage: { label: 'Voltage', unit: 'V', Icon: Zap },
};

function humanize(key) {
  return String(key)
    .replace(/[_-]+/g, ' ')
    .replace(/\b\w/g, (c) => c.toUpperCase());
}

// `labelKey` indexes the i18n dictionary (metric.*). Known keys get one so the
// consumer can translate via t(); unknown keys keep null and fall back to the
// humanized English label (stays hardware-agnostic — new sensor types still
// render, just untranslated).
export function metricMeta(key) {
  const norm = String(key).toLowerCase().replace(/[\s-]+/g, '_');
  const known = META[norm];
  if (known) return { ...known, labelKey: `metric.${norm}` };
  return { label: humanize(key), unit: '', Icon: null, labelKey: null };
}

export function formatMetricValue(value) {
  if (typeof value === 'boolean') return value ? 'ON' : 'OFF';
  if (typeof value === 'number') {
    return Number.isInteger(value) ? String(value) : value.toFixed(1);
  }
  return String(value);
}
