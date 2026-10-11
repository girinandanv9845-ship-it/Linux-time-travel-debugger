import React, { useState } from 'react';
import { Terminal, Copy, Check, Trash2 } from 'lucide-react';

interface ProgramOutputProps {
  stdout: string;
  stderr: string;
  onClear: () => void;
}

export const ProgramOutput: React.FC<ProgramOutputProps> = ({
  stdout,
  stderr,
  onClear,
}) => {
  const [copied, setCopied] = useState<boolean>(false);
  const [activeTab, setActiveTab] = useState<'all' | 'stdout' | 'stderr'>('all');

  const copyOutput = () => {
    const text = `--- STDOUT ---\n${stdout}\n--- STDERR ---\n${stderr}`;
    navigator.clipboard.writeText(text);
    setCopied(true);
    setTimeout(() => setCopied(false), 1500);
  };

  const hasContent = stdout.trim() || stderr.trim();

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-lg flex flex-col h-full shadow-sm overflow-hidden">
      {/* Header */}
      <div className="bg-slate-950/80 px-3 py-2 border-b border-slate-800 flex items-center justify-between text-xs">
        <div className="flex items-center gap-2">
          <Terminal className="w-4 h-4 text-emerald-400" />
          <span className="font-semibold text-slate-200">Program Console (I/O)</span>

          {/* Filter Tabs */}
          <div className="flex items-center gap-1 ml-2">
            <button
              onClick={() => setActiveTab('all')}
              className={`px-2 py-0.5 rounded text-[11px] cursor-pointer ${
                activeTab === 'all'
                  ? 'bg-slate-800 text-white font-medium'
                  : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              All
            </button>
            <button
              onClick={() => setActiveTab('stdout')}
              className={`px-2 py-0.5 rounded text-[11px] cursor-pointer ${
                activeTab === 'stdout'
                  ? 'bg-slate-800 text-emerald-300 font-medium'
                  : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              stdout
            </button>
            <button
              onClick={() => setActiveTab('stderr')}
              className={`px-2 py-0.5 rounded text-[11px] cursor-pointer ${
                activeTab === 'stderr'
                  ? 'bg-slate-800 text-rose-300 font-medium'
                  : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              stderr
            </button>
          </div>
        </div>

        <div className="flex items-center gap-2">
          <button
            onClick={copyOutput}
            disabled={!hasContent}
            className="p-1 text-slate-400 hover:text-slate-200 disabled:opacity-30 cursor-pointer"
            title="Copy console output"
          >
            {copied ? <Check className="w-3.5 h-3.5 text-emerald-400" /> : <Copy className="w-3.5 h-3.5" />}
          </button>
          <button
            onClick={onClear}
            disabled={!hasContent}
            className="p-1 text-slate-400 hover:text-rose-400 disabled:opacity-30 cursor-pointer"
            title="Clear console view"
          >
            <Trash2 className="w-3.5 h-3.5" />
          </button>
        </div>
      </div>

      {/* Console Display */}
      <div className="flex-1 p-3 bg-slate-950 font-mono text-xs overflow-auto select-text">
        {!hasContent ? (
          <div className="text-slate-600 italic">No program output captured yet.</div>
        ) : (
          <div className="flex flex-col gap-1">
            {(activeTab === 'all' || activeTab === 'stdout') && stdout && (
              <div className="text-emerald-300 whitespace-pre-wrap">{stdout}</div>
            )}
            {(activeTab === 'all' || activeTab === 'stderr') && stderr && (
              <div className="text-rose-400 whitespace-pre-wrap">{stderr}</div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};

