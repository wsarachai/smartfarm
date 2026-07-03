// A physical-LED-style status dot: color by status, same-hue glow + pulse when
// "online". currentColor drives the glow, so text color and bg color match.
const STYLES = {
  online: 'bg-primary text-primary',
  stale: 'bg-tertiary text-tertiary',
  offline: 'bg-error text-error',
};

export default function Led({ status = 'offline', size = 'w-2.5 h-2.5', className = '' }) {
  const style = STYLES[status] || STYLES.offline;
  const pulse = status === 'online' ? 'led-pulse' : '';
  return <span className={`inline-block rounded-full ${size} ${style} ${pulse} ${className}`} />;
}
