export type AudioSettings = {
  volume: number;
  monitoring: boolean;
  outputMode: "internal" | "external" | "silent";
  velocityCurve: number;
  channels: number[];
  outputDevice: number;
  exclusive: boolean;
  externalDevice: number;
};
export type Meter = { numerator: number; denominator: 2 | 4 | 8 | 16 };
export type MetronomeSettings = {
  mode: "independent" | "follow";
  tempo: number;
  meter: Meter;
  beatUnit: number;
  subdivision: "quarter" | "eighth" | "sixteenth" | "triplet";
  accents: ("strong" | "weak" | "mute")[];
  swing: number;
  countIn: number;
  sound: "click" | "woodblock" | "rim";
  volume: number;
  mute: boolean;
};
export type HeldNote = {
  source: string;
  sourceId: number;
  channel: number;
  note: number;
  velocity: number;
  instance: number;
  bus: "live" | "playback" | "example";
  held: boolean;
  sounding: boolean;
  sustained: boolean;
};
export type Device = {
  id: string;
  name: string;
  enabled: boolean;
  connected: boolean;
};
export type NativeState = {
  generation: number;
  engineTimeNs: number;
  sampleFrame: number;
  sampleRate: number;
  audioReady: boolean;
  pianoReady: boolean;
  pianoLoading: boolean;
  error: string | null;
  disabledInputs: Record<string, boolean>;
  externalPortOpens: number;
  devices: Device[];
  midiOutputs: { id: number; name: string }[];
  audioOutputs: { id: number; name: string }[];
  heldNotes: HeldNote[];
  monitor: {
    status: number;
    data1: number;
    data2: number;
    sourceId: number;
    sampleFrame: number;
  }[];
  overload: boolean;
  voiceRefusals: number;
  dropouts: number;
  externalErrors: number;
  pendingChange: AudioSettings | null;
  settings: AudioSettings;
  output: {
    backend: string;
    actualSampleRate: number;
    actualFrames: number;
    requestedFrames: number;
    latencyMs: number;
    name: string;
    exclusive: boolean;
  };
  metronome: {
    running: boolean;
    activeTempo: number;
    activeMeter: Meter;
    beat: number;
    bar: number;
    settings: MetronomeSettings;
  };
};
export type Command = {
  version: 1;
  id: string;
  type: string;
  params: Record<string, unknown>;
};
export type Reply = {
  version: 1;
  id: string;
  ok: boolean;
  result: unknown;
  error: { code: string; message: string } | null;
};
export type NativeEvent = {
  sequence: number;
  type: "note" | "beat";
  event: Record<string, number | boolean>;
};
export type SnapshotMessage = {
  version: 1;
  type: "snapshot";
  generation: number;
  state: NativeState;
  events: NativeEvent[];
  eventDrops: number;
  resync?: boolean;
};
export type NoteEvent = {
  id: string;
  trackId: string;
  channel: number;
  midiNote: number;
  velocity: number;
  startTick: number;
  endTick: number;
};
export type Song = {
  id: string;
  title: string;
  ppq: number;
  tracks: unknown[];
  notes: NoteEvent[];
  controllers: unknown[];
  tempoMap: unknown[];
  meterMap: unknown[];
  keyMap: unknown[];
};
export type TransportSnapshot = {
  generation: number;
  sampleFrame: number;
  sampleRate: number;
  engineTimeNs: number;
  songTick: number;
  state: "stopped" | "playing" | "paused";
  speed: number;
};
export type MetronomeMeterEdit = Partial<Meter>;
export type MetronomeAccentEdit = {
  beat: number;
  accent: MetronomeSettings["accents"][number];
};
export type AudioChannelEdit = { channel: number; enabled: boolean };
