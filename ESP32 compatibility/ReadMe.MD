# ESP32 Simple Demo - Provides compatibility with the I2C SD-Card Module

This demo targets the ATtiny1614 I2C SD-Card Module firmware and implements an ESP32 master that speaks the very small command set used by that firmware (`F` filename, `W` write, `A` append, `R` read, `S` size). It intentionally avoids the ESP32 hardware I2C driver and Arduino `Wire.h`, and instead uses a minimal bit‑banged (software) I2C master.

## The problem: long clock stretching
The ATtiny1614 I2C SD-Card Module firmware serves SD card operations over I2C. Opening a file, reading sectors, or appending data can occasionally block inside the ATtiny’s SD stack. During those moments, the ATtiny holds SCL low (clock stretching) until the byte can be accepted or returned. In practice we observed stretches on the order of tens of milliseconds (≈65 ms in worst cases) during file operations.

ESP32’s hardware I2C master (used both by ESP‑IDF and Arduino Wire) has internal bus‑timeout and state machine constraints. When a slave holds SCL low for "too long," the controller times out, aborts the transaction, or returns error codes. Even when increasing software timeouts, the peripheral’s finite hardware timeout and ISR/driver assumptions still cause aborted transactions when stretches are very long and occur repeatedly mid‑byte.

In short:
- ATtiny1614: may legally stretch SCL for a long time while it does SD work.
- ESP32 HW I2C (ESP‑IDF/Wire): expects short (under 13ms), bounded stretches; long stretches trigger timeouts or bus error handling paths.

## Why bit‑banged I2C works here
The ESP32 demo implements I2C in software with explicit open‑drain behavior and per‑edge stretch awareness:
- Open‑drain emulation: we drive lows actively and release lines to go high via pull‑ups. This mirrors real I2C electrical behavior and avoids bus contention.
- Per‑clock stretch handling: on every rising edge of SCL we wait until SCL actually goes high before proceeding, honoring any slave‑imposed stretching. My implementation allows long stretches (we use a generous microsecond timeout) without the hardware state machine forcing an abort.
- Simple, byte‑oriented sequencing: we clock each bit and byte deterministically, with explicit ACK/NACK handling, START/STOP timing, and no hidden ISR or DMA assumptions. That simplicity tolerates the ATtiny’s blocking SD operations.

The result is robust communication even when the slave occasionally stretches the clock for tens of milliseconds.

## Why ESP‑IDF and Arduino Wire don’t work (in this setup)
- Both use the ESP32 hardware I2C controller. While it supports clock stretching, it also enforces internal bus‑timeout and state progression that are not designed for repeated, very long stretches in the middle of byte transfers.
- The driver stacks batch transactions and schedule them through ISRs. If a stretch exceeds the controller’s expectations, the transfer can time out and be aborted, returning errors like NACK/timeout rather than passively waiting.
- Arduino `Wire` on ESP32 is a thin wrapper over the same driver, so it inherits the same limitations.

These are reasonable design choices for typical I2C sensors and peripherals (which keep stretches short). They just don’t align with this particular I2C SD-Card Module pattern where the slave is doing heavy SD work while the bus is active.

## Implementation notes (ESP32 demo)
- Pins: SDA=21, SCL=22 by default. Change at the top of the sketch if needed.
- Electrical: use external pull‑up resistors (e.g., 2.2k–3.4k) on SDA/SCL. The GPIO internal pull‑ups are weak and not a substitute.
- Timing: `I2C_DELAY` sets the base software timing. We also wait for SCL to actually go high on every edge to honor stretching.
- Minimal protocol: the demo implements only the very basic commands used by the ATtiny firmware (`F`, `W`, `A`, `R`, `S`).
- Resource usage: pure C‑style code, no dynamic `String`, minimal stack usage. CPU cost is acceptable because transfers are short and infrequent compared to SD work on the ATtiny.

## If you want to use ESP‑IDF/Wire instead
You would need to remove the long stretches from the slave side. Typical strategies:
- Buffering on the ATtiny: quickly ACK bytes and defer SD work until after an I2C transaction (so SCL isn’t held low for long).
- Split operations into short transactions with clear START/STOP boundaries and short, bounded processing between them.
- Use an interrupt‑driven state machine on the ATtiny that never blocks while the I2C bus is active.

Those changes are architectural and not part of the ATtiny firmware, which is why this ESP32 Simple Demo opts for software I2C instead.

## Build and test
1. Open the ESP32 Simple Demo sketch
2. Select your ESP32 board and COM port.
3. Upload and open Serial Monitor at 115200 baud.
4. The demo will run these automated tests via the ATtiny I2C SD-Card Module:
   - Write / size / read `TEST.TXT` (41 bytes)
   - 128‑byte stream write / size / read `STREAM.BIN`
   - Append flow on `APPEND.TXT`: write 41 bytes, append additional bytes, verify size, then read‑back

## Current test coverage
- `TEST.TXT` (write/size/read): exercises filename send, write path (`W`), size (`S`), and streaming read (`R`).
- `STREAM.BIN` (128‑byte stream): writes a repeating 16‑byte pattern to exactly 128 bytes and verifies size and read‑back.
- `APPEND.TXT` (append): writes a 41‑byte first part, appends additional bytes using append (`A`), checks the new size, and verifies the full content sequence via read (`R`).

## Helper functions (ESP32 demo)
The sketch exposes a small set of helpers that mirror the ATtiny command set:
- `send_command(char cmd)` — Send a single command byte (`'F'`, `'W'`, `'A'`, `'R'`, `'S'`).
- `write_data(const char* filename, const uint8_t* data, size_t length)` — Write a buffer to a file (creates/truncates on the ATtiny side as implemented by firmware).
- `append_data(const char* filename, const uint8_t* data, size_t length)` — Append a buffer to an existing (or newly created) file.
- `get_file_size(const char* filename)` — Return file size (32‑bit, big‑endian over I2C).
- `read_file_content(const char* filename, uint32_t size)` — Stream a file of known size.

Low‑level I2C primitives used by the helpers:
- `i2c_init()`, `i2c_start()`, `i2c_stop()`, `i2c_write_byte(uint8_t)`, `i2c_read_byte(bool ack)` with explicit clock‑stretch handling.

## Known limitations
- Software I2C is CPU‑driven and slower than hardware I2C; that’s acceptable here given the ATtiny’s SD latency.
- Single‑master only; no multi‑master arbitration support.
- If the bus becomes stuck (e.g., slave holds SDA low), add a simple bus‑recovery routine that toggles SCL up to 9 times, then issues a STOP.

This explains the rationale so future changes don’t accidentally revert to the hardware driver and reintroduce the clock‑stretching problem.