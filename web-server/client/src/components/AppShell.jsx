import { Link, useLocation } from "react-router-dom";
import {
  Cpu,
  SlidersHorizontal,
  LayoutDashboard,
  Video,
  Droplet,
  BrainCircuit,
} from "lucide-react";
import { COPYRIGHT } from "../lib/buildInfo";
import { useI18n, LANGS, LANG_LABEL } from "../i18n";

// Routable pages have a `to`; any entry without one renders as visible-but-inert
// chrome until its page is implemented. `tKey` indexes the i18n dictionary.
const NAV = [
  { tKey: "nav.dashboard", Icon: LayoutDashboard, to: "/" },
  { tKey: "nav.cameras", Icon: Video, to: "/cameras" },
  { tKey: "nav.irrigation", Icon: Droplet, to: "/irrigation" },
  { tKey: "nav.insights", Icon: BrainCircuit, to: "/insights" },
];

function LanguageToggle() {
  const { lang, setLang, t } = useI18n();
  return (
    <div
      role="group"
      aria-label={t("common.language")}
      className="flex items-center rounded-full border border-outline-variant overflow-hidden"
    >
      {LANGS.map((l) => (
        <button
          key={l}
          type="button"
          onClick={() => setLang(l)}
          aria-pressed={lang === l}
          className={`px-2.5 py-1 font-label-caps text-[13px] transition-colors ${
            lang === l
              ? "bg-primary/15 text-primary"
              : "text-on-surface-variant hover:text-on-surface"
          }`}
        >
          {LANG_LABEL[l]}
        </button>
      ))}
    </div>
  );
}

export default function AppShell({ children }) {
  const { pathname } = useLocation();
  const { t } = useI18n();
  const isActive = (to) =>
    to === "/" ? pathname === "/" : pathname.startsWith(to);
  const settingsActive = pathname.startsWith("/settings");

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
          {NAV.map(({ tKey, to }) => {
            if (!to) {
              return (
                <button
                  key={tKey}
                  type="button"
                  disabled
                  title={t("nav.comingSoon")}
                  className="font-label-caps text-label-caps text-on-surface-variant/60 cursor-not-allowed"
                >
                  {t(tKey)}
                </button>
              );
            }
            const active = isActive(to);
            return (
              <Link
                key={tKey}
                to={to}
                className={`font-label-caps text-label-caps px-3 py-2 transition-colors ${
                  active
                    ? "text-primary font-bold"
                    : "text-on-surface-variant hover:text-primary"
                }`}
              >
                {t(tKey)}
              </Link>
            );
          })}
        </nav>

        <div className="flex items-center gap-2">
          <LanguageToggle />
          <Link
            to="/settings"
            title={t("nav.settings")}
            className={`p-2 rounded-full transition-transform active:scale-95 ${
              settingsActive
                ? "text-primary bg-primary/10"
                : "text-on-surface-variant hover:bg-surface-container-high"
            }`}
          >
            <SlidersHorizontal size={20} />
          </Link>
        </div>
      </header>

      <main className="max-w-container-max mx-auto p-margin-mobile md:p-margin-desktop pb-24 md:pb-8">
        {children}
        <footer className="mt-12 pt-4 border-t border-outline-variant/40 text-center">
          <p className="font-data-mono text-[12px] text-on-surface-variant/70">
            {COPYRIGHT}
          </p>
        </footer>
      </main>

      {/* Mobile bottom nav. */}
      <nav className="md:hidden fixed bottom-0 z-50 w-full h-16 px-2 bg-surface-container border-t border-outline-variant flex justify-around items-center">
        {NAV.map(({ tKey, Icon, to }) => {
          const active = to && isActive(to);
          const content = (
            <>
              <Icon size={22} />
              <span className="font-label-caps text-label-caps">{t(tKey)}</span>
            </>
          );
          const cls = `flex flex-col items-center justify-center ${
            active ? "text-primary font-bold" : "text-on-surface-variant/60"
          }`;
          return to ? (
            <Link key={tKey} to={to} className={cls}>
              {content}
            </Link>
          ) : (
            <button
              key={tKey}
              type="button"
              disabled
              title={t("nav.comingSoon")}
              className={`${cls} cursor-not-allowed`}
            >
              {content}
            </button>
          );
        })}
      </nav>
    </div>
  );
}
