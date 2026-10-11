import React, { useState } from 'react';
import { ListFilter, Search } from 'lucide-react';
import { TraceEvent } from '../../types/debugger';

interface EventLogProps {
  events: TraceEvent[];
  selectedSequence: number;
  onSelectEvent: (sequence: number) => void;
}

export const EventLog: React.FC<EventLogProps> = ({
  events,
  selectedSequence,
  onSelectEvent,
}) => {
  const [searchTerm, setSearchTerm] = useState<string>('');
  const [filterType, setFilterType] = useState<string>('all');

  const filteredEvents = events.filter((ev) => {
    if (filterType !== 'all' && ev.type !== filterType) return false;
    if (searchTerm.trim()) {
      const term = searchTerm.toLowerCase();
      return (
        ev.description.toLowerCase().includes(term) ||
        ev.sequence.toString().includes(term) ||
        ev.type.toLowerCase().includes(term) ||
        ev.rip.toLowerCase().includes(term)
      );
    }
    return true;
  });

  const getBadgeStyle = (type: string) => {
    switch (type) {
      case 'checkpoint_captured':
        return 'bg-cyan-950 text-cyan-300 border-cyan-800';
      case 'single_step_completed':
        return 'bg-slate-800 text-slate-300 border-slate-700';
      case 'signal_observed':
        return 'bg-amber-950 text-amber-300 border-amber-800';
      case 'process_exited':
        return 'bg-purple-950 text-purple-300 border-purple-800';
      case 'process_terminated_by_signal':
        return 'bg-rose-950 text-rose-300 border-rose-800';
      default:
        return 'bg-indigo-950 text-indigo-300 border-indigo-800';
    }
  };

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-lg flex flex-col h-full shadow-sm overflow-hidden">
      {/* Header & Filter Controls */}
      <div className="bg-slate-950/80 px-3 py-2 border-b border-slate-800 flex flex-wrap items-center justify-between gap-2 text-xs">
        <div className="flex items-center gap-2">
          <ListFilter className="w-4 h-4 text-indigo-400" />
          <span className="font-semibold text-slate-200">Execution Event Log</span>
          <span className="text-[11px] text-slate-500 font-mono">({filteredEvents.length} shown)</span>
        </div>

        <div className="flex items-center gap-2">
          {/* Search */}
          <div className="flex items-center gap-1 bg-slate-900 border border-slate-700 rounded px-2 py-0.5">
            <Search className="w-3 h-3 text-slate-500" />
            <input
              type="text"
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              placeholder="Search events..."
              className="w-24 md:w-32 bg-transparent text-slate-200 text-xs focus:outline-none"
            />
          </div>

          {/* Type Filter */}
          <select
            value={filterType}
            onChange={(e) => setFilterType(e.target.value)}
            className="bg-slate-900 text-slate-300 border border-slate-700 rounded px-2 py-0.5 text-xs focus:outline-none cursor-pointer"
          >
            <option value="all">All Events</option>
            <option value="single_step_completed">Single Steps</option>
            <option value="checkpoint_captured">Checkpoints</option>
            <option value="signal_observed">Signals</option>
            <option value="process_exited">Process Exit</option>
          </select>
        </div>
      </div>

      {/* Events Table */}
      <div className="flex-1 overflow-auto text-xs">
        <table className="w-full text-left border-collapse">
          <thead className="bg-slate-950/60 sticky top-0 border-b border-slate-800 text-slate-400 font-mono text-[11px]">
            <tr>
              <th className="py-1.5 px-3 w-16">Seq</th>
              <th className="py-1.5 px-3 w-36">Event Type</th>
              <th className="py-1.5 px-3 w-40">Instruction Pointer (RIP)</th>
              <th className="py-1.5 px-3">Description</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-850 font-mono text-[11px]">
            {filteredEvents.length === 0 ? (
              <tr>
                <td colSpan={4} className="py-8 text-center text-slate-600 italic">
                  No recorded events match the filter criteria.
                </td>
              </tr>
            ) : (
              filteredEvents.map((ev) => {
                const isSelected = ev.sequence === selectedSequence;
                return (
                  <tr
                    key={ev.sequence}
                    onClick={() => onSelectEvent(ev.sequence)}
                    className={`cursor-pointer transition-colors ${
                      isSelected
                        ? 'bg-amber-950/40 text-amber-200'
                        : 'hover:bg-slate-800/60 text-slate-300'
                    }`}
                  >
                    <td className="py-1.5 px-3 font-bold text-slate-400">
                      #{ev.sequence}
                    </td>
                    <td className="py-1.5 px-3">
                      <span
                        className={`px-1.5 py-0.5 rounded border text-[10px] ${getBadgeStyle(
                          ev.type
                        )}`}
                      >
                        {ev.type.replace(/_/g, ' ')}
                      </span>
                    </td>
                    <td className="py-1.5 px-3 text-cyan-300">{ev.rip}</td>
                    <td className="py-1.5 px-3 text-slate-300 truncate max-w-xs md:max-w-md font-sans">
                      {ev.description}
                    </td>
                  </tr>
                );
              })
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
};
