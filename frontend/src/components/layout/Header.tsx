import React from 'react';
import { Cpu, Terminal, History, Disc } from 'lucide-react';
import { SessionState } from '../../types/debugger';

interface HeaderProps {
  backendConnected: boolean;
  architecture: string;
  sessionId: string | null;
  pid: number | null;
  state: SessionState;
  selectedSequence: number;
  latestSequence: number;
  isHistoricalMode: boolean;
}

export const Header: React.FC<HeaderProps> = ({
  backendConnected,
  architecture,
  sessionId,
  pid,
  state,
  selectedSequence,
  latestSequence,
  isHistoricalMode,
}) => {
  const getStateBadge = (st: SessionState) => {
    switch (st) {
      case 'running':
        return 'bg-emerald-500/20 text-emerald-400 border-emerald-500/40 animate-pulse';
      case 'paused':
        return 'bg-amber-500/20 text-amber-400 border-amber-500/40';
      case 'compiling':
        return 'bg-blue-500/20 text-blue-400 border-blue-500/40 animate-pulse';
      case 'starting':
        return 'bg-indigo-500/20 text-indigo-400 border-indigo-500/40';
      case 'exited':
        return 'bg-purple-500/20 text-purple-400 border-purple-500/40';
      case 'failed':
        return 'bg-rose-500/20 text-rose-400 border-rose-500/40';
      default:
        return 'bg-slate-800 text-slate-400 border-slate-700';
    }
  };

  return (
    <header className="bg-slate-900 border-b border-slate-800 px-4 py-2.5 flex flex-wrap items-center justify-between gap-4 select-none">
      {/* Title & Brand */}
      <div className="flex items-center gap-3">
        <div className="w-8 h-8 rounded-lg bg-indigo-600/20 border border-indigo-500/40 flex items-center justify-center text-indigo-400">
          <Terminal className="w-5 h-5" />
        </div>
        <div>
          <div className="flex items-center gap-2">
            <h1 className="text-sm font-bold text-white tracking-wide">
              Linux Process Time-Travel Debugger
            </h1>
            <span className="text-[10px] px-1.5 py-0.5 rounded bg-indigo-950 text-indigo-400 border border-indigo-800 font-mono">
              v1.0.0
            </span>
          </div>
          <p className="text-[11px] text-slate-400 flex items-center gap-1.5">
            <Cpu className="w-3 h-3 text-slate-400" />
            <span>Target: {architecture || 'Linux x86_64'} (Native ptrace)</span>
          </p>
        </div>
      </div>

      {/* Center Telemetry & Mode Badge */}
      <div className="flex items-center gap-3">
        {isHistoricalMode ? (
          <div className="flex items-center gap-2 px-3 py-1 rounded-full bg-amber-950/60 border border-amber-500/50 text-amber-300 text-xs shadow-sm">
            <History className="w-3.5 h-3.5 animate-spin" style={{ animationDuration: '6s' }} />
            <span className="font-semibold">Historical Inspection</span>
            <span className="font-mono bg-amber-900/50 px-1.5 py-0.2 rounded text-[11px]">
              Seq #{selectedSequence} of {latestSequence}
            </span>
          </div>
        ) : (
          <div className="flex items-center gap-2 px-3 py-1 rounded-full bg-emerald-950/60 border border-emerald-500/40 text-emerald-300 text-xs">
            <Disc className="w-3.5 h-3.5 text-emerald-400" />
            <span className="font-semibold">Live Process Mode</span>
            {latestSequence > 0 && (
              <span className="font-mono bg-emerald-900/50 px-1.5 py-0.2 rounded text-[11px]">
                Seq #{latestSequence}
              </span>
            )}
          </div>
        )}

        {sessionId && (
          <div className="hidden md:flex items-center gap-2 text-xs font-mono bg-slate-950/80 px-2.5 py-1 rounded border border-slate-800 text-slate-300">
            <span className="text-slate-500">PID:</span>
            <span className="text-indigo-400 font-bold">{pid ?? '—'}</span>
          </div>
        )}
      </div>

      {/* Right Controls: State & Backend Connection */}
      <div className="flex items-center gap-2.5">
        <div
          className={`px-2.5 py-1 rounded text-xs font-mono font-medium border uppercase tracking-wider ${getStateBadge(
            state
          )}`}
        >
          {state}
        </div>

        <div className="flex items-center gap-1.5 px-2.5 py-1 rounded bg-slate-950 border border-slate-800 text-xs">
          <span
            className={`w-2 h-2 rounded-full ${
              backendConnected ? 'bg-emerald-500 shadow-[0_0_8px_rgba(16,185,129,0.6)]' : 'bg-rose-500'
            }`}
          />
          <span className="text-slate-300 text-[11px]">
            {backendConnected ? 'Backend Connected' : 'Disconnected'}
          </span>
        </div>
      </div>
    </header>
  );
};
