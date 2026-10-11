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
  ear: EarSnapshot;
  flash: FlashSnapshot;
  practice: PracticeSnapshot;
  transport: TransportSnapshot;
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
  overload: number;
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
export type EarConfig = {
  mode: "interval" | "melody";
  intervals: string[];
  directions: string[];
  root: number;
  randomRoot: boolean;
  transpose: number;
  tempo: number;
  order: "ordered" | "random";
  playOrder: "forward" | "reverse" | "harmonic";
  groups: number;
  pitches: number;
  range: { min: number; max: number };
  melodyDirection: "ascending" | "descending" | "any";
  continuous: boolean;
  pitchWheel: boolean;
};
export type EarSnapshot = {
  config: EarConfig;
  target: null | {
    mode: string;
    root: number;
    interval?: string;
    direction?: string;
    key?: string;
    groups: number[][];
  };
  playing: boolean;
  paused: boolean;
  autoplay: boolean;
  answerLocked: boolean;
  index: number;
  hits: number;
  misses: number;
  accuracy: number;
  elapsed: number;
  feedback: string;
  error: string;
  answered: boolean;
  notes: import("./notationModel").TimelineNote[];
  endFrame: number;
  pauseFrame: number;
  revision: number;
};
export type FlashConfig = {
  mode: "noteMath" | "chord";
  roots: string[];
  distances: number[];
  keys: string[];
  inversions: number[];
  staff: "treble" | "bass" | "grand";
};
export type ChordAnswer = { root: string; accidental: string; quality: string };
export type FlashSnapshot = {
  answerLocked: boolean;
  config: FlashConfig;
  target: null | {
    mode: string;
    root?: string;
    distance?: number;
    key?: string;
    inversion?: number;
    answer: string | ChordAnswer;
    notes?: import("./notationModel").TimelineNote[];
  };
  hits: number;
  misses: number;
  accuracy: number;
  feedback: string;
  answered: boolean;
  revision: number;
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
  type: "note" | "beat" | "practice";
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
  id: number;
  trackId: number;
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
  tracks: SongTrack[];
  durationTicks: number;
  durationSeconds: number;
  recent: number;
  trashed?: boolean;
  notes: NoteEvent[];
  controllers: unknown[];
  tempoMap: unknown[];
  meterMap: unknown[];
  keyMap: unknown[];
};
export type TransportSnapshot = {
  accompanimentError: string;
  followError: string;
  tempo: number;
  songId: string;
  title: string;
  revision: number;
  songSeconds: number;
  retry: boolean;
  target: number[];
  hits: number;
  misses: number;
  accuracy: number;
  error: string;
  config: TransportConfig;
  generation: number;
  sampleFrame: number;
  sampleRate: number;
  engineTimeNs: number;
  songTick: number;
  state: "stopped" | "playing" | "paused";
  speed: number;
};
export type SongTrack = {
  id: number;
  title: string;
  noteCount: number;
  percussion: boolean;
  practice: boolean;
  backing: boolean;
  staff: "split" | "treble" | "bass" | "grand";
};
export type TransportConfig = {
  speed: number;
  hearPractice: boolean;
  practiceEnabled: boolean;
  mode: "continuous" | "pauseOnMiss";
  toleranceMs: number;
  spacing: number;
  loop: boolean;
  a: number;
  b: number;
  accompaniment: string;
  rate: number;
  accompanimentSpacing: number;
};
export type MetronomeMeterEdit = Partial<Meter>;
export type MetronomeAccentEdit = {
  beat: number;
  accent: MetronomeSettings["accents"][number];
};
export type AudioChannelEdit = { channel: number; enabled: boolean };
// Shared visual-only timeline contract. Native controllers own generation, scoring and sound.
export type { StaffMode, SpelledPitch, TimelineNote } from "./notationModel";
export type PracticeConfig = {
  staff: "treble" | "bass" | "grand" | "chordSymbol";
  generator:
    | "random"
    | "triads"
    | "sevenths"
    | "progression"
    | "position"
    | "intervals"
    | "chordSymbol";
  key: string;
  range: { min: number; max: number };
  smoothness: number;
  mode: "wait" | "scroll";
  tempo: number;
  toleranceMs: number;
  seed: number;
  generatorOptions: {
    notes: number;
    hands: number;
    chordBased: boolean;
    inversions: boolean;
    openVoicing: boolean;
    progression: "autumnLeaves" | "fifties" | "circle" | "substitution";
    startingFinger: number;
    intervals: number[];
    randomChords: boolean;
    multiKey: boolean;
    keys: string[];
    scale: "major" | "naturalMinor" | "harmonicMinor" | "melodicMinor";
    tones: number;
    commonNotes: number;
    ignoreAbove: number;
  };
};
export type PracticeSnapshot = {
  error: string;
  config: PracticeConfig;
  running: boolean;
  revision: number;
  originFrame: number;
  clockFrame: number;
  index: number;
  hits: number;
  misses: number;
  accuracy: number;
  elapsed: number;
  notes: import("./notationModel").TimelineNote[];
  targets: {
    index: number;
    symbol: string;
    fingering: number;
    result: "target" | "hit" | "miss";
    startFrame: number;
    endFrame: number;
    pitches: {
      midiNote: number;
      spelling: import("./notationModel").SpelledPitch;
    }[];
  }[];
  pressed: {
    sourceId: number;
    channel: number;
    instance: number;
    note: number;
  }[];
};
