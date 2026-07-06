import { Link } from 'react-router-dom';
import { Bug, ScanSearch, ArrowRight } from 'lucide-react';
import { useGetDiseaseQuery, useAnalyzeDiseaseMutation } from './diseaseApi';
import { diseaseMeta } from './diseaseMeta';

// Compact disease summary for the dashboard: latest headline + on-demand trigger.
export default function DiseaseCard() {
  const { data } = useGetDiseaseQuery();
  const [analyze, { isLoading }] = useAnalyzeDiseaseMutation();
  const status = data?.status ?? 'idle';
  const m = diseaseMeta(status);
  const busy = isLoading || data?.analyzing;

  return (
    <div className="panel p-6 flex flex-col justify-between min-h-[200px]">
      <div>
        <div className="flex items-center justify-between mb-4">
          <span className="font-label-caps text-label-caps text-on-surface-variant flex items-center gap-2">
            <Bug size={14} /> Disease Detection
          </span>
          <span className="font-label-caps text-label-caps text-primary/70">PLANTVILLAGE</span>
        </div>
        <div className={`inline-flex items-center gap-2 px-3 py-1.5 rounded border ${m.bg} ${m.border}`}>
          <span className={`w-2.5 h-2.5 rounded-full ${m.dot}`} />
          <span className={`font-body-md text-sm ${m.text}`}>{data?.headline ?? 'Not analyzed yet'}</span>
        </div>
      </div>

      <div className="mt-4 grid grid-cols-2 gap-3">
        <button
          type="button"
          onClick={() => analyze()}
          disabled={busy}
          className="inline-flex items-center justify-center gap-2 bg-primary text-on-primary px-3 py-2 rounded font-label-caps text-label-caps hover:brightness-110 disabled:opacity-50"
        >
          <ScanSearch size={15} />
          {busy ? 'Analyzing…' : 'Analyze'}
        </button>
        <Link
          to="/insights"
          className="inline-flex items-center justify-center gap-2 bg-surface-container-high border border-outline-variant text-on-surface px-3 py-2 rounded font-label-caps text-label-caps hover:bg-surface-container-highest"
        >
          Details
          <ArrowRight size={15} className="text-on-surface-variant" />
        </Link>
      </div>
    </div>
  );
}
