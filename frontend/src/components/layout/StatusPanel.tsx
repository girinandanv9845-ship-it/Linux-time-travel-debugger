import React from 'react';
import { Info, AlertTriangle } from 'lucide-react';
import { SessionCurrentState, SeekResult } from '../../types/debugger';

interface StatusPanelProps {
  sessionState: SessionCurrentState | null;
  seekResult: SeekResult | null;
  isHistoricalMode: boolean;
}

export const StatusPanel: React.FC<StatusPanelProps> = ({
  sessionState,
  seekResult,
  isHistoricalMode,
}) => {
  return (
    <div className="bg-slate-900 border border-slate-800 rounded-lg p-3 text-xs flex flex-col gap-2.5 shadow-sm">
      <div className="flex items-center justify-between border-b border-slate-800 pb-2">
        <div className="flex items-center gap-1.5 font-semibold text-slate-200">
          <Info className="w-4 h-4 text-indigo-400" />
          <span>Debugger Telemetry & Time-Travel Guarantees</span>
        </div>
        <div className="text-[11px] text-slate-500 font-mono">
          Session: {sessionState?.sessionId ?? 'None'}
        </div>
      </div>

      {/* Replay Limitation & Truthful Inspection Banner */}
      {isHistoricalMode && seekResult && (
        <div className="p-2.5 rounded bg-amber-950/40 border border-amber-500/40 text-amber-200 flex items-start gap-2">
          <AlertTriangle className="w-4 h-4 text-amber-400 shrink-0 mt-0.5" />
          <div className="flex-1">
            <div className="font-semibold text-amber-300 flex items-center gap-2">
              <span>Historical Inspection Active</span>
              <span className="text-[10px] px-1.5 py-0.2 rounded bg-amber-900/60 font-mono uppercase">
                {seekResult.stateAvailability.replace(/_/g, ' ')}
              </span>
            </div>
            <p className="text-[11px] text-amber-200/90 mt-1 leading-relaxed">
              {seekResult.replayLimitationNotice}
            </p>
          </div>
        </div>
      )}

      {/* Grid of Key Telemetry Stats */}
      <div className="grid grid-cols-2 sm:grid-cols-4 gap-2 font-mono text-[11px]">
        <div className="bg-slate-950/60 p-2 rounded border border-slate-800">
          <div className="text-slate-500 text-[10px]">CURRENT SEQUENCE</div>
          <div className="text-amber-400 font-bold text-sm mt-0.5">
            #{sessionState?.selectedSequence || sessionState?.latestSequence || 0}
          </div>
        </div>

        <div className="bg-slate-950/60 p-2 rounded border border-slate-800">
          <div className="text-slate-500 text-[10px]">RECORDED EVENTS</div>
          <div className="text-indigo-400 font-bold text-sm mt-0.5">
            {sessionState?.totalEvents ?? 0}
          </div>
        </div>

        <div className="bg-slate-950/60 p-2 rounded border border-slate-800">
          <div className="text-slate-500 text-[10px]">CHECKPOINTS</div>
          <div className="text-cyan-400 font-bold text-sm mt-0.5">
            {sessionState?.checkpointsCount ?? 0}
          </div>
        </div>

        <div className="bg-slate-950/60 p-2 rounded border border-slate-800">
          <div className="text-slate-500 text-[10px]">PROCESS PID</div>
          <div className="text-emerald-400 font-bold text-sm mt-0.5">
            {sessionState?.pid ?? '—'}
          </div>
        </div>
      </div>
    </div>
  );
};
