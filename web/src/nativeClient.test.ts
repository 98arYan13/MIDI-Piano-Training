import { it, expect } from "vitest";
import { NativeClient } from "./nativeClient";
it("exposes a native command error when connection is unavailable", async () => {
  const client = new NativeClient();
  await expect(
    client.command("audio.settings", { volume: 0.5 }),
  ).rejects.toThrow("Native service is disconnected");
  expect(client.error).toBe("Native service is disconnected");
});
