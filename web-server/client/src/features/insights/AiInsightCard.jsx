import { Link } from 'react-router-dom';
import { Sparkles, BrainCircuit } from 'lucide-react';

// AI Analytics now exists as a page, but it's driven by a backend SIMULATOR (no
// real inference engine). Kept in the tertiary/gold family and labelled SIMULATED
// so it never reads as a real, live system; links through to the full page.
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
            SIMULATED
          </span>
        </div>
        <h3 className="font-headline-md text-headline-md font-bold mb-4">Inference telemetry live</h3>
        <p className="font-body-md text-sm mb-6">
          A simulated inference engine is streaming demo confidence, latency and GPU
          telemetry. Open the AI Analytics page to watch the real-time trend — every
          value is fabricated for the demo, not from a real model.
        </p>
      </div>
      <Link
        to="/analytics"
        title="Open AI Analytics"
        className="bg-on-tertiary/10 border border-on-tertiary/20 p-3 hover:bg-on-tertiary/20 transition-colors active:scale-95"
      >
        <div className="flex items-center gap-3">
          <Sparkles size={18} />
          <span className="font-data-mono text-xs font-bold">VIEW AI ANALYTICS →</span>
        </div>
      </Link>
    </div>
  );
}
