# ZeroKernel TODO

Tujuan file ini:

- mencatat trouble teknis yang masih ada
- menjaga arah pengembangan tetap rapi
- menjadi backlog kerja sebelum eksekusi fitur besar berikutnya

Status saat ini:

- core kernel sudah berjalan dan teruji di desktop, ESP8266, dan ESP32
- Wemos tersedia untuk hardware test aktif
- fokus berikutnya harus tetap portable untuk semua target, bukan vendor-specific

## Selesai Di Batch Terbaru

1. Governance runtime dasar

- `abiVersion()` dan `runtimeVersion()` sudah ada.
- `KernelState` resmi sudah ada.
- `triggerPanic()` sudah ada.
- `IdleStrategy` resmi sudah ada.
- `getTimingReport()` sudah ada.

2. Runtime supervision dasar

- `ExecutionContract` dasar sudah ada.
- callback `onStateChange()` sudah ada.
- panic path sudah terhubung ke fault kritikal tertentu.

3. Optimasi low-level

- helper atomic ringan sudah ada via builtin compiler.
- idle instruction low-level sudah ada via internal arch helper.
- cycle counter helper berbasis ASM sekarang sudah ada untuk target Xtensa.

4. Optimasi RAM dan hot path

- deferred queue tetap key-based.
- task/subscriber/command handler slot tidak lagi menyimpan salinan label tetap 24 byte.
- fast subscription / fast command registration API sekarang sudah ada.
- build tanpa legacy label API sekarang juga tidak menyimpan pointer label di slot subscriber/typed subscriber/command handler.
- `EventValue` sekarang memakai union payload, jadi queue/event/work item tidak lagi membawa semua varian payload sekaligus.
- profile kecil sekarang bisa mematikan extended per-task metrics, sehingga lag/per-task deadline bookkeeping tidak lagi menghabiskan storage per task.
- diagnostics sekarang dipisah ke layer `src/diagnostics`, dengan gate build terpisah supaya profile kecil tidak membawa formatter dump.
- `run_resource_matrix.sh` sekarang mencetak budget PASS/FAIL agar regression footprint lebih mudah dijaga.
- fitur yang dimatikan sekarang tidak lagi membawa dummy storage slot besar untuk command/work/trace tables; build kecil hanya menyisakan stub minimal.
- ada `run_desktop_lean_smoke.sh` untuk menjaga jalur `POWER_SAVE` tetap teruji.
- `run_wemos_compare.sh` sekarang bisa menegakkan gate determinism (`fast_miss/lag` harus tetap nol).
- `run_wemos_level2.sh` sekarang mengembalikan board ke `KernelIdentity` otomatis setelah stress selesai.
- mode `TOPIC_KEY_ONLY` sekarang sudah menjadi jalur wrapper yang lebih tegas untuk build lean.
- benchmark desktop sekarang punya soft performance gate opsional.

## Belum Dikerjakan (Dan Alasannya)

1. Rewrite total ke C

- Tidak dikerjakan karena cost maintenance terlalu besar dan ROI teknis rendah saat ini.
- Pendekatan yang dipilih adalah mempertahankan API C++ dan mengencangkan hot path/internal layer.

2. Rewrite luas ke ASM

- Tidak dikerjakan karena akan merusak portability lintas MCU.
- ASM hanya dipakai di titik yang benar-benar bernilai: idle hint dan cycle counter helper.

3. Capability system

- Belum dikerjakan karena governance/runtime supervision dasarnya masih lebih penting.
- Capability akan lebih masuk akal setelah policy state/contract lebih matang.

4. Persistent fault counter / safe-mode boot

- Belum dikerjakan karena itu butuh storage/RTC target-specific dan berisiko bikin desain terlalu board-specific terlalu cepat.

5. Topic-key-only runtime sepenuhnya

- Belum selesai penuh.
- Fast registry path sudah ada, tetapi legacy string API masih dipertahankan agar kompatibilitas tetap aman.
- Build kecil sekarang jauh lebih dekat ke mode topic-key-first karena storage label subscriber sudah benar-benar dipangkas.

## Trouble Saat Ini

1. Footprint masih bisa diperkecil

- RAM sudah cukup baik, tetapi flash masih mudah naik saat fitur baru ditambahkan.
- Jalur legacy string API masih ikut membawa beban compatibility di build kecil.
- Diagnostics sudah bisa dimatikan, tetapi pemisahan core vs diagnostics masih belum benar-benar tegas.

2. Dispatch masih campuran antara mode lama dan mode cepat

- `TopicKey` fast path sudah ada.
- Build dengan legacy API aktif masih menyimpan label string.
- Jalur string compare masih ada untuk kompatibilitas.
- Potensi optimasi berikutnya adalah `topic-key-only subscription` agar storage dan hot path lebih ringan.

3. State governance kernel belum formal

- Safe mode sudah ada.
- State model resmi sekarang sudah ada: `BOOT`, `NORMAL`, `DEGRADED`, `SAFE_MODE`, `RECOVERY`, `PANIC`.
- Callback state transition sekarang sudah ada.
- Langkah berikutnya adalah memperjelas policy transisi untuk semua jalur fault.

4. Panic path belum ada

- Saat ini ada watchdog, fault counter, recovery, dan safe mode.
- Tetapi sebelumnya belum ada jalur panic resmi; sekarang `triggerPanic()` sudah ada.

5. Execution contract belum formal

- Sudah ada elemen dasar: `priority`, `maxRuntimeMs`, watchdog, safe mode.
- Execution contract dasar sekarang sudah ada:
  - `critical`
  - `allowDegrade`
  - `dropIfLate`
  - `maxRuntimeUs`
- Yang masih perlu diperdalam adalah enforcement policy yang lebih kaya.

6. Timing report belum resmi

- Banyak metrik runtime sudah tersedia.
- `getTimingReport()` sekarang sudah ada.
- Langkah berikutnya adalah menambah laporan jitter yang lebih detail dan format binary snapshot.

7. ABI dan manifest belum ada

- Runtime version sudah ada.
- ABI version formal sekarang sudah ada.
- `zerokernel.manifest.json` sekarang sudah ada.
- Langkah berikutnya adalah menjaga manifest tetap sinkron dengan profile build.

8. Reliability claim belum tertulis resmi

- Sifat-sifat penting kernel sudah ada.
- Dokumen reliability dasar sekarang sudah ada.
- Langkah berikutnya adalah memperluasnya menjadi matriks claim vs feature.

9. Low-level hooks belum lengkap

- Idle hint low-level sudah mulai ada di adapter.
- Atomic helper/arch helper masih perlu difinalkan dengan rapi.
- Belum ada abstraction resmi untuk:
  - idle strategy
  - arch-specific cycle counter
  - critical section ringan

## Yang Sudah Mulai Diarahin, Tapi Perlu Diputuskan Dulu

1. C / ASM usage

- Dipakai hanya di area yang memang bernilai:
  - idle instruction (`WFI`)
  - atomic flags/counters
  - cycle/timing helpers
- Tidak untuk rewrite total core.
- Prinsip: core tetap portable, bagian low-level dipindah ke adapter/internal arch layer.

2. Safe mode

- Sudah ada sebagai mekanisme runtime ringan.
- Perlu diputuskan:
  - apakah tetap sederhana
  - atau dinaikkan jadi state machine penuh

3. Work queue dan event flags

- Sudah ada.
- Perlu diputuskan apakah ingin diposisikan sebagai primitive resmi kernel, lalu didokumentasikan lebih kuat.

## Rencana Kerja Yang Disarankan

Fase 1: Governance dan identitas runtime

1. Selesai: tambah `abiVersion()`
2. Selesai: tambah `runtimeVersion()` yang eksplisit
3. Selesai: tambah `zerokernel.manifest.json`
4. Selesai: tambah docs `Reliability Guarantees`
5. Selesai: formalisasi `Idle Strategy Hook`

Fase 2: Runtime supervision

1. Selesai: implement `Execution Contract` dasar
2. Selesai: tambah `KernelState` resmi
3. Selesai: tambah `onStateChange()`
4. Selesai: tambah `triggerPanic(reason)`
5. Tambah policy panic:
   - freeze
   - safe mode
   - dump
   - reboot callback

Fase 3: Determinism dan observability

1. Selesai: tambah `getTimingReport()`
2. Tambah worst-case tick metrics
3. Tambah average task execution metrics
4. Tambah runtime jitter report
5. Tambah optional binary telemetry snapshot

Fase 4: Footprint dan speed optimization

1. Tambah `topic-key-only subscription`
2. Selesai: jadikan legacy string API opsional lewat compile-time flag
3. Pisahkan diagnostics dari core lebih tegas
3. Selesai: diagnostics dipisah ke folder/compile gate tersendiri
4. Tambah low-footprint profile yang lebih agresif
5. Tambah benchmark compare untuk:
   - legacy path
   - fast path
   - stripped build

Fase 5: Long-term platform features

1. Capability system
2. Per-task recovery policy
3. Persistent fault counter
4. Safe-mode boot policy
5. Optional target-specific watchdog bridges tambahan

## Hal Yang Jangan Dilakukan Dulu

1. Rewrite total ke C

- Nilainya belum sebanding dengan biaya maintenance.
- Lebih masuk akal memperketat hot path yang benar-benar penting.

2. Menambah fitur “terlihat keren” tapi belum teruji

- Fokus harus tetap ke fitur yang memperkuat reliability, determinism, portability, dan footprint.

3. Membuat ZeroKernel jadi RTOS penuh

- Posisi yang kuat untuk ZeroKernel adalah:
  - lebih kuat dari scheduler biasa
  - lebih ringan dan lebih mudah dari RTOS penuh

## Keputusan Diskusi Yang Perlu Kita Sepakati

Sebelum implementasi batch berikutnya, kita perlu sepakat:

1. Apakah prioritas berikutnya adalah `governance/runtime supervision` atau `size/speed optimization`?
2. Apakah `safe mode` ingin tetap minimal atau dinaikkan menjadi `state machine` resmi?
3. Apakah legacy string API tetap dipertahankan default, atau kita mulai dorong model `fast path first`?
4. Apakah `triggerPanic()` boleh memicu reboot callback, atau harus tetap non-destructive default?
5. Apakah kita ingin `C/ASM` dipakai hanya di adapter/internal layer, tanpa masuk ke public API?
