import type { NativeState, SnapshotMessage, Reply, NativeEvent } from "./types";
import { acceptSnapshot, estimateFrame, clockOffset } from "./clock";

// Own one connection for the entire app, independently of routes and dialogs.
export class NativeClient {
  private socket: WebSocket | null = null;
  private reconnect: ReturnType<typeof setTimeout> | null = null;
  private pingTimer: ReturnType<typeof setInterval> | null = null;
  private disposed = false;
  private requestId = 0;
  private snapshot: SnapshotMessage | null = null;
  private received = 0;
  private offsetMs: number | null = null;
  private listeners = new Set<() => void>();
  private pending = new Map<
    string,
    {
      resolve: (v: unknown) => void;
      reject: (e: Error) => void;
      timer: ReturnType<typeof setTimeout>;
    }
  >();
  state: NativeState | null = null;
  connected = false;
  error: string | null = null;
  events: NativeEvent[] = [];
  readonly source = `browser-${crypto.randomUUID()}`;
  subscribe = (listener: () => void) => {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  };
  private publish() {
    for (const listener of this.listeners) listener();
  }
  frame() {
    if (!this.snapshot) return 0;
    const received =
      this.offsetMs === null
        ? this.received
        : this.snapshot.state.engineTimeNs / 1e6 - this.offsetMs;
    return estimateFrame(this.snapshot, received, performance.now());
  }
  private async calibrate() {
    const sent = performance.now();
    try {
      const state = (await this.command("clock.ping")) as NativeState;
      const received = performance.now();
      if (received - sent < 250)
        this.offsetMs = clockOffset(sent, received, state.engineTimeNs);
    } catch {
      /* Reconnect owns recovery; never resend sound commands. */
    }
  }
  connect() {
    this.disposed = false;
    if (this.socket) return;
    const socket = new WebSocket(
      `${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/api/v1/events`,
    );
    this.socket = socket;
    this.snapshot = null;
    this.offsetMs = null;
    socket.onopen = () => {
      if (this.socket !== socket) return;
      this.connected = true;
      this.error = null;
      this.publish();
      void this.calibrate();
      this.pingTimer = setInterval(() => {
        void this.calibrate();
      }, 2000);
    };
    socket.onmessage = (event) => {
      if (this.socket !== socket) return;
      try {
        const message = JSON.parse(event.data) as SnapshotMessage | Reply;
        if ("type" in message && message.type === "snapshot") {
          if (message.version === 1 && acceptSnapshot(this.snapshot, message)) {
            // Initial/resync state is authoritative, historical events must not retrigger UI.
            this.events =
              this.snapshot && !message.resync ? message.events : [];
            this.snapshot = message;
            this.state = message.state;
            this.received = performance.now();
            this.publish();
          }
        } else if ("id" in message) {
          const request = this.pending.get(message.id);
          if (request) {
            clearTimeout(request.timer);
            this.pending.delete(message.id);
            if (message.ok) request.resolve(message.result);
            else {
              this.error = message.error?.message ?? "Native command rejected";
              request.reject(new Error(this.error));
              this.publish();
            }
          }
        }
      } catch {
        this.error = "Invalid native response";
        this.publish();
      }
    };
    socket.onclose = () => {
      if (this.socket !== socket) return;
      this.socket = null;
      this.connected = false;
      if (this.pingTimer) clearInterval(this.pingTimer);
      this.pingTimer = null;
      for (const request of this.pending.values()) {
        clearTimeout(request.timer);
        request.reject(new Error("Native connection lost"));
      }
      this.pending.clear();
      this.publish();
      if (!this.disposed)
        this.reconnect = setTimeout(() => this.connect(), 750);
    };
    socket.onerror = () => {
      if (this.socket === socket) {
        this.error = "Connecting to the local piano service…";
        this.publish();
      }
    };
  }
  command(
    type: string,
    params: Record<string, unknown> = {},
  ): Promise<unknown> {
    if (!this.connected || this.socket?.readyState !== WebSocket.OPEN) {
      this.error = "Native service is disconnected";
      this.publish();
      return Promise.reject(new Error(this.error));
    }
    const id = String(++this.requestId);
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        this.error = "Native command timed out";
        this.publish();
        reject(new Error(this.error));
      }, 5000);
      this.pending.set(id, { resolve, reject, timer });
      this.socket!.send(JSON.stringify({ version: 1, id, type, params }));
    });
  }
  dispose() {
    this.disposed = true;
    if (this.reconnect) clearTimeout(this.reconnect);
    this.reconnect = null;
    if (this.pingTimer) clearInterval(this.pingTimer);
    this.pingTimer = null;
    this.socket?.close();
  }
  clearError() {
    this.error = null;
    this.publish();
  }
}
