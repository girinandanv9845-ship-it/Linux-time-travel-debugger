import {
  BackendCapabilities,
  SessionCurrentState,
  TraceEvent,
  Checkpoint,
  SeekResult,
  MemoryDump,
  Registers,
  ProgramOutputData
} from '../types/debugger';

const BASE_URL = '/api';

export class ApiError extends Error {
  constructor(public status: number, message: string, public data?: unknown) {
    super(message);
    this.name = 'ApiError';
  }
}

async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
  const url = `${BASE_URL}${path}`;
  const response = await fetch(url, {
    ...options,
    headers: {
      'Content-Type': 'application/json',
      ...options.headers,
    },
  });

  if (!response.ok) {
    let errorMsg = `HTTP Error ${response.status}: ${response.statusText}`;
    let errorData = null;
    try {
      errorData = await response.json();
      if (errorData?.error) {
        errorMsg = errorData.error;
      }
    } catch {
      // Non-JSON response
    }
    throw new ApiError(response.status, errorMsg, errorData);
  }

  return response.json() as Promise<T>;
}

export const api = {
  getHealth: () => request<{ status: string; service: string; version: string; architecture: string }>('/health'),

  getCapabilities: () => request<BackendCapabilities>('/capabilities'),

  startSession: (source: string, language = 'c', options = { optimization: 'O0', debugSymbols: true }) =>
    request<{
      success: boolean;
      sessionId: string;
      state: string;
      pid: number;
      initialState: SessionCurrentState;
    }>('/debug/start', {
      method: 'POST',
      body: JSON.stringify({ source, language, options }),
    }),

  pauseSession: (sessionId: string) =>
    request<{ success: boolean; state: string }>(`/debug/${sessionId}/pause`, { method: 'POST' }),

  resumeSession: (sessionId: string) =>
    request<{ success: boolean; state: string }>(`/debug/${sessionId}/resume`, { method: 'POST' }),

  stepSession: (sessionId: string) =>
    request<{
      success: boolean;
      state: string;
      event: TraceEvent;
      currentState: SessionCurrentState;
    }>(`/debug/${sessionId}/step`, { method: 'POST' }),

  stopSession: (sessionId: string) =>
    request<{ success: boolean; state: string }>(`/debug/${sessionId}/stop`, { method: 'POST' }),

  getSessionState: (sessionId: string) =>
    request<SessionCurrentState>(`/debug/${sessionId}/state`),

  getTimeline: (sessionId: string, from = 1, to = 999999, limit = 500, offset = 0) =>
    request<{
      sessionId: string;
      totalEvents: number;
      latestSequence: number;
      events: TraceEvent[];
      checkpoints: Checkpoint[];
    }>(`/debug/${sessionId}/timeline?from=${from}&to=${to}&limit=${limit}&offset=${offset}`),

  getEvents: (sessionId: string, typeFilter = '', limit = 100, offset = 0) =>
    request<{
      sessionId: string;
      events: TraceEvent[];
    }>(`/debug/${sessionId}/events?type=${encodeURIComponent(typeFilter)}&limit=${limit}&offset=${offset}`),

  getCheckpoints: (sessionId: string) =>
    request<{
      sessionId: string;
      count: number;
      checkpoints: Checkpoint[];
    }>(`/debug/${sessionId}/checkpoints`),

  seekSession: (sessionId: string, targetSequence: number) =>
    request<SeekResult>(`/debug/${sessionId}/seek`, {
      method: 'POST',
      body: JSON.stringify({ targetSequence }),
    }),

  readMemory: (sessionId: string, address: string, length = 64) =>
    request<MemoryDump>(`/debug/${sessionId}/memory?address=${encodeURIComponent(address)}&length=${length}`),

  getRegisters: (sessionId: string) =>
    request<{
      sessionId: string;
      isLive: boolean;
      isHistorical?: boolean;
      checkpointId?: string;
      sequence?: number;
      registers: Registers;
    }>(`/debug/${sessionId}/registers`),

  getOutput: (sessionId: string) =>
    request<ProgramOutputData>(`/debug/${sessionId}/output`),
};

