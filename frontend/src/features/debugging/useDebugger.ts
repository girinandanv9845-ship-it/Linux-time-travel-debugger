import { useState, useEffect, useRef } from 'react';
import { api, ApiError } from '../../services/api';
import { DebugWebSocket } from '../../services/websocket';
import {
  SessionState,
  SessionCurrentState,
  TraceEvent,
  Checkpoint,
  Registers,
  RegisterDiffItem,
  MemoryDump,
  SeekResult,
  BackendCapabilities,
} from '../../types/debugger';

export function useDebugger() {
  const [backendConnected, setBackendConnected] = useState<boolean>(false);
  const [capabilities, setCapabilities] = useState<BackendCapabilities | null>(null);

  const [sessionId, setSessionId] = useState<string | null>(null);
  const [sessionState, setSessionState] = useState<SessionState>('created');
  const [pid, setPid] = useState<number | null>(null);
  const [currentState, setCurrentState] = useState<SessionCurrentState | null>(null);

  const [events, setEvents] = useState<TraceEvent[]>([]);
  const [checkpoints, setCheckpoints] = useState<Checkpoint[]>([]);

  const [selectedSequence, setSelectedSequence] = useState<number>(0);
  const [latestSequence, setLatestSequence] = useState<number>(0);
  const [isHistoricalMode, setIsHistoricalMode] = useState<boolean>(false);

  const [registers, setRegisters] = useState<Registers | null>(null);
  const [registerDiff, setRegisterDiff] = useState<RegisterDiffItem[]>([]);
  const [memoryDump, setMemoryDump] = useState<MemoryDump | null>(null);
  const [seekResult, setSeekResult] = useState<SeekResult | null>(null);

  const [stdout, setStdout] = useState<string>('');
  const [stderr, setStderr] = useState<string>('');

  const [compilationError, setCompilationError] = useState<string | null>(null);
  const [compilationExitCode, setCompilationExitCode] = useState<number | null>(null);

  const [loading, setLoading] = useState<boolean>(false);
  const [errorNotification, setErrorNotification] = useState<string | null>(null);

  const wsRef = useRef<DebugWebSocket | null>(null);
  const pendingTraceEventsRef = useRef<TraceEvent[]>([]);
  const traceFlushTimerRef = useRef<number | null>(null);

  const flushPendingTraceEvents = () => {
    const batch = pendingTraceEventsRef.current;
    pendingTraceEventsRef.current = [];
    traceFlushTimerRef.current = null;
    if (!batch.length) return;

    setEvents((prev) => {
      const next = [...prev];
      const seen = new Set(next.map((event) => event.sequence));
      for (const ev of batch) {
        if (!seen.has(ev.sequence)) {
          seen.add(ev.sequence);
          next.push(ev);
        }
      }
      return next;
    });

    const maxSequence = batch.reduce((max, ev) => Math.max(max, ev.sequence), latestSequence);
    setLatestSequence(maxSequence);
  };

  // Check backend health and capabilities on mount
  useEffect(() => {
    const checkHealth = async () => {
      try {
        const health = await api.getHealth();
        if (health.status === 'healthy') {
          setBackendConnected(true);
          const caps = await api.getCapabilities();
          setCapabilities(caps);
        }
      } catch {
        setBackendConnected(false);
      }
    };

    checkHealth();
    const timer = setInterval(checkHealth, 5000);
    return () => clearInterval(timer);
  }, []);

  // Manage WebSocket connection when sessionId changes
  useEffect(() => {
    if (!sessionId) {
      if (wsRef.current) {
        wsRef.current.disconnect();
        wsRef.current = null;
      }
      return;
    }

    const ws = new DebugWebSocket(
      sessionId,
      (type, payload) => {
        if (type === 'trace_event') {
          const ev = payload as TraceEvent;
          pendingTraceEventsRef.current.push(ev);

          if (pendingTraceEventsRef.current.length >= 25) {
            flushPendingTraceEvents();
          } else if (!traceFlushTimerRef.current) {
            traceFlushTimerRef.current = window.setTimeout(() => {
              flushPendingTraceEvents();
            }, 16);
          }

          // Update live registers if not in historical mode
          if (!isHistoricalMode && ev.registers) {
            setRegisters((prev) => {
              if (prev && ev.registers) {
                // compute diff
                const diffs: RegisterDiffItem[] = [];
                const keys: Array<keyof Registers> = [
                  'rip', 'rsp', 'rbp', 'rax', 'rbx', 'rcx', 'rdx', 'rsi', 'rdi',
                  'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15', 'eflags'
                ];
                keys.forEach((k) => {
                  const oldV = prev[k] as string;
                  const newV = ev.registers![k] as string;
                  diffs.push({
                    name: k.toUpperCase(),
                    oldValue: oldV,
                    newValue: newV,
                    changed: oldV !== newV,
                  });
                });
                setRegisterDiff(diffs);
              }
              return ev.registers;
            });
          }
        } else if (type === 'checkpoint_created') {
          const ckpt = payload as Checkpoint;
          setCheckpoints((prev) => {
            if (prev.some((c) => c.id === ckpt.id)) return prev;
            return [...prev, ckpt];
          });
        } else if (type === 'session_state_changed') {
          setSessionState(payload.state as SessionState);
          if (payload.pid) setPid(payload.pid);
        } else if (type === 'program_output') {
          setStdout(payload.stdout || '');
          setStderr(payload.stderr || '');
        } else if (type === 'compilation_failed') {
          setCompilationError(payload.stderr || 'Compilation failed');
          setCompilationExitCode(payload.exitCode ?? 1);
        } else if (type === 'tracing_error') {
          setErrorNotification(payload.message || 'Tracing failed; inspect the backend diagnostic log for details');
        }
      },
      (connected) => {
        setBackendConnected(connected);
      }
    );

    wsRef.current = ws;

    return () => {
      if (traceFlushTimerRef.current) {
        clearTimeout(traceFlushTimerRef.current);
        traceFlushTimerRef.current = null;
      }
      pendingTraceEventsRef.current = [];
      ws.disconnect();
      wsRef.current = null;
    };
  }, [sessionId, isHistoricalMode]);

  // Launch target session
  const startSession = async (
    source: string,
    language = 'c',
    options = { optimization: 'O0', debugSymbols: true }
  ) => {
    setLoading(true);
    setErrorNotification(null);
    setCompilationError(null);
    setCompilationExitCode(null);
    setEvents([]);
    setCheckpoints([]);
    setSelectedSequence(0);
    setLatestSequence(0);
    setIsHistoricalMode(false);
    setRegisters(null);
    setRegisterDiff([]);
    setMemoryDump(null);
    setSeekResult(null);
    setStdout('');
    setStderr('');

    try {
      const res = await api.startSession(source, language, options);
      setSessionId(res.sessionId);
      setSessionState(res.state as SessionState);
      setPid(res.pid);
      setCurrentState(res.initialState);

      // Fetch initial timeline
      const timeline = await api.getTimeline(res.sessionId);
      setEvents(timeline.events);
      setCheckpoints(timeline.checkpoints);
      setLatestSequence(timeline.latestSequence);
      setSelectedSequence(timeline.latestSequence);

      if (res.initialState?.liveRegisters) {
        setRegisters(res.initialState.liveRegisters);
      }
    } catch (err: any) {
      const errData = (err instanceof ApiError) ? (err.data as any) : null;
      if (errData?.compilation) {
        setCompilationError(errData.compilation.stderr || err.message);
        setCompilationExitCode(errData.compilation.exitCode ?? 1);
      } else {
        setErrorNotification(err.message || 'Failed to start debugging session');
      }
    } finally {
      setLoading(false);
    }
  };

  // Pause session
  const pauseSession = async () => {
    if (!sessionId) return;
    setLoading(true);
    try {
      const res = await api.pauseSession(sessionId);
      setSessionState(res.state as SessionState);
      // Fetch latest state
      const state = await api.getSessionState(sessionId);
      setCurrentState(state);
      if (state.liveRegisters) {
        setRegisters(state.liveRegisters);
      }
    } catch (err: any) {
      setErrorNotification(err.message);
    } finally {
      setLoading(false);
    }
  };

  // Resume session
  const resumeSession = async () => {
    if (!sessionId) return;
    setLoading(true);
    try {
      const res = await api.resumeSession(sessionId);
      setSessionState(res.state as SessionState);
      setIsHistoricalMode(false);
    } catch (err: any) {
      setErrorNotification(err.message);
    } finally {
      setLoading(false);
    }
  };

  // Single-step
  const stepSession = async () => {
    if (!sessionId) return;
    setLoading(true);
    try {
      const res = await api.stepSession(sessionId);
      setSessionState(res.state as SessionState);
      setCurrentState(res.currentState);

      if (res.event) {
        setEvents((prev) => {
          if (prev.some((e) => e.sequence === res.event.sequence)) return prev;
          return [...prev, res.event];
        });
        setLatestSequence(res.event.sequence);
        setSelectedSequence(res.event.sequence);
      }

      if (res.currentState?.liveRegisters) {
        setRegisters((prev) => {
          if (prev && res.currentState.liveRegisters) {
            const diffs: RegisterDiffItem[] = [];
            const keys: Array<keyof Registers> = [
              'rip', 'rsp', 'rbp', 'rax', 'rbx', 'rcx', 'rdx', 'rsi', 'rdi',
              'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15', 'eflags'
            ];
            keys.forEach((k) => {
              const oldV = prev[k] as string;
              const newV = res.currentState.liveRegisters![k] as string;
              diffs.push({
                name: k.toUpperCase(),
                oldValue: oldV,
                newValue: newV,
                changed: oldV !== newV,
              });
            });
            setRegisterDiff(diffs);
          }
          return res.currentState.liveRegisters;
        });
      }

      // Refresh output
      const out = await api.getOutput(sessionId);
      setStdout(out.stdout);
      setStderr(out.stderr);
    } catch (err: any) {
      setErrorNotification(err.message);
    } finally {
      setLoading(false);
    }
  };

  // Stop session
  const stopSession = async () => {
    if (!sessionId) return;
    setLoading(true);
    try {
      const res = await api.stopSession(sessionId);
      setSessionState(res.state as SessionState);
    } catch (err: any) {
      setErrorNotification(err.message);
    } finally {
      setLoading(false);
    }
  };

  // Historical Seek
  const seek = async (sequence: number) => {
    if (!sessionId || sequence <= 0) return;
    setLoading(true);
    try {
      const res = await api.seekSession(sessionId, sequence);
      setSeekResult(res);
      setSelectedSequence(res.actualSequence);
      setIsHistoricalMode(res.actualSequence !== latestSequence);

      if (res.registers) {
        setRegisters(res.registers);
      }
      if (res.registerDiff) {
        setRegisterDiff(res.registerDiff);
      }
      if (res.stackMemory) {
        setMemoryDump(res.stackMemory);
      }
    } catch (err: any) {
      setErrorNotification(err.message);
    } finally {
      setLoading(false);
    }
  };

  // Seek Previous
  const seekPrevious = () => {
    const cur = selectedSequence || latestSequence;
    if (cur > 1) {
      seek(cur - 1);
    }
  };

  // Seek Next
  const seekNext = () => {
    const cur = selectedSequence || latestSequence;
    if (cur < latestSequence) {
      seek(cur + 1);
    }
  };

  // Return to live process state
  const returnToLive = async () => {
    if (!sessionId) return;
    setIsHistoricalMode(false);
    setSelectedSequence(latestSequence);
    setSeekResult(null);

    // Refresh live registers
    try {
      const state = await api.getSessionState(sessionId);
      if (state.liveRegisters) {
        setRegisters(state.liveRegisters);
      }
    } catch {}
  };

  // Read memory
  const readMemory = async (address: string, length = 64) => {
    if (!sessionId) return;
    setLoading(true);
    try {
      const dump = await api.readMemory(sessionId, address, length);
      setMemoryDump(dump);
    } catch (err: any) {
      setErrorNotification(err.message);
    } finally {
      setLoading(false);
    }
  };

  return {
    backendConnected,
    capabilities,
    sessionId,
    sessionState,
    pid,
    currentState,
    events,
    checkpoints,
    selectedSequence,
    latestSequence,
    isHistoricalMode,
    registers,
    registerDiff,
    memoryDump,
    seekResult,
    stdout,
    stderr,
    compilationError,
    compilationExitCode,
    loading,
    errorNotification,
    clearErrorNotification: () => setErrorNotification(null),
    startSession,
    pauseSession,
    resumeSession,
    stepSession,
    stopSession,
    seek,
    seekPrevious,
    seekNext,
    returnToLive,
    readMemory,
    clearOutput: () => {
      setStdout('');
      setStderr('');
    },
  };
}
