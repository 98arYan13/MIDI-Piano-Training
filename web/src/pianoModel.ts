export const pianoKeys = () => {
  const names = [
    "C",
    "C♯",
    "D",
    "D♯",
    "E",
    "F",
    "F♯",
    "G",
    "G♯",
    "A",
    "A♯",
    "B",
  ];
  let white = 0;
  return Array.from({ length: 88 }, (_, i) => {
    const midi = i + 21,
      black = [1, 3, 6, 8, 10].includes(midi % 12);
    const left = black ? white - 0.32 : white++;
    return {
      midi,
      label: `${names[midi % 12]}${Math.floor(midi / 12) - 1}`,
      black,
      left,
    };
  });
};
export const keyboardWidth = (width: number) => Math.max(1040, width);
export const computerMap: Record<string, number> = {
  KeyA: 0,
  KeyW: 1,
  KeyS: 2,
  KeyE: 3,
  KeyD: 4,
  KeyF: 5,
  KeyT: 6,
  KeyG: 7,
  KeyY: 8,
  KeyH: 9,
  KeyU: 10,
  KeyJ: 11,
  KeyK: 12,
  KeyO: 13,
  KeyL: 14,
  KeyP: 15,
  Semicolon: 16,
};
let instance = 0;
export class VirtualInput {
  private held = new Map<string, { note: number; instance: number }>();
  private pedalDown = false;
  constructor(
    private send: (type: string, params: Record<string, unknown>) => void,
    private source: string,
  ) {}
  on(owner: string, note: number, velocity: number) {
    if (this.held.has(owner) || note < 21 || note > 108) return;
    const token = { note, instance: ++instance };
    this.held.set(owner, token);
    this.send("virtual.noteOn", {
      source: this.source,
      channel: 0,
      ...token,
      velocity,
    });
  }
  off(owner: string) {
    const token = this.held.get(owner);
    if (!token) return;
    this.held.delete(owner);
    this.send("virtual.noteOff", { source: this.source, channel: 0, ...token });
  }
  pedal(down: boolean) {
    if (down === this.pedalDown) return;
    this.pedalDown = down;
    this.send("virtual.pedal", { source: this.source, channel: 0, down });
  }
  release() {
    if (!this.held.size && !this.pedalDown) return;
    this.held.clear();
    this.pedalDown = false;
    this.send("virtual.disconnect", { source: this.source });
  }
}
