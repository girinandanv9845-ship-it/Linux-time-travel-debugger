export type SessionState =
  | 'created'
  | 'compiling'
  | 'starting'
  | 'running'
  | 'paused'
  | 'exited'
  | 'stopped'
  | 'failed';

export interface Registers {
  rip: string;
  rsp: string;
  rbp: string;
  rax: string;
  rbx: string;
  rcx: string;
  rdx: string;
  rsi: string;
  rdi: string;
  r8: string;
  r9: string;
  r10: string;
  r11: string;
  r12: string;
  r13: string;
  r14: string;
  r15: string;
  eflags: string;
  cs?: string;
  ss?: string;
  ds?: string;
  es?: string;
  fs?: string;
  gs?: string;
  raw?: {
    rip: number;
    rsp: number;
    rbp: number;
    rax: number;
    rbx: number;
    rcx: number;
    rdx: number;
    rsi: number;
    rdi: number;
    r8: number;
    r9: number;
    r10: number;
    r11: number;
    r12: number;
    r13: number;
    r14: number;
    r15: number;
    eflags: number;
  };
}

export interface RegisterDiffItem {
  name: string;
  oldValue: string;
  newValue: string;
  changed: boolean;
}

export interface MemoryDump {
  address: string;
  rawAddress: number;
  length: number;
  bytes: number[];
  hexString: string;
  asciiString: string;
  isAccessible: boolean;
  errorMessage?: string;
  isHistorical?: boolean;
}

export interface TraceEvent {
  sequence: number;
  type: string;
  timestamp: number;
  pid: number;
  threadId: number;
  rip: string;
  rawRip: number;
  sourceLine: number | null;
  sourceFile: string;
  description: string;
  registers: Registers | null;
  signalNumber?: number;
  signalName?: string;
  syscallNumber?: number;
  syscallName?: string;
  checkpointId?: string;
  metadata?: Record<string, unknown>;
}

export interface Checkpoint {
  id: string;
  sequence: number;
  timestamp: number;
  pid: number;
  registers: Registers;
  stackSample: MemoryDump;
  coverageDescription: string;
  metadata?: Record<string, unknown>;
}

export interface SeekResult {
  targetSequence: number;
  actualSequence: number;
  stateAvailability: 'exact_snapshot' | 'interpolated_checkpoint' | 'event_metadata_only';
  isLiveProcess: boolean;
  success: boolean;
  errorMessage?: string;
  event: TraceEvent;
  registers: Registers | null;
  registerDiff: RegisterDiffItem[];
  stackMemory: MemoryDump | null;
  nearestCheckpointId: string;
  nearestCheckpointSequence: number;
  replayLimitationNotice: string;
}

export interface ProgramOutputData {
  stdout: string;
  stderr: string;
  timestamp: number;
  sequence: number;
}

export interface SessionCurrentState {
  sessionId: string;
  state: SessionState;
  pid: number;
  totalEvents: number;
  latestSequence: number;
  selectedSequence: number;
  checkpointsCount: number;
  isLive: boolean;
  liveRegisters: Registers | null;
  output: {
    stdout: string;
    stderr: string;
  };
}

export interface BackendCapabilities {
  architecture: string;
  isSupportedArch: boolean;
  tracingEngine: string;
  supportedLanguages: string[];
  maxMemoryReadBytes: number;
  maxEventsPerSession: number;
  maxCheckpointsPerSession: number;
  eventTypes: string[];
  replayGuarantees: {
    mode: string;
    description: string;
    limitations: string;
  };
}

export interface ExampleProgram {
  id: string;
  name: string;
  filename: string;
  description: string;
  source: string;
}

