// Shared risk-level styling for the water-stress card + insights page.
const RISK_META = {
  low: { label: 'Low', text: 'text-primary', bg: 'bg-primary/15', border: 'border-primary/40', dot: 'bg-primary' },
  medium: { label: 'Medium', text: 'text-tertiary', bg: 'bg-tertiary/15', border: 'border-tertiary/40', dot: 'bg-tertiary' },
  high: { label: 'High', text: 'text-error', bg: 'bg-error/15', border: 'border-error/40', dot: 'bg-error' },
  unknown: {
    label: 'Unknown',
    text: 'text-on-surface-variant',
    bg: 'bg-surface-container-high',
    border: 'border-outline-variant',
    dot: 'bg-outline',
  },
};

export function riskMeta(risk) {
  return RISK_META[risk] || RISK_META.unknown;
}
