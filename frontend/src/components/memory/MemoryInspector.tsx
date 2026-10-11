import React, { useState } from 'react';
import { Database, Search, AlertCircle, RefreshCw } from 'lucide-react';
import { MemoryDump, Registers } from '../../types/debugger';

interface MemoryInspectorProps {
  memoryDump: MemoryDump | null;
  currentRegisters: Registers | null;
  onReadMemory: (address: string, length: number) => void;
  loading: boolean;
  isHistorical: boolean;
}

export const MemoryInspector: React.FC<MemoryInspectorProps> = ({
  memoryDump,
  currentRegisters,
  onReadMemory,
  loading,
  isHistorical,
}) => {
  const [addressInput, setAddressInput] = useState<string>('');
  const [lengthInput, setLengthInput] = useState<number>(64);

  const handleFetch = (addr?: string) => {
    const target = addr || addressInput;
    if (target.trim()) {
      onReadMemory(target.trim(), lengthInput);
    }
  };

  const handleQuickJump = (regVal: string | undefined) => {
    if (regVal) {
      setAddressInput(regVal);
      handleFetch(regVal);
    }
  };

  // Group bytes into 16-byte rows for classic hex viewer
  const rows: Array<{ offset: number; hexBytes: string[]; ascii: string }> = [];
  if (memoryDump && memoryDump.bytes && memoryDump.isAccessible) {
    const bytes = memoryDump.bytes;
    for (let i = 0; i < bytes.length; i += 16) {
      const slice = bytes.slice(i, i + 16);
      const hexBytes = slice.map((b) => b.toString(16).padStart(2, '0'));
      const ascii = slice
        .map((b) => (b >= 32 && b <= 126 ? String.fromCharCode(b) : '.'))
        .join('');
      rows.push({ offset: i, hexBytes, ascii });
    }
  }

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-lg flex flex-col h-full shadow-sm overflow-hidden">
      {/* Header Controls */}
      <div className="bg-slate-950/80 px-3 py-2 border-b border-slate-800 flex flex-wrap items-center justify-between gap-2 text-xs">
        <div className="flex items-center gap-2">
          <Database className="w-4 h-4 text-cyan-400" />
          <span className="font-semibold text-slate-200">Memory Inspector</span>
          {isHistorical && (
            <span className="text-[10px] px-1.5 py-0.5 rounded bg-amber-950 text-amber-400 border border-amber-800">
              Checkpoint Snapshot
            </span>
          )}
        </div>

        {/* Quick Registers Jump Pills */}
        {currentRegisters && (
          <div className="flex items-center gap-1.5 text-[11px] font-mono">
            <span className="text-slate-500">Jump:</span>
            <button
              onClick={() => handleQuickJump(currentRegisters.rsp)}
              className="px-1.5 py-0.5 rounded bg-slate-800 hover:bg-slate-700 text-cyan-300 border border-slate-700 cursor-pointer"
              title={`Stack pointer: ${currentRegisters.rsp}`}
            >
              $rsp
            </button>
            <button
              onClick={() => handleQuickJump(currentRegisters.rbp)}
              className="px-1.5 py-0.5 rounded bg-slate-800 hover:bg-slate-700 text-cyan-300 border border-slate-700 cursor-pointer"
              title={`Base pointer: ${currentRegisters.rbp}`}
            >
              $rbp
            </button>
            <button
              onClick={() => handleQuickJump(currentRegisters.rip)}
              className="px-1.5 py-0.5 rounded bg-slate-800 hover:bg-slate-700 text-cyan-300 border border-slate-700 cursor-pointer"
              title={`Instruction pointer: ${currentRegisters.rip}`}
            >
              $rip
            </button>
          </div>
        )}
      </div>

      {/* Address Query Bar */}
      <div className="p-2 border-b border-slate-800 bg-slate-950/40 flex items-center gap-2 text-xs">
        <div className="flex-1 flex items-center gap-1.5 bg-slate-900 border border-slate-700 rounded px-2 py-1">
          <Search className="w-3.5 h-3.5 text-slate-500" />
          <input
            type="text"
            value={addressInput}
            onChange={(e) => setAddressInput(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && handleFetch()}
            placeholder="Address (e.g. 0x7fffffffe000)"
            className="w-full bg-transparent text-slate-200 font-mono text-xs focus:outline-none"
          />
        </div>

        <select
          value={lengthInput}
          onChange={(e) => setLengthInput(parseInt(e.target.value, 10))}
          className="bg-slate-900 text-slate-300 border border-slate-700 rounded px-2 py-1 text-xs cursor-pointer focus:outline-none"
        >
          <option value={32}>32 Bytes</option>
          <option value={64}>64 Bytes</option>
          <option value={128}>128 Bytes</option>
          <option value={256}>256 Bytes</option>
        </select>

        <button
          onClick={() => handleFetch()}
          disabled={loading || !addressInput.trim()}
          className="px-2.5 py-1 bg-cyan-700 hover:bg-cyan-600 disabled:opacity-40 text-white rounded text-xs font-medium transition-colors cursor-pointer flex items-center gap-1"
        >
          {loading ? <RefreshCw className="w-3.5 h-3.5 animate-spin" /> : 'Inspect'}
        </button>
      </div>

      {/* Memory Content Display */}
      <div className="flex-1 p-3 overflow-auto font-mono text-xs">
        {!memoryDump ? (
          <div className="h-full flex items-center justify-center text-slate-600 italic">
            Enter a virtual memory address or click a register shortcut ($rsp, $rip) to read bytes.
          </div>
        ) : !memoryDump.isAccessible ? (
          <div className="p-3 bg-rose-950/30 border border-rose-800/50 rounded text-rose-300 flex items-start gap-2">
            <AlertCircle className="w-4 h-4 text-rose-400 shrink-0 mt-0.5" />
            <div>
              <p className="font-semibold">Memory Access Inaccessible</p>
              <p className="text-[11px] text-rose-200/80 mt-0.5">{memoryDump.errorMessage}</p>
            </div>
          </div>
        ) : (
          <div className="flex flex-col gap-1.5 select-text">
            {/* Header row */}
            <div className="flex text-slate-500 font-semibold text-[11px] border-b border-slate-800 pb-1">
              <span className="w-40">Address Offset</span>
              <span className="flex-1">Hexadecimal Dump (16-Bytes)</span>
              <span className="w-24 text-right">ASCII</span>
            </div>

            {/* Hex Rows */}
            {rows.map((row) => {
              const baseAddr = BigInt(memoryDump.address || '0');
              const rowAddr = (baseAddr + BigInt(row.offset)).toString(16).padStart(16, '0');

              return (
                <div key={row.offset} className="flex items-center hover:bg-slate-850 py-0.5 rounded">
                  <span className="w-40 text-slate-500 text-[11px]">0x{rowAddr}</span>
                  <div className="flex-1 flex gap-2 font-mono text-slate-300">
                    <span className="flex gap-1.5">
                      {row.hexBytes.slice(0, 8).map((b, bi) => (
                        <span key={bi} className={b === '00' ? 'text-slate-600' : 'text-cyan-300'}>
                          {b}
                        </span>
                      ))}
                    </span>
                    <span className="text-slate-700">|</span>
                    <span className="flex gap-1.5">
                      {row.hexBytes.slice(8, 16).map((b, bi) => (
                        <span key={bi + 8} className={b === '00' ? 'text-slate-600' : 'text-cyan-300'}>
                          {b}
                        </span>
                      ))}
                    </span>
                  </div>
                  <span className="w-24 text-right text-slate-400 tracking-wider">
                    {row.ascii}
                  </span>
                </div>
              );
            })}
          </div>
        )}
      </div>
    </div>
  );
};

