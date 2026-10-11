import React, { useState } from 'react';
import { Header } from './components/layout/Header';
import { ExecutionToolbar } from './components/toolbar/ExecutionToolbar';
import { InteractiveTimeline } from './components/timeline/InteractiveTimeline';
import { CodeEditor } from './components/editor/CodeEditor';
import { RegisterInspector } from './components/registers/RegisterInspector';
import { MemoryInspector } from './components/memory/MemoryInspector';
import { EventLog } from './components/events/EventLog';
import { ProgramOutput } from './components/output/ProgramOutput';
import { StatusPanel } from './components/layout/StatusPanel';
import { useDebugger } from './features/debugging/useDebugger';
import { EXAMPLE_PROGRAMS } from './features/debugging/examples';
import { AlertCircle, X } from 'lucide-react';

export const App: React.FC = () => {
  const {
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
    clearErrorNotification,
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
    clearOutput,
  } = useDebugger();

  const [sourceCode, setSourceCode] = useState<string>(EXAMPLE_PROGRAMS[0].source);
  const [language, setLanguage] = useState<string>('c');
  const [optimization, setOptimization] = useState<string>('O0');
  const [debugSymbols, setDebugSymbols] = useState<boolean>(true);

  const handleSelectExample = (id: string) => {
    const ex = EXAMPLE_PROGRAMS.find((p) => p.id === id);
    if (ex) {
      setSourceCode(ex.source);
    }
  };

  const isSessionActive =
    sessionState !== 'created' && sessionState !== 'stopped' && sessionState !== 'failed';

  return (
    <div className="flex flex-col h-screen bg-slate-950 text-slate-100 overflow-hidden font-sans">
      {/* Top Application Header */}
      <Header
        backendConnected={backendConnected}
        architecture={capabilities?.architecture ?? 'x86_64'}
        sessionId={sessionId}
        pid={pid}
        state={sessionState}
        selectedSequence={selectedSequence}
        latestSequence={latestSequence}
        isHistoricalMode={isHistoricalMode}
      />

      {/* Execution Control Toolbar */}
      <ExecutionToolbar
        state={sessionState}
        isHistoricalMode={isHistoricalMode}
        selectedSequence={selectedSequence}
        latestSequence={latestSequence}
        onStart={() =>
          startSession(sourceCode, language, { optimization, debugSymbols })
        }
        onPause={pauseSession}
        onResume={resumeSession}
        onStep={stepSession}
        onStop={stopSession}
        onSeekPrevious={seekPrevious}
        onSeekNext={seekNext}
        onReturnToLive={returnToLive}
        loading={loading}
      />

      {/* Interactive Horizontal Timeline */}
      <div className="px-3 pt-2 pb-1 shrink-0">
        <InteractiveTimeline
          events={events}
          checkpoints={checkpoints}
          selectedSequence={selectedSequence}
          latestSequence={latestSequence}
          onSelectSequence={seek}
        />
      </div>

      {/* Error Notification Toast */}
      {errorNotification && (
        <div className="mx-3 mt-1 bg-rose-950 border border-rose-800 text-rose-200 px-3 py-2 rounded text-xs flex items-center justify-between shadow-lg">
          <div className="flex items-center gap-2">
            <AlertCircle className="w-4 h-4 text-rose-400 shrink-0" />
            <span>{errorNotification}</span>
          </div>
          <button
            onClick={clearErrorNotification}
            className="text-rose-400 hover:text-rose-100 p-0.5 cursor-pointer"
          >
            <X className="w-4 h-4" />
          </button>
        </div>
      )}

      {/* Main Multi-Panel Workspace */}
      <div className="flex-1 px-3 py-2 grid grid-cols-1 lg:grid-cols-12 gap-2.5 overflow-hidden min-h-0">
        {/* Left Column: Code Editor (4 cols on lg) */}
        <div className="lg:col-span-4 h-full flex flex-col min-h-0">
          <CodeEditor
            source={sourceCode}
            onChange={setSourceCode}
            language={language}
            onLanguageChange={setLanguage}
            optimization={optimization}
            onOptimizationChange={setOptimization}
            debugSymbols={debugSymbols}
            onDebugSymbolsChange={setDebugSymbols}
            compilationError={compilationError}
            compilationExitCode={compilationExitCode}
            isSessionActive={isSessionActive}
            onSelectExample={handleSelectExample}
          />
        </div>

        {/* Center Column: Register Inspector & Memory Inspector (4 cols on lg) */}
        <div className="lg:col-span-4 h-full flex flex-col gap-2.5 min-h-0">
          <div className="flex-1 min-h-0">
            <RegisterInspector
              registers={registers}
              registerDiff={registerDiff}
              isLive={!isHistoricalMode}
              selectedSequence={selectedSequence}
              checkpointId={seekResult?.nearestCheckpointId}
            />
          </div>
          <div className="h-48 shrink-0">
            <MemoryInspector
              memoryDump={memoryDump}
              currentRegisters={registers}
              onReadMemory={readMemory}
              loading={loading}
              isHistorical={isHistoricalMode}
            />
          </div>
        </div>

        {/* Right Column: Event Log & Program Console (4 cols on lg) */}
        <div className="lg:col-span-4 h-full flex flex-col gap-2.5 min-h-0">
          <div className="flex-1 min-h-0">
            <EventLog
              events={events}
              selectedSequence={selectedSequence}
              onSelectEvent={seek}
            />
          </div>
          <div className="h-48 shrink-0">
            <ProgramOutput
              stdout={stdout}
              stderr={stderr}
              onClear={clearOutput}
            />
          </div>
        </div>
      </div>

      {/* Bottom Telemetry & Status Bar */}
      <div className="px-3 pb-2 shrink-0">
        <StatusPanel
          sessionState={currentState}
          seekResult={seekResult}
          isHistoricalMode={isHistoricalMode}
        />
      </div>
    </div>
  );
};

export default App;

