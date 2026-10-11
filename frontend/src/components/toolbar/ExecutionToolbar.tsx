import React from 'react';
import {
  Play,
  Pause,
  StepForward,
  Square,
  ChevronLeft,
  ChevronRight,
  Radio,
  Zap,
} from 'lucide-react';
import { SessionState } from '../../types/debugger';

interface ExecutionToolbarProps {
  state: SessionState;
  isHistoricalMode: boolean;
  selectedSequence: number;
  latestSequence: number;
  onStart: () => void;
  onPause: () => void;
  onResume: () => void;
  onStep: () => void;
  onStop: () => void;
  onSeekPrevious: () => void;
  onSeekNext: () => void;
  onReturnToLive: () => void;
  loading: boolean;
}

export const ExecutionToolbar: React.FC<ExecutionToolbarProps> = ({
  state,
  isHistoricalMode,
  selectedSequence,
  latestSequence,
  onStart,
  onPause,
  onResume,
  onStep,
  onStop,
  onSeekPrevious,
  onSeekNext,
  onReturnToLive,
  loading,
}) => {
  const isRunning = state === 'running';
  const isPaused = state === 'paused';
  const isCompiling = state === 'compiling' || state === 'starting';
  const hasActiveSession = state !== 'created' && state !== 'stopped' && state !== 'failed';

  return (
    <div className="bg-slate-900/90 border-b border-slate-800 px-4 py-2 flex flex-wrap items-center justify-between gap-3 text-sm select-none">
      {/* Live Execution Controls */}
      <div className="flex items-center gap-1.5">
        <span className="text-[11px] font-semibold text-slate-500 uppercase tracking-wider mr-1">
          Execution
        </span>

        {!hasActiveSession ? (
          <button
            onClick={onStart}
            disabled={loading || isCompiling}
            className="flex items-center gap-1.5 px-3 py-1.5 bg-indigo-600 hover:bg-indigo-500 disabled:opacity-50 text-white rounded font-medium text-xs shadow transition-colors cursor-pointer"
            title="Compile and launch target under ptrace"
          >
            <Play className="w-3.5 h-3.5 fill-white" />
            <span>Launch Target</span>
          </button>
        ) : (
          <>
            {isPaused && (
              <button
                onClick={onResume}
                disabled={loading || isHistoricalMode}
                className="flex items-center gap-1.5 px-3 py-1.5 bg-emerald-600 hover:bg-emerald-500 disabled:opacity-40 text-white rounded font-medium text-xs shadow transition-colors cursor-pointer"
                title="Resume execution"
              >
                <Play className="w-3.5 h-3.5 fill-white" />
                <span>Resume</span>
              </button>
            )}

            {isRunning && (
              <button
                onClick={onPause}
                disabled={loading}
                className="flex items-center gap-1.5 px-3 py-1.5 bg-amber-600 hover:bg-amber-500 disabled:opacity-40 text-white rounded font-medium text-xs shadow transition-colors cursor-pointer"
                title="Pause execution"
              >
                <Pause className="w-3.5 h-3.5 fill-white" />
                <span>Pause</span>
              </button>
            )}

            <button
              onClick={onStep}
              disabled={loading || !isPaused || isHistoricalMode}
              className="flex items-center gap-1.5 px-3 py-1.5 bg-slate-800 hover:bg-slate-700 disabled:opacity-40 text-slate-200 rounded font-medium text-xs border border-slate-700 transition-colors cursor-pointer"
              title="Single-step one machine instruction (PTRACE_SINGLESTEP)"
            >
              <StepForward className="w-3.5 h-3.5 text-indigo-400" />
              <span>Single Step</span>
            </button>

            <button
              onClick={onStop}
              disabled={loading}
              className="flex items-center gap-1.5 px-2.5 py-1.5 bg-rose-950/40 hover:bg-rose-900/60 disabled:opacity-40 text-rose-300 rounded font-medium text-xs border border-rose-800/50 transition-colors cursor-pointer ml-1"
              title="Terminate process and cleanup session"
            >
              <Square className="w-3.5 h-3.5 fill-rose-400" />
              <span>Stop</span>
            </button>
          </>
        )}
      </div>

      {/* Historical Navigation Controls */}
      <div className="flex items-center gap-2 border-l border-slate-800 pl-4">
        <span className="text-[11px] font-semibold text-slate-500 uppercase tracking-wider mr-1 flex items-center gap-1">
          <Zap className="w-3 h-3 text-amber-500" />
          Time Travel
        </span>

        <button
          onClick={onSeekPrevious}
          disabled={loading || selectedSequence <= 1 || latestSequence === 0}
          className="flex items-center gap-1 px-2.5 py-1.5 bg-slate-800 hover:bg-slate-700 disabled:opacity-30 text-slate-300 rounded text-xs border border-slate-700 transition-colors cursor-pointer"
          title="Step backward to previous historical event"
        >
          <ChevronLeft className="w-4 h-4 text-amber-400" />
          <span>Previous Event</span>
        </button>

        <div className="px-2 py-1 bg-slate-950 rounded border border-slate-800 font-mono text-xs text-slate-300 flex items-center gap-1">
          <span className="text-slate-500">Seq:</span>
          <span className="text-amber-400 font-bold">{selectedSequence || latestSequence || 0}</span>
          <span className="text-slate-600">/</span>
          <span className="text-slate-400">{latestSequence}</span>
        </div>

        <button
          onClick={onSeekNext}
          disabled={loading || selectedSequence >= latestSequence || latestSequence === 0}
          className="flex items-center gap-1 px-2.5 py-1.5 bg-slate-800 hover:bg-slate-700 disabled:opacity-30 text-slate-300 rounded text-xs border border-slate-700 transition-colors cursor-pointer"
          title="Step forward to next historical event"
        >
          <span>Next Event</span>
          <ChevronRight className="w-4 h-4 text-amber-400" />
        </button>

        {isHistoricalMode && (
          <button
            onClick={onReturnToLive}
            className="flex items-center gap-1 px-2.5 py-1.5 bg-indigo-950/60 hover:bg-indigo-900/80 text-indigo-300 rounded text-xs border border-indigo-700/60 transition-colors cursor-pointer ml-1"
            title="Return playhead to live execution position"
          >
            <Radio className="w-3.5 h-3.5 text-indigo-400 animate-pulse" />
            <span>Return to Live</span>
          </button>
        )}
      </div>
    </div>
  );
};
