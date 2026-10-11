import React from 'react';
import { FileCode, AlertTriangle, Lock } from 'lucide-react';
import { EXAMPLE_PROGRAMS } from '../../features/debugging/examples';

interface CodeEditorProps {
  source: string;
  onChange: (val: string) => void;
  language: string;
  onLanguageChange: (lang: string) => void;
  optimization: string;
  onOptimizationChange: (opt: string) => void;
  debugSymbols: boolean;
  onDebugSymbolsChange: (dbg: boolean) => void;
  compilationError: string | null;
  compilationExitCode: number | null;
  isSessionActive: boolean;
  onSelectExample: (id: string) => void;
}

export const CodeEditor: React.FC<CodeEditorProps> = ({
  source,
  onChange,
  language,
  onLanguageChange,
  optimization,
  onOptimizationChange,
  debugSymbols,
  onDebugSymbolsChange,
  compilationError,
  compilationExitCode,
  isSessionActive,
  onSelectExample,
}) => {
  const lineCount = source.split('\n').length;

  return (
    <div className="flex flex-col h-full bg-slate-900 border border-slate-800 rounded-lg overflow-hidden shadow-sm">
      {/* Editor Header Toolbar */}
      <div className="bg-slate-950/80 px-3 py-2 border-b border-slate-800 flex flex-wrap items-center justify-between gap-3 text-xs">
        <div className="flex items-center gap-2">
          <FileCode className="w-4 h-4 text-indigo-400" />
          <span className="font-semibold text-slate-200">Source Editor</span>

          {/* Example Program Selector */}
          <select
            disabled={isSessionActive}
            onChange={(e) => onSelectExample(e.target.value)}
            className="bg-slate-900 text-slate-300 border border-slate-700 rounded px-2 py-1 text-xs focus:outline-none focus:border-indigo-500 cursor-pointer disabled:opacity-50"
          >
            <option value="">-- Load Example Program --</option>
            {EXAMPLE_PROGRAMS.map((ex) => (
              <option key={ex.id} value={ex.id}>
                {ex.name}
              </option>
            ))}
          </select>
        </div>

        {/* Build Options */}
        <div className="flex items-center gap-3">
          <div className="flex items-center gap-1.5">
            <span className="text-slate-500 text-[11px]">Lang:</span>
            <select
              disabled={isSessionActive}
              value={language}
              onChange={(e) => onLanguageChange(e.target.value)}
              className="bg-slate-900 text-slate-300 border border-slate-700 rounded px-1.5 py-0.5 text-xs cursor-pointer disabled:opacity-50"
            >
              <option value="c">C (gcc)</option>
              <option value="cpp">C++ (g++)</option>
            </select>
          </div>

          <div className="flex items-center gap-1.5">
            <span className="text-slate-500 text-[11px]">Opt:</span>
            <select
              disabled={isSessionActive}
              value={optimization}
              onChange={(e) => onOptimizationChange(e.target.value)}
              className="bg-slate-900 text-slate-300 border border-slate-700 rounded px-1.5 py-0.5 text-xs cursor-pointer disabled:opacity-50"
            >
              <option value="O0">-O0 (Debug)</option>
              <option value="O1">-O1</option>
              <option value="O2">-O2</option>
            </select>
          </div>

          <label className="flex items-center gap-1 text-[11px] text-slate-400 cursor-pointer">
            <input
              type="checkbox"
              disabled={isSessionActive}
              checked={debugSymbols}
              onChange={(e) => onDebugSymbolsChange(e.target.checked)}
              className="rounded bg-slate-900 border-slate-700 text-indigo-600 focus:ring-0"
            />
            <span>-g</span>
          </label>

          {isSessionActive && (
            <span className="flex items-center gap-1 text-[11px] text-amber-400 font-mono bg-amber-950/40 px-2 py-0.5 rounded border border-amber-800/40">
              <Lock className="w-3 h-3" />
              Locked (Traced)
            </span>
          )}
        </div>
      </div>

      {/* Compilation Error Banner */}
      {compilationError && (
        <div className="bg-rose-950/80 border-b border-rose-800/60 p-3 text-xs text-rose-200 flex items-start gap-2.5 overflow-x-auto">
          <AlertTriangle className="w-4 h-4 text-rose-400 shrink-0 mt-0.5" />
          <div className="flex-1">
            <div className="font-semibold text-rose-300 mb-1">
              Compilation Failed (exit status {compilationExitCode ?? 1})
            </div>
            <pre className="font-mono text-[11px] text-rose-200/90 whitespace-pre-wrap">
              {compilationError}
            </pre>
          </div>
        </div>
      )}

      {/* Code Editor Body */}
      <div className="flex-1 flex overflow-hidden relative">
        {/* Line Numbers */}
        <div className="w-12 bg-slate-950/60 border-r border-slate-800/80 select-none py-3 px-2 text-right font-mono text-xs text-slate-600 shrink-0">
          {Array.from({ length: Math.max(lineCount, 15) }, (_, i) => (
            <div key={i + 1} className="leading-6">
              {i + 1}
            </div>
          ))}
        </div>

        {/* Text Area */}
        <textarea
          value={source}
          onChange={(e) => onChange(e.target.value)}
          disabled={isSessionActive}
          placeholder="// Enter C or C++ source code here..."
          spellCheck={false}
          className="flex-1 bg-transparent text-slate-100 font-mono text-xs leading-6 p-3 resize-none focus:outline-none focus:ring-0 border-0 disabled:opacity-85 whitespace-pre overflow-auto"
        />
      </div>
    </div>
  );
};
