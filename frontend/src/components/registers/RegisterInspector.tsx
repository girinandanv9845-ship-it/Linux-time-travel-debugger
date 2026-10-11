import React, { useState } from 'react';
import { Cpu, Copy, Check } from 'lucide-react';
import { Registers, RegisterDiffItem } from '../../types/debugger';

interface RegisterInspectorProps {
  registers: Registers | null;
  registerDiff?: RegisterDiffItem[];
  isLive: boolean;
  selectedSequence?: number;
  checkpointId?: string;
}

export const RegisterInspector: React.FC<RegisterInspectorProps> = ({
  registers,
  registerDiff = [],
  isLive,
  selectedSequence,
  checkpointId,
}) => {
  const [copiedKey, setCopiedKey] = useState<string | null>(null);

  const diffMap = new Map<string, RegisterDiffItem>();
  registerDiff.forEach((d) => diffMap.set(d.name.toUpperCase(), d));

  const copyToClipboard = (key: string, val: string) => {
    navigator.clipboard.writeText(val);
    setCopiedKey(key);
    setTimeout(() => setCopiedKey(null), 1500);
  };

  if (!registers) {
    return (
      <div className="bg-slate-900 border border-slate-800 rounded-lg p-4 h-full flex flex-col items-center justify-center text-center text-slate-500 text-xs">
        <Cpu className="w-8 h-8 mb-2 text-slate-700" />
        <p className="font-semibold text-slate-400">No Register State Available</p>
        <p className="text-[11px] mt-1 text-slate-600 max-w-[240px]">
          Launch a debugging session and pause or step to inspect architecture registers.
        </p>
      </div>
    );
  }

  const regList: Array<{ name: string; val: string; desc: string }> = [
    { name: 'RIP', val: registers.rip, desc: 'Instruction Pointer' },
    { name: 'RSP', val: registers.rsp, desc: 'Stack Pointer' },
    { name: 'RBP', val: registers.rbp, desc: 'Base Frame Pointer' },
    { name: 'RAX', val: registers.rax, desc: 'Accumulator / Return Value' },
    { name: 'RBX', val: registers.rbx, desc: 'Base Register' },
    { name: 'RCX', val: registers.rcx, desc: 'Counter Register' },
    { name: 'RDX', val: registers.rdx, desc: 'Data Register' },
    { name: 'RSI', val: registers.rsi, desc: 'Source Index' },
    { name: 'RDI', val: registers.rdi, desc: 'Destination Index' },
    { name: 'R8', val: registers.r8, desc: 'General Register 8' },
    { name: 'R9', val: registers.r9, desc: 'General Register 9' },
    { name: 'R10', val: registers.r10, desc: 'General Register 10' },
    { name: 'R11', val: registers.r11, desc: 'General Register 11' },
    { name: 'R12', val: registers.r12, desc: 'General Register 12' },
    { name: 'R13', val: registers.r13, desc: 'General Register 13' },
    { name: 'R14', val: registers.r14, desc: 'General Register 14' },
    { name: 'R15', val: registers.r15, desc: 'General Register 15' },
    { name: 'EFLAGS', val: registers.eflags, desc: 'CPU Status & Control Flags' },
  ];

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-lg flex flex-col h-full shadow-sm overflow-hidden">
      {/* Header */}
      <div className="bg-slate-950/80 px-3 py-2 border-b border-slate-800 flex items-center justify-between text-xs">
        <div className="flex items-center gap-2">
          <Cpu className="w-4 h-4 text-indigo-400" />
          <span className="font-semibold text-slate-200">CPU Registers (x86_64)</span>
        </div>

        <div className="flex items-center gap-2">
          {isLive ? (
            <span className="text-[10px] px-2 py-0.5 rounded-full bg-emerald-950 text-emerald-400 border border-emerald-800 font-mono">
              Live Process State
            </span>
          ) : (
            <span className="text-[10px] px-2 py-0.5 rounded-full bg-amber-950 text-amber-400 border border-amber-800 font-mono">
              Historical Snapshot #{selectedSequence} {checkpointId ? `(${checkpointId})` : ''}
            </span>
          )}
        </div>
      </div>

      {/* Register Grid */}
      <div className="flex-1 p-2.5 overflow-y-auto grid grid-cols-1 sm:grid-cols-2 gap-2 text-xs">
        {regList.map((item) => {
          const diff = diffMap.get(item.name);
          const hasChanged = diff?.changed ?? false;

          let rawNum = 0n;
          try {
            rawNum = BigInt(item.val);
          } catch {}

          return (
            <div
              key={item.name}
              className={`p-2 rounded border flex flex-col justify-between transition-colors font-mono relative group ${
                hasChanged
                  ? 'bg-amber-950/30 border-amber-500/50 text-amber-200'
                  : 'bg-slate-950/60 border-slate-800 text-slate-300'
              }`}
            >
              <div className="flex items-center justify-between">
                <span className={`font-bold ${hasChanged ? 'text-amber-400' : 'text-indigo-400'}`}>
                  {item.name}
                </span>

                <div className="flex items-center gap-1">
                  {hasChanged && (
                    <span className="text-[9px] px-1 py-0.2 rounded bg-amber-500/20 text-amber-400 border border-amber-500/30">
                      Delta
                    </span>
                  )}
                  <button
                    onClick={() => copyToClipboard(item.name, item.val)}
                    className="opacity-0 group-hover:opacity-100 p-0.5 text-slate-500 hover:text-slate-200 transition-opacity"
                    title="Copy hex value"
                  >
                    {copiedKey === item.name ? (
                      <Check className="w-3 h-3 text-emerald-400" />
                    ) : (
                      <Copy className="w-3 h-3" />
                    )}
                  </button>
                </div>
              </div>

              {/* Value display */}
              <div className="mt-1 flex items-baseline justify-between gap-1 overflow-hidden">
                <span className="font-mono text-xs font-semibold tracking-wide text-white truncate">
                  {item.val}
                </span>
                <span className="text-[10px] text-slate-500 font-mono truncate" title={`Decimal: ${rawNum.toString()}`}>
                  dec: {rawNum < 1000000000n ? rawNum.toString() : '...'}
                </span>
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
};
