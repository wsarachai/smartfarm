import { Sparkles, BrainCircuit } from 'lucide-react';

// Labeled PREVIEW: AI Analytics is a separate wireframe/feature with no backend
// yet. Rendered for layout fidelity but clearly marked as not-live.
export default function AiInsightCard() {
  return (
    <div className="bg-tertiary-container text-on-tertiary-container p-6 relative overflow-hidden flex flex-col justify-between min-h-[220px]">
      <div className="absolute -right-4 -top-4 opacity-10 pointer-events-none">
        <BrainCircuit size={120} />
      </div>
      <div>
        <div className="flex items-center justify-between mb-2">
          <span className="font-label-caps text-label-caps text-on-tertiary-container/70">
            AI ANALYTICS ENGINE
          </span>
          <span className="font-label-caps text-label-caps bg-on-tertiary/10 border border-on-tertiary/20 px-2 py-0.5">
            PREVIEW
          </span>
        </div>
        <h3 className="font-headline-md text-headline-md font-bold mb-4">Insights coming soon</h3>
        <p className="font-body-md text-sm mb-6">
          Irrigation and analytics recommendations will appear here once the AI Analytics
          module is wired up. This panel is a placeholder for that feature.
        </p>
      </div>
      <div className="bg-on-tertiary/10 border border-on-tertiary/20 p-3">
        <div className="flex items-center gap-3">
          <Sparkles size={18} />
          <span className="font-data-mono text-xs font-bold">MODULE STATUS: NOT CONNECTED</span>
        </div>
      </div>
    </div>
  );
}
