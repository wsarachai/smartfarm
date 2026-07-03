import { Cpu, SlidersHorizontal, LayoutDashboard, Video, Droplet, BrainCircuit } from 'lucide-react';

// The nav anticipates the other wireframes (Cameras / Irrigation / AI Analytics).
// Only DASHBOARD is wired for now; the rest are visible-but-inert chrome.
const NAV = [
  { label: 'DASHBOARD', Icon: LayoutDashboard, active: true },
  { label: 'CAMERAS', Icon: Video },
  { label: 'IRRIGATION', Icon: Droplet },
  { label: 'AI ANALYTICS', Icon: BrainCircuit },
];

export default function AppShell({ children }) {
  return (
    <div className="min-h-screen">
      <header className="sticky top-0 z-50 w-full bg-background border-b border-outline-variant flex items-center justify-between px-margin-mobile py-unit md:px-margin-desktop">
        <div className="flex items-center gap-3">
          <Cpu className="text-primary" size={22} />
          <h1 className="font-headline-sm text-headline-sm font-bold text-primary tracking-tight">
            SMART FARM CONTROL CENTER
          </h1>
        </div>

        <nav className="hidden md:flex items-center gap-6">
          {NAV.map(({ label, active }) =>
            active ? (
              <a
                key={label}
                href="#"
                className="font-label-caps text-label-caps text-primary font-bold px-3 py-2"
              >
                {label}
              </a>
            ) : (
              <button
                key={label}
                type="button"
                disabled
                title="Coming soon"
                className="font-label-caps text-label-caps text-on-surface-variant/60 cursor-not-allowed"
              >
                {label}
              </button>
            )
          )}
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

      {/* Mobile bottom nav (same inert semantics). */}
      <nav className="md:hidden fixed bottom-0 z-50 w-full h-16 px-2 bg-surface-container border-t border-outline-variant flex justify-around items-center">
        {NAV.map(({ label, Icon, active }) => (
          <button
            key={label}
            type="button"
            disabled={!active}
            title={active ? undefined : 'Coming soon'}
            className={`flex flex-col items-center justify-center ${
              active ? 'text-primary font-bold' : 'text-on-surface-variant/60 cursor-not-allowed'
            }`}
          >
            <Icon size={22} />
            <span className="font-label-caps text-label-caps">{label}</span>
          </button>
        ))}
      </nav>
    </div>
  );
}
