// Hand-rolled i18n — zero deps, matching the project's minimal-dependency ethos.
// Two locales (default Thai, English fallback), preference persisted per-browser
// in localStorage. t('a.b.c', { var }) resolves a dotted key against the current
// dictionary, falls back to English, then to the raw key, and interpolates
// {var} placeholders. Only static UI chrome is translated — server-generated
// strings (AI factors, disease names, activity-log text) stay as received.

import { createContext, useContext, useState, useCallback, useEffect } from 'react';
import en from './en.json';
import th from './th.json';

const DICTS = { en, th };
export const LANGS = ['th', 'en'];
export const LANG_LABEL = { th: 'ไทย', en: 'EN' };
const STORAGE_KEY = 'smartfarm.lang';
const DEFAULT_LANG = 'th';

const I18nContext = createContext(null);

function getInitialLang() {
  try {
    const saved = localStorage.getItem(STORAGE_KEY);
    if (saved && LANGS.includes(saved)) return saved;
  } catch {
    /* localStorage unavailable — fall through to default */
  }
  return DEFAULT_LANG;
}

function lookup(dict, key) {
  return key.split('.').reduce((o, k) => (o == null ? undefined : o[k]), dict);
}

function interpolate(str, vars) {
  if (!vars) return str;
  return str.replace(/\{(\w+)\}/g, (m, k) => (vars[k] != null ? String(vars[k]) : m));
}

export function I18nProvider({ children }) {
  const [lang, setLangState] = useState(getInitialLang);

  useEffect(() => {
    document.documentElement.lang = lang;
    try {
      localStorage.setItem(STORAGE_KEY, lang);
    } catch {
      /* ignore persistence failure */
    }
  }, [lang]);

  const setLang = useCallback((next) => {
    if (LANGS.includes(next)) setLangState(next);
  }, []);

  const t = useCallback(
    (key, vars) => {
      const val = lookup(DICTS[lang], key) ?? lookup(DICTS.en, key) ?? key;
      return typeof val === 'string' ? interpolate(val, vars) : key;
    },
    [lang],
  );

  return (
    <I18nContext.Provider value={{ lang, setLang, t }}>{children}</I18nContext.Provider>
  );
}

export function useI18n() {
  const ctx = useContext(I18nContext);
  if (!ctx) throw new Error('useI18n must be used within <I18nProvider>');
  return ctx;
}

// Convenience: components that only need the translator.
export function useT() {
  return useI18n().t;
}
