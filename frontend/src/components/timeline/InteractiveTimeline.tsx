import React from 'react';
import { Flag, Zap } from 'lucide-react';
import { TraceEvent, Checkpoint } from '../../types/debugger';

interface InteractiveTimelineProps {
  events: TraceEvent[];
  checkpoints: Checkpoint[];
  selectedSequence: number;
  latestSequence: number;
  onSelectSequence: (seq: number) => void;
}

export const InteractiveTimeline: React.FC<InteractiveTimelineProps> = ({
  events,
  checkpoints,
  selectedSequence,
  latestSequence,
  onSelectSequence,
}) => {
  const ckptMap = new Map<number, Checkpoint>();
  checkpoints.forEach((c) => ckptMap.set(c.sequence, c));

  // If there are many events, slice recent 60 around selected or latest
  const displayEvents = events.slice(-60);

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-lg p-3 flex flex-col gap-2 shadow-sm">
      <div className="flex items-center justify-between text-xs">
        <div className="flex items-center gap-2">
          <Zap className="w-3.5 h-3.5 text-amber-400" />
          <span className="font-semibold text-slate-200">Execution Timeline</span>
          <span className="text-[11px] text-slate-500 font-mono">
            ({events.length} recorded events, {checkpoints.length} checkpoints)
          </span>
        </div>

        {/* Direct Jumper */}
        {latestSequence > 1 && (
          <div className="flex items-center gap-2">
            <span className="text-slate-500 text-[11px]">Jump to Seq:</span>
            <input
              type="number"
              min={1}
              max={latestSequence}
              value={selectedSequence || latestSequence}
              onChange={(e) => {
                const val = parseInt(e.target.value, 10);
                if (!isNaN(val) && val >= 1 && val <= latestSequence) {
                  onSelectSequence(val);
                }
              }}
              className="w-16 bg-slate-950 border border-slate-700 rounded px-1.5 py-0.5 text-xs text-amber-300 font-mono text-center focus:outline-none focus:border-amber-500"
            />
          </div>
        )}
      </div>

      {/* Scrubber Range Slider */}
      {latestSequence > 1 && (
        <div className="px-1 py-1">
          <input
            type="range"
            min={1}
            max={latestSequence}
            value={selectedSequence || latestSequence}
            onChange={(e) => onSelectSequence(parseInt(e.target.value, 10))}
            className="w-full accent-amber-500 cursor-pointer h-1.5 bg-slate-800 rounded-lg appearance-none"
          />
        </div>
      )}

      {/* Visual Sequence Track */}
      <div className="relative overflow-x-auto py-2 flex items-center gap-1.5 scrollbar-thin">
        {events.length === 0 ? (
          <div className="text-xs text-slate-600 italic py-2 px-1">
            Timeline will populate as process steps execute...
          </div>
        ) : (
          displayEvents.map((ev) => {
            const isSelected = ev.sequence === selectedSequence;
            const hasCkpt = ckptMap.has(ev.sequence);
            const isExit =
              ev.type === 'process_exited' || ev.type === 'process_terminated_by_signal';

            let bgColor = 'bg-slate-800 border-slate-700 text-slate-400';
            if (isSelected) {
              bgColor = 'bg-amber-500/20 border-amber-400 text-amber-300 shadow-[0_0_10px_rgba(245,158,11,0.3)] ring-1 ring-amber-400';
            } else if (hasCkpt) {
              bgColor = 'bg-cyan-950/60 border-cyan-500/60 text-cyan-300';
            } else if (isExit) {
              bgColor = 'bg-purple-950/60 border-purple-500/60 text-purple-300';
            }

            return (
              <button
                key={ev.sequence}
                onClick={() => onSelectSequence(ev.sequence)}
                className={`flex flex-col items-center justify-center min-w-[42px] px-1 py-1.5 rounded border text-[11px] font-mono transition-all cursor-pointer relative shrink-0 ${bgColor}`}
                title={`Seq #${ev.sequence}: ${ev.type}\n${ev.description}${hasCkpt ? '\n[Checkpoint Captured]' : ''}`}
              >
                {/* Checkpoint Flag Marker */}
                {hasCkpt && (
                  <span className="absolute -top-1.5 -right-1 text-cyan-400">
                    <Flag className="w-2.5 h-2.5 fill-cyan-400" />
                  </span>
                )}

                <span className="font-bold text-[10px]">#{ev.sequence}</span>
                <span className="text-[9px] truncate max-w-[38px] opacity-80">
                  {hasCkpt ? 'CKPT' : isExit ? 'EXIT' : 'STEP'}
                </span>

                {/* Current Playhead Indicator */}
                {isSelected && (
                  <div className="w-1.5 h-1.5 rounded-full bg-amber-400 mt-0.5 animate-ping" />
                )}
              </button>
            );
          })
        )}
      </div>

      {/* Legend */}
      <div className="flex flex-wrap items-center gap-4 text-[10px] text-slate-500 pt-1 border-t border-slate-800/60">
        <div className="flex items-center gap-1">
          <span className="w-2 h-2 rounded-full bg-amber-400" />
          <span>Selected Playhead</span>
        </div>
        <div className="flex items-center gap-1">
          <span className="w-2 h-2 rounded-full bg-cyan-400" />
          <span>Checkpoint (Full State)</span>
        </div>
        <div className="flex items-center gap-1">
          <span className="w-2 h-2 rounded-full bg-slate-700" />
          <span>Step Event (Register Snapshot)</span>
        </div>
        <div className="flex items-center gap-1">
          <span className="w-2 h-2 rounded-full bg-purple-400" />
          <span>Exit / Termination</span>
        </div>
      </div>
    </div>
  );
};
