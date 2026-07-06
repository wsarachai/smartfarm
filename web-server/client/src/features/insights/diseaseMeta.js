// Status styling for disease-detection results (shared by the card + panel).
const META = {
  healthy: { text: 'text-primary', dot: 'bg-primary', bg: 'bg-primary/15', border: 'border-primary/40' },
  disease: { text: 'text-error', dot: 'bg-error', bg: 'bg-error/15', border: 'border-error/40' },
  inconclusive: { text: 'text-tertiary', dot: 'bg-tertiary', bg: 'bg-tertiary/15', border: 'border-tertiary/40' },
};
const NEUTRAL = {
  text: 'text-on-surface-variant',
  dot: 'bg-outline',
  bg: 'bg-surface-container-high',
  border: 'border-outline-variant',
};

export function diseaseMeta(status) {
  return META[status] || NEUTRAL;
}
