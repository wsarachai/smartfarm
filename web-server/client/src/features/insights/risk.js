// Shared risk-level styling for the water-stress card + insights page.
// `labelKey` indexes the i18n dictionary (risk.*); consumers translate via t().
const RISK_META = {
  low: { label: 'Low', labelKey: 'risk.low', text: 'text-primary', bg: 'bg-primary/15', border: 'border-primary/40', dot: 'bg-primary' },
  medium: { label: 'Medium', labelKey: 'risk.medium', text: 'text-tertiary', bg: 'bg-tertiary/15', border: 'border-tertiary/40', dot: 'bg-tertiary' },
  high: { label: 'High', labelKey: 'risk.high', text: 'text-error', bg: 'bg-error/15', border: 'border-error/40', dot: 'bg-error' },
  unknown: {
    label: 'Unknown',
    labelKey: 'risk.unknown',
    text: 'text-on-surface-variant',
    bg: 'bg-surface-container-high',
    border: 'border-outline-variant',
    dot: 'bg-outline',
  },
};

export function riskMeta(risk) {
  return RISK_META[risk] || RISK_META.unknown;
}
