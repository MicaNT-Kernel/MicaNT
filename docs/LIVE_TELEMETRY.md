# MicaNT Bare-Metal Live Silicon Telemetry

**Last Updated**: `2026-10-18 20:21:23` (`2026-10-19T03:21:23.578393+00:00`)  
**Server Target**: `147.135.1.229` (Supermicro X11SSH-F / Intel S1200SP, Intel Xeon E3-1270 v6 @ 3.80 GHz)  
**Overall Status**: **`HEALTHY`** (0 Faults, 0 Panics)

---

## 1. Real-Time Hardware Performance

| Metric | Live Silicon Value | Target / Reference | Health Status |
| :--- | :--- | :--- | :--- |
| **CPU Utilization** | **`12%`** | < 25% @ 60 FPS | **OPTIMAL (Green)** |
| **Frame Render Time** | **`16.1 ms`** | 16.66 ms (60 FPS) | **LOCKED (Stable)** |
| **DTS Thermal (On-Die)** | **`40 °C`** | < 65.0 °C (Kaby Lake) | **CHILLY (Normal)** |
| **RAM Utilization** | **`78 MB / 32455 MB`** | Sub-128 MB Footprint | **OPTIMAL (0.24% allocated)** |
| **TSC Clock Speed** | **`3794 MHz`** | 3794 MHz Base | **EXACT** |
| **APIC Topology** | **`4C/8T (8 APICs)`** | 8 Logical Processors | **NORMALIZED** |
| **Storage Subsystem** | **`2x HGST Ultrastar 2000GB GPT`** | AHCI BAR5 `0xA2D18000` | **ONLINE** |
| **PCIe Bus Enumeration** | **`15 Devices OK`** | Host, xHCI, SATA, Bridges | **DISCOVERED** |
| **Executive Suites** | **`341/341`** | 100% Pass Rate | **PASSED** |
| **Active Subsystems** | **`44 PE32+ DLLs`** | Horizon 118 UI, PrismX | **ACTIVE** |

---

## 2. Live Task Manager & Silicon Oscilloscope View
![Live Task Manager](live_taskmgr_latest.png)

*Captured directly from ASPEED AST2400 GOP video stream over BMC JViewer KVM.*

---

## 3. Alerts & Watchdog Status
- **Active Alerts**: 0
- ✅ **All silicon vitals, frame pacing, and thermal boundaries within nominal specifications.**
