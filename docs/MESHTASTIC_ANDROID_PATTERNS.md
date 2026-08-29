# Patterns extracted from meshtastic/Meshtastic-Android

The `vendor/meshtastic-android` submodule was cloned to check for reusable
patterns for a companion-app / any-device story, not because we need the
Android app itself. Findings below; the submodule was then removed —
nothing here requires keeping the full 67MB checkout around.

**License note:** Meshtastic-Android is GPLv3. Neither `omokoda-mesh` nor
any sibling repo has a LICENSE file yet — pick one before copying any
actual code from this project verbatim (GPLv3 code brings copyleft
obligations with it). Everything below is a *pattern* description, not
copied source, so it's safe to keep regardless of what license we settle
on for our own repos.

## The relevant finding: it's already Kotlin Multiplatform, not Android-only

Despite the repo name, this is mid-transition to KMP explicitly to "decouple
business logic from Android for multi-platform (Android, Desktop, iOS)"
(their own `AGENTS.md`). Source sets: `commonMain` (shared logic, no
`android.*`/`java.*` imports allowed), `androidMain`, `iosMain`, `jvmMain`
(desktop). This is the direct precedent for "runs off any compatible
device" at the companion-app layer, not just the firmware layer.

## `RadioTransport` interface (`core/repository/.../RadioTransport.kt`)

One small interface every hardware transport implements — BLE, Serial/USB,
TCP all conform to the same shape:

```kotlin
interface RadioTransport {
    fun handleSendToRadio(p: ByteArray): Boolean   // non-blocking admission + async enqueue
    fun start() {}
    fun keepAlive() {}                              // zombie-connection detection
    suspend fun close()                              // must use NonCancellable for teardown
}
```

Concrete implementations live in `core/network/.../radio/`:
`BleRadioTransport.kt`, `TcpRadioTransport.kt`, plus Android-specific
`SerialRadioTransport.kt` (USB serial via `UsbManager`). There's also a
`NopRadioTransport` (no-op, for tests) and a `MockRadioTransport` /
`ReplayRadioTransport` (deterministic test fixtures) — worth mirroring:
a fake transport that can replay a captured session is much easier to
test against than real hardware every time.

## Why this matters for omokoda-mesh

The firmware side (`omokoda-mesh`, `omokoda-mesh-firmware`) already has an
analogous split: `RadioHAL` interface + per-radio drivers
(`include/hal/radio_hal.h`, `lib/hal_lora_sx1262`, etc.). The
Meshtastic-Android pattern is the same idea one layer up — if/when a
companion app (phone, desktop, or gateway UI) gets built to talk to
omokoda-mesh nodes, it should follow this same shape: one
`RadioTransport`-equivalent interface, concrete BLE/Serial/TCP
implementations, KMP source-set split so the same business logic runs on
Android, iOS, and desktop without forking it three times. Don't invent a
different shape — this one is proven and already handles the annoying
edge cases (GATT cache invalidation, BLE reconnect policy, cancellable
teardown).

## What we did NOT extract

- Compose UI / Material3 theming — irrelevant, our companion app (if any)
  isn't built yet and shouldn't assume Compose.
- Firebase/Crowdin/CI tooling — project-specific, not a pattern.
- Their protobuf models — we have our own event schema
  (`docs/EVENT_KINDS.md`); no reason to adopt theirs at the app layer.

## Status

No companion app exists yet in this ecosystem. This doc is here so the
`RadioTransport`-equivalent shape gets reused instead of re-derived from
scratch whenever that work starts.
