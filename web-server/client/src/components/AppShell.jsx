import { Link, useLocation } from 'react-router-dom';
import { Cpu, SlidersHorizontal, LayoutDashboard, Video, Droplet, BrainCircuit } from 'lucide-react';

// Routable pages have a `to`; the rest are visible-but-inert chrome until their
// wireframes (AI Analytics) are implemented.
const NAV = [
  { label: 'DASHBOARD', Icon: LayoutDashboard, to: '/' },
  { label: 'CAMERAS', Icon: Video, to: '/cameras' },
  { label: 'IRRIGATION', Icon: Droplet, to: '/irrigation' },
  { label: 'AI ANALYTICS', Icon: BrainCircuit },
];

export default function AppShell({ children }) {
  const { pathname } = useLocation();
  const isActive = (to) => (to === '/' ? pathname === '/' : pathname.startsWith(to));

  return (
    <div className="min-h-screen">
      <header className="sticky top-0 z-50 w-full bg-background border-b border-outline-variant flex items-center justify-between px-margin-mobile py-unit md:px-margin-desktop">
        <Link to="/" className="flex items-center gap-3">
          <Cpu className="text-primary" size={22} />
          <h1 className="font-headline-sm text-headline-sm font-bold text-primary tracking-tight">
            SMART FARM CONTROL CENTER
          </h1>
        </Link>

        <nav className="hidden md:flex items-center gap-6">
          {NAV.map(({ label, to }) => {
            if (!to) {
              return (
                <button
                  key={label}
                  type="button"
                  disabled
                  title="Coming soon"
                  className="font-label-caps text-label-caps text-on-surface-variant/60 cursor-not-allowed"
                >
                  {label}
                </button>
              );
            }
            const active = isActive(to);
            return (
              <Link
                key={label}
                to={to}
                className={`font-label-caps text-label-caps px-3 py-2 transition-colors ${
                  active ? 'text-primary font-bold' : 'text-on-surface-variant hover:text-primary'
                }`}
              >
                {label}
              </Link>
            );
          })}
        </nav>

        <button
          type="button"
          title="Settings (coming soon)"
          className="text-on-surface-variant hover:bg-surface-container-high p-2 rounded-full transition-transform active:scale-95"
        >
          <SlidersHorizontal size={20} />
        </button>
      </header>

      <main className="max-w-container-max mx-auto p-margin-mobile md:p-margin-desktop pb-24 md:pb-8">
        {children}
      </main>

      {/* Mobile bottom nav. */}
      <nav className="md:hidden fixed bottom-0 z-50 w-full h-16 px-2 bg-surface-container border-t border-outline-variant flex justify-around items-center">
        {NAV.map(({ label, Icon, to }) => {
          const active = to && isActive(to);
          const content = (
            <>
              <Icon size={22} />
              <span className="font-label-caps text-label-caps">{label}</span>
            </>
          );
          const cls = `flex flex-col items-center justify-center ${
            active ? 'text-primary font-bold' : 'text-on-surface-variant/60'
          }`;
          return to ? (
            <Link key={label} to={to} className={cls}>
              {content}
            </Link>
          ) : (
            <button key={label} type="button" disabled title="Coming soon" className={`${cls} cursor-not-allowed`}>
              {content}
            </button>
          );
        })}
      </nav>
    </div>
  );
}
