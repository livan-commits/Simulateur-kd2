// Emulator worker: the WASM ESP32-P4, the co-simulation pump, the virtual
// Kode Dot part and the framebuffer blit all run here, off the page's main
// thread. The page keeps the UI, the console log and the input forwarding.
import init, { WasmEmulator } from "./esp-emu/pkg/esp_emu.js";
import { CosimPump } from "./esp-emu/cosim.js";

let emu = null;
let pump = null;
let boardWorker = null;
let uartInput = null;
let ctx = null;
let scratchImage = null;
let scratchView = null;
let framebufferSymbols = null;
let lastFramebufferSeq = -1;
let hasFramebufferFrame = false;
let framebufferErrorShown = false;
let started = false;

// RGB565 -> RGBA8888, precomputed: one table lookup per pixel instead of
// the shift/mask/multiply chain, written through a Uint32Array view of the
// ImageData buffer (little-endian: 0xAABBGGRR renders as R,G,B,A bytes).
const RGB565_LUT = new Uint32Array(65536);
for (let value = 0; value < 65536; value++) {
  const r = (value >>> 11) & 0x1f;
  const g = (value >>> 5) & 0x3f;
  const b = value & 0x1f;
  RGB565_LUT[value] = (255 << 24)
    | (Math.round((b * 255 + 15) / 31) << 16)
    | (Math.round((g * 255 + 31) / 63) << 8)
    | Math.round((r * 255 + 15) / 31);
}

function readMemoryWord(address) {
  const bytes = emu.read_memory(address, 4);
  if (!(bytes instanceof Uint8Array) || bytes.length !== 4) {
    throw new Error("Lecture mémoire ESP-EMU incomplète à 0x" + address.toString(16));
  }
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).getUint32(0, true);
}

function pollLoop() {
  pollFramebuffer();
  if (started) setTimeout(pollLoop, 16);
}

function pollFramebuffer() {
  if (!started || !framebufferSymbols) return;
  try {
    const seqAddress = Number.parseInt(framebufferSymbols.sequence_address, 16);
    const pointerAddress = Number.parseInt(framebufferSymbols.front_pointer_address, 16);
    const y1Address = Number.parseInt(framebufferSymbols.dirty_y1_address, 16);
    const y2Address = Number.parseInt(framebufferSymbols.dirty_y2_address, 16);
    const seqBefore = readMemoryWord(seqAddress);
    if ((seqBefore & 1) !== 0 || seqBefore === lastFramebufferSeq) return;

    const framebuffer = readMemoryWord(pointerAddress);
    const dirtyY1 = readMemoryWord(y1Address);
    const dirtyY2 = readMemoryWord(y2Address);
    const seqCheck = readMemoryWord(seqAddress);
    if (seqBefore !== seqCheck || (seqCheck & 1) !== 0) return;
    if (!framebuffer) return;

    const y1 = hasFramebufferFrame ? Math.max(0, Math.min(501, dirtyY1)) : 0;
    const y2 = hasFramebufferFrame ? Math.max(y1, Math.min(502, dirtyY2)) : 502;
    if (y2 <= y1) {
      lastFramebufferSeq = seqCheck;
      return;
    }

    if (!scratchImage) {
      scratchImage = ctx.createImageData(410, 502);
      scratchView = new Uint32Array(scratchImage.data.buffer);
    }
    const rowBytes = 410 * 2;
    const maxChunk = Number(framebufferSymbols.max_read_bytes || 4096);
    let base = y1 * 410;
    for (let offset = 0; offset < (y2 - y1) * rowBytes; offset += maxChunk) {
      const count = Math.min(maxChunk, (y2 - y1) * rowBytes - offset);
      const chunk = emu.read_memory(framebuffer + y1 * rowBytes + offset, count);
      if (!(chunk instanceof Uint8Array) || chunk.length !== count) {
        throw new Error("Lecture PSRAM incomplète (" + offset + "/" + ((y2 - y1) * rowBytes) + " octets)");
      }
      const pixelBase = base + (offset >> 1);
      for (let i = 0, p = pixelBase; i < chunk.length; i += 2, p++) {
        scratchView[p] = RGB565_LUT[chunk[i] | (chunk[i + 1] << 8)];
      }
    }

    const seqAfterCopy = readMemoryWord(seqAddress);
    if (seqAfterCopy !== seqCheck || (seqAfterCopy & 1) !== 0) return;

    ctx.putImageData(scratchImage, 0, y1, 0, 0, 410, y2 - y1);
    lastFramebufferSeq = seqAfterCopy;
    hasFramebufferFrame = true;
    framebufferErrorShown = false;
    self.postMessage({ type: "frame", text: "PSRAM KGUI · séquence " + seqAfterCopy });
  } catch (error) {
    if (!framebufferErrorShown) {
      self.postMessage({ type: "uart", text: "[framebuffer mémoire] " + (error?.message || error) + "\n" });
      framebufferErrorShown = true;
    }
  }
}

self.onmessage = async (event) => {
  const msg = event.data;
  if (msg.type === "input") {
    boardWorker?.postMessage({ type: "input", kind: msg.kind });
    return;
  }
  if (msg.type === "uart-in") {
    uartInput?.(msg.text);
    return;
  }
  if (msg.type !== "start") return;
  try {
    await init();
    emu = new WasmEmulator("esp32p4");
    emu.load_default_rom();
    await emu.load_firmware(new Uint8Array(msg.firmware));
    framebufferSymbols = msg.symbols;
    ctx = msg.canvas.getContext("2d", { alpha: false });
    const encoder = new TextEncoder();
    uartInput = (text) => emu.uart_input(encoder.encode(text));

    boardWorker = new Worker("./esp-emu/part-worker.js", { type: "module" });
    const channel = new MessageChannel();
    pump = new CosimPump(emu, {
      kodeboard: { port: channel.port1, worker: boardWorker }
    });
    boardWorker.postMessage({
      type: "start",
      port: channel.port2,
      module: "./parts/kode_board.wasm",
      config: { pins: { SCL: 5, SDA: 4 } }
    }, [channel.port2]);
    if (!(await pump.waitForDevice(5000))) {
      throw new Error("La carte virtuelle Kode Dot ne s'est pas attachée au bus I2C en 5 secondes.");
    }
    self.postMessage({ type: "status", message: "ESP32-P4 démarré · attente du démarrage KodeOS…", kind: "ok" });
    pump.start(
      (text) => self.postMessage({ type: "uart", text }),
      (error) => self.postMessage({ type: "emu-error", error: String(error?.message || error) })
    );
    started = true;
    pollLoop();
    self.postMessage({ type: "started" });
  } catch (error) {
    self.postMessage({ type: "start-failed", error: String(error?.stack || error) });
  }
};
