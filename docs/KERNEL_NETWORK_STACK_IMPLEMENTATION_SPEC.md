# MicaNT — Native Networking Stack: Reuse-First Implementation Specification

**Document:** `docs/KERNEL_NETWORK_STACK_IMPLEMENTATION_SPEC.md`  
**Milestone:** M10 — Native Networking Stack (proposed)
**Status:** Implementation specification; not evidence of completed networking  
**Target:** MicaNT modular NT-compatible kernel, AMD64 only, C++23  
**Primary objective:** A real QEMU network path from `virtio-net` through the canonical MicaNT kernel transport and Winsock to a certificate-validated HTTPS client.

## 1. Guiding decision: integrate, do not rewrite everything

The fastest safe path is to preserve MicaNT's canonical NDIS/TCPIP/AFD interfaces, add the missing freestanding adapters, and **reuse a mature TCP/IP engine** where it demonstrably saves effort. Do not implement multiple competing TCP stacks, present hosted tests as native operation, or expand API coverage before packets move end-to-end.

Order of preference:

1. **Reuse MicaNT:** preserve correct code, types, ABI and lifecycle already present; complete missing hardware and runtime wiring.
2. **Port a proven library:** use a narrowly scoped and license-compatible dependency where completing MicaNT's current implementation would take longer and carry greater protocol risk.
3. **Study reference implementations:** consult ReactOS, Linux drivers and authoritative specifications for algorithms and contracts, without blindly copying their entire subsystems.
4. **Write new code only for MicaNT-specific glue:** NDIS miniport integration, NT object/handle mapping, IRQ/DPC synchronization, AFD semantics and user/kernel boundaries.

A C/C++ source file or named export is **not** evidence of functional networking. Tests must validate real packets traversing the virtual NIC and the kernel before a milestone is marked complete.

## 2. Current source inventory and known gaps

The following files existed in the inspected fork; recheck the working tree before implementation.

| Existing source | Intended role | Integration assessment / next action |
|---|---|---|
| `include/micant/km/drivers/net/ndis.hpp` | NDIS types, buffers and miniport-facing abstractions | Large hosted-heavy implementation; extract a freestanding-compatible canonical core, without retaining user-mode registrations inside Ring 0. |
| `include/micant/km/drivers/net/tcpip.hpp` | IPv4/IPv6, ARP, UDP/TCP model | Contains protocol structures and code but hosted STL, mutex and clock dependencies; audit behavior, then choose **one** transport engine. |
| `include/micant/km/drivers/net/afd_wsk_rio.hpp` and `kernel/io/afd_wsk.cpp` | NT socket endpoint / WSK layer | Thin Executive registration today; connect actual endpoints to the chosen transport engine. |
| `include/micant/km/drivers/net/i210_baremetal.hpp` | Intel I210 hardware driver | Preserve for later physical-hardware stage; don't make an I210 card a prerequisite for QEMU. |
| `include/micant/um/subsystems/net/ws2_32.hpp` | Winsock user-mode API | Bind to AFD-backed NT handles; remove simulated success paths, keep private `micant/km/` headers out of DLLs. |
| `include/micant/um/subsystems/net/winhttp.hpp` | WinHTTP HTTP client | Keep HTTP/TLS in Ring 3; refactor hosted dependencies and connect to real Winsock. |
| `include/micant/km/drivers/security/tls13.hpp` | Existing TLS model | Audit carefully; do not treat it as security-validated without certificate/crypto/protocol tests. |
| `include/micant/um/subsystems/net/dhcp.hpp`, `dnsapi.hpp`, `iphlpapi.hpp` | Network configuration and name resolution | Reuse public contracts, implement genuine packet/service paths. |
| `CMakeLists.txt` | Production source manifest | Previous inspected target `ntoskrnl_core_baremetal` did not link a production NIC, `afd_wsk.cpp` or complete TCP/IP backend; verify current target anew. |

**Rule:** no second NDIS, socket manager, transport registry or independently exposed TCP stack. A third-party engine may implement internal TCP algorithms, but all externally visible NT kernel and user-mode traffic uses the one MicaNT transport path.

## 3. Architecture and ownership

```text
Applications / browsers / services (Ring 3)
    |  Winsock / WinHTTP / DNS / Schannel-compatible ABI
    v
ws2_32.dll   dnsapi.dll   winhttp.dll   TLS provider (Ring 3)
    |  validated syscall / I/O request boundary
    v
AFD endpoint + Ob handle + Io IRP (Ring 0)
    |  bind/connect/listen/accept/send/recv/wait/cancel
    v
MicaNT TCPIP transport facade (Ring 0)
    |  one engine: adapted MicaNT core or a port of lwIP
    v
MicaNT NDIS data path (Ring 0)
    |  packet buffers, queue ownership, DPC/IRQ, DMA
    v
virtio-net miniport (QEMU); I210 / other physical NICs later
```

**HAL/Mm:** PCI discovery, BAR mapping, pinning/DMA-safe buffers, interrupt routing, timers.  
**Ke:** ISR/DPC, waits, cancel, scheduler and network timer integration.  
**Io/Ob:** device/endpoint object lifetime, access rights, IRPs and cancellation.  
**TCPIP:** packet parsing, routing, ARP/NDP, transport state and retry.  
**AFD:** NT socket semantics and asynchronous endpoint operations.  
**Ring 3:** DNS policy/cache, DHCP configuration service where feasible, TLS certificates, HTTP, proxies and URL handling.

Never hold a global network lock while executing application callbacks, waiting for an IRQ, blocking on socket I/O, or running TLS/HTTP parsing.

## 4. External reuse decision matrix

| Source | What to reuse | Mode | Recommendation |
|---|---|---|---|
| **lwIP** | Mature IPv4/IPv6, ARP/NDP, ICMP, UDP, TCP state/retransmission/timers, DHCP/DNS algorithms | Primarily a self-contained C engine behind the canonical TCPIP facade | **First candidate** for rapid initial TCP/IP availability. Official project documents a BSD license and raw API. |
| **ReactOS** | NT socket/AFD/WSK semantics, handles, IRP completion, transport integration and behavior tests | Reference comparison, selectively adopted code subject to license review | **Primary NT architectural reference**, not an instruction to import its full codebase. |
| **Linux virtio-net** | Device feature negotiation, virtqueue and buffer lifecycle, interrupt and reset behavior | Hardware-reference implementation | Study together with OASIS VirtIO specification; implement a MicaNT native driver. |
| **Mbed TLS** | TLS 1.2/1.3 core, X.509 chain verification and crypto primitives | Ring 3 TLS provider | **Port candidate** after confirming selected version, license, patch cadence and trust-store integration. |
| **BearSSL** | Compact TLS and crypto implementation | Ring 3 | Compare engineering cost and certificate requirements; select one primary TLS engine, not two. |
| **smoltcp** | Packet parsing, transport architecture and test cases | Reference | Primarily Rust; do not introduce a Rust kernel toolchain just to ship the first network stack. |
| **Windows WDK/NDIS/Winsock contracts** | Public ABI, errors, object lifetimes and behavior | Specification | Authoritative compatibility requirements; don't infer ABI from lwIP interfaces. |

External implementations remain subject to the repository's `AGENTS.md`, `CONTRIBUTING.md` and license/provenance requirements. In particular, ReactOS is GPL-licensed; copying derivative code requires a conscious compliance decision. Preserve source notices and version pins. A port must not silently weaken these constraints.

### 4.1 Choose the TCP engine by a bounded spike

Before a wholesale rewrite, compare two short paths:

- **Candidate A — MicaNT TCP engine:** isolate `tcpip.hpp` from hosted dependencies, exercise packet decoding, connection state, retransmit and timer semantics through one real NIC frame path.
- **Candidate B — lwIP:** cross-compile a pinned lwIP version and bind `sys_arch`/`netif`/`pbuf` or raw API to MicaNT Ke/Mm/NDIS. Exercise the same frame path.

Select **one** based on real link closure, ability to handle timer/locking safely, packet correctness, code maintenance and integration burden. If lwIP wins, preserve MicaNT TCPIP as the owning facade/ABI and replace or retire the conflicting internal implementation. Do not keep parallel TCP state machines in production.

**Important:** the lwIP raw API is event-driven and its callbacks must not block. Run its core under a well-defined single networking execution context until safe multiprocessor serialization exists. Do not call raw-core functions arbitrarily from ISR or multiple CPUs.

## 5. Network packet contract and driver lifecycle

Introduce or complete the existing internal packet contract, without creating a new public Windows-facing ABI:

- `NetBuffer` / `NetBufferList` ownership and explicit reference counters.
- Packet length, capacity, headroom, alignment, checksum metadata and scatter/gather descriptors.
- RX/TX completion callbacks, cancellation and safe buffer release after device reset.
- Bounded RX and TX queues with backpressure and per-interface statistics.
- Strict DMA lifetime: never expose transient stack/host pointer storage as bus-masterable memory.
- Link-up/down notifications and NIC hot/reset handling.
- Device capabilities and negotiated offloads default to **off** until implemented and proven.

The first `virtio-net` miniport supports a simple and maintainable feature set: PCI discovery, config space validation, status/feature handshake, RX/TX virtqueues, descriptor reclamation, MAC/link state, queue reset and interrupt-based notification or a documented diagnostic polling mode. Reuse the existing HAL/PCI/DMA and VirtIO infrastructure where applicable. Do not build a second HAL or memory allocator.

Reject oversized packets, malformed chains, integer overflow, unsupported features, invalid indices and unexpected device reset without corrupting ring state.

## 6. Canonical TCPIP implementation requirements

### Ethernet, ARP and IPv4/IPv6

- Ethernet II parsing, destination MAC checks, broadcast/multicast filtering and payload length validation.
- ARP requests/replies, neighbor cache timeout and bounded pending-packet queue.
- IPv4 header checksum, total length, TTL, fragmentation/reassembly safety policy and ICMP Echo.
- IPv6 header/extension validation, hop limit and NDP/SLAAC in the later IPv6 phase.
- Routing table, gateway and interface selection. No fixed local/remote IPs embedded in production.
- Defensive parsing of all user-supplied and network-supplied lengths.

### UDP

- Port binding, demultiplexing, checksum verification (mandatory for IPv6), send/recv, packet boundaries, timeout, cancellation.
- DHCP client state machine and DNS resolver layered on actual UDP, with configurable timeout/retries.

### TCP

Prefer the selected mature engine's TCP state machine. At minimum verify:

- SYN/SYN-ACK/ACK, active and passive open, duplicate SYN and reset handling.
- Sequence and acknowledgement arithmetic with wraparound.
- Congestion window, receiver window, flow control and SACK behavior according to the selected engine's supported feature set.
- Retransmission timeout, RTT estimation, delayed ACK, connection keepalive policy and zero-window probes where supported.
- FIN/RST, half-close, TIME_WAIT, duplicate and out-of-order segments.
- Bounded send/receive buffers, backpressure and connection teardown.
- Timers driven by Ke, not `std::chrono` host clocks or fake tick counters.

Do not claim full RFC compliance from a successful handshake; exercise packet loss, reordering and connection cleanup.

## 7. AFD, Ob, Io and Winsock

The data path is **one endpoint implementation**:

1. A process creates a socket through `ws2_32.dll`.
2. The DLL invokes the appropriate native I/O boundary; no `micant/km/` header or kernel object is linked into the user DLL.
3. AFD allocates an Ob-managed endpoint, binds it to one TCPIP transport control block and records access rights.
4. The operation uses Ke wait/event and Io completion/cancellation. Synchronous Winsock calls block the owning thread, not a global network worker.
5. Close and process termination cancel outstanding I/O, release packet buffers and destroy endpoint state after the last reference.

First supported Winsock subset: `WSAStartup`, `WSACleanup`, `socket`, `closesocket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`, `sendto`, `recvfrom`, `shutdown`, `getsockname`, `getpeername`, `setsockopt`/`getsockopt` for supported options, `select` (or existing wait mechanism), `ioctlsocket` non-blocking mode, `WSAGetLastError`.

Follow Windows Winsock error codes and structure/calling-convention layouts. Unsupported options fail explicitly; never map every return value to success.

After the synchronous path is stable, implement asynchronous overlapped I/O, cancellation, completion signaling, WSK and advanced RIO when required. These are not prerequisites for the first HTTPS download.

## 8. DNS, DHCP, TLS and HTTP boundaries

- **DHCP:** obtain address, mask, gateway and DNS without hardcoded values; handle lease loss and link changes. DHCP packet path may use reusable lwIP functionality or a MicaNT Ring 3 service once UDP socket operations exist.
- **DNS:** `dnsapi.dll` and resolver service in Ring 3; support A/AAAA, response validation, TTL caching, timeout and fallback. Reuse proven resolver algorithms instead of duplicating fragile DNS parsers.
- **TLS:** `schannel.dll`-compatible provider (or an internal TLS adapter behind existing user-mode contracts) using one vetted TLS library. Enforce hostname verification, expiry, trust anchors, correct time, chain building and certificate signature verification. Never ship a bypass for unknown/expired/self-signed certificates as the default.
- **HTTP:** `winhttp.dll` remains in Ring 3 and uses the real socket/TLS path. Implement HTTP/1.1 request/response, bounded header parser, chunked transfer, redirects, connection reuse and timeouts before HTTP/2 or HTTP/3.
- **HTTP.sys:** not required for a basic outbound HTTP client; a later server-side kernel interface must be a separately justified feature, not a reason to move generic HTTP parsing into Ring 0.

The project's existing TLS/WinHTTP structures are an implementation starting point, but they are not a substitute for a connected, authenticated, real network session.

## 9. M10 milestones — end-to-end rather than header expansion

### N0 — Network device foundation

**Implement:** production `virtio-net` driver, canonical NDIS packet ownership, PCI/DMA, interrupt/DPC completion.  
**Acceptance:** real QEMU TX/RX packets observed; correctly handle ring wrap, queue exhaustion, reset and malformed descriptors. No hidden canned packet responses.

### N1 — Layer 2/3 networking

**Implement:** Ethernet, ARP, IPv4, ICMP, routes, neighbor timeout.  
**Acceptance:** ping between MicaNT guest and a reachable QEMU host-side endpoint, with transmitted/received frames captured and verified. QEMU user networking may have ICMP restrictions; use an explicit TAP/socket/test-peer setup or a controlled virtual LAN where necessary rather than claiming a missing ping is a kernel failure.

### N2 — UDP + automatic configuration

**Implement:** UDP transport, DHCP and DNS.  
**Acceptance:** DHCP lease from the configured virtual network, DNS A resolution from an actual responder, data integrity and retry/timeout behavior under dropped UDP packets.

### N3 — TCP connection and reliability

**Implement:** chosen TCP engine integrated into the canonical MicaNT TCPIP interface; timeouts and full endpoint lifecycle.  
**Acceptance:** connect, send/receive bidirectionally, listen/accept, retransmit after packet loss, close without leaked PFN/handles/packet buffers; loopback and remote peer tested.

### N4 — NT socket stack and Winsock

**Implement:** AFD + Ob + Io + `ws2_32.dll` real operations.  
**Acceptance:** **native Ring 3 PE** opens TCP and UDP sockets via Winsock, sends and receives real data, observes errors/timeouts, exits and releases resources. A Ring 0 helper calling transport functions is not acceptance.

### N5 — Secure HTTP

**Implement:** Ring 3 DNS/TLS/WinHTTP, certificate validation and trust handling.  
**Acceptance:** native Ring 3 PE downloads a known HTTPS payload using `winhttp.dll` and validates length/content; rejects wrong hostname, expired certificate and unknown issuer using a controlled local TLS test endpoint. If the system clock/trust store is unavailable, do not claim secure HTTPS completion.

### N6 — Modern protocols and scale

**Implement progressively:** IPv6, ICMPv6/NDP, socket asynchronous I/O, NIC offloads, RSS/multiqueue, firewall/WFP integration, HTTP/2 and, only with a mature UDP/TLS base, QUIC/HTTP/3.  
**Acceptance:** individual protocol tests, throughput/loss measurements, multiple simultaneous connections and security checks. N6 is not a prerequisite to N0–N5.

## 10. Fast execution workflow for the coding agent

1. Read `AGENTS.md`, `docs/KERNEL_MILESTONES.md`, this specification and the exact MicaNT source files above.
2. Make a small dependency map and **choose the earliest unimplemented real packet path**. Do not audit the entire repository or add another network framework.
3. Study the relevant lwIP/ReactOS/VirtIO or TLS reference locally; record exact pinned version and the components actually reused.
4. Implement one **end-to-end vertical slice** (NIC RX/TX → frame parser → protocol operation → socket endpoint) rather than many disconnected declarations.
5. Complete all ownership, failure, callback and cleanup paths for that slice.
6. Build only impacted targets during development; run focused host packet tests and QEMU at meaningful integration boundaries, unless the user explicitly defers verification.
7. After successful milestone acceptance, advance to the next phase **within the same mandate** when the environment permits. A report or commit is a checkpoint, not the endpoint.
8. If code is temporarily not buildable, do not invent green test claims. Fix the blocker or record the precise failure and preserve the worktree.

Do not make general NT process, filesystem or GPU functionality a hidden dependency of the first ICMP and TCP milestones. Introduce only the kernel interfaces truly required for each slice.

## 11. Tests, fault injection and performance

- Packet corpus: malformed Ethernet/IP/TCP/UDP/DNS frames, truncation, bad lengths, checksum errors, fragmented and overlapping packet cases.
- State tests: TCP handshake, retransmission, out-of-order, FIN/RST, zero window, duplicate ACK, concurrent open/close and descriptor exhaustion.
- Failure injection: NIC reset during TX, memory allocation failure, canceled I/O, thread exit and process termination with open endpoints.
- Security: packet parser fuzzing where possible; incoming firewall default deny except explicitly allowed local test endpoints; no TLS certificate bypass.
- QEMU harness: provision a deterministic peer, collect packet capture, serial log and test-exit status, inject loss/delay/reorder, run native guest application.
- Accounting: measure PFN, DMA descriptors, Ob handles, TCP PCBs, packet buffers and queue counts before/after test runs.
- Performance (after correctness): TCP throughput, RTT under load, loss recovery, DPC budget, IRQ moderation, CPU overhead and effect on compositor/input latency.

Initial implementation may be single-BSP/bounded. Document such limitations; don't declare SMP or zero-copy simply because interfaces have those names.

## 12. Acceptance evidence format

For each N0–N6 milestone record in `docs/KERNEL_INTEGRATION_STATUS.md`:

- Commit or worktree revision.
- Exact production files and target in `CMakeLists.txt`.
- Actual NIC/transport/AFD/user-mode call path used.
- External code provenance, version, license and modifications.
- Commands executed and observed results (host, baremetal link, QEMU separately).
- Measured error handling and resource reclamation.
- Unsupported features and next integration dependency.

`docs/KERNEL_MILESTONES.md` should contain a concise M10 row pointing to this specification, not another competing milestone definition. Do not edit historical milestone meanings to make networking look completed.

## 13. Security and compatibility invariants

- Kernel network code executes only on validated packets and lengths; no unbounded parser allocation or sleep while IRQ/dispatch locks are held.
- Network driver DMA mappings remain valid until device completion, and device-accessible memory is never freed prematurely.
- User buffers are probed/copied/pinned as required by the canonical NT boundary; no direct kernel trust of arbitrary Ring 3 pointers.
- Firewalling and route changes are enforced at one centralized transport/Io policy boundary.
- Client libraries do not include private `micant/km/` implementation headers.
- NT ABI and error codes remain authoritative; lwIP's API and error types are implementation details, not public exports.
- A missing feature returns a documented failure, never a simulated success.
- Reused code is pinned and auditable; no silent dependency upgrades or licenses removed.
- No external network telemetry or connection attempts without explicit application/OS configuration.

## 14. Technical reference set

- Existing MicaNT code: `include/micant/km/drivers/net/{ndis,tcpip,afd_wsk_rio,i210_baremetal}.hpp`, `kernel/io/afd_wsk.cpp`, `include/micant/um/subsystems/net/{ws2_32,winhttp,dnsapi,dhcp}.hpp`.
- lwIP overview, raw API and license: https://www.nongnu.org/lwip/ and https://www.nongnu.org/lwip/2_1_x/group__api.html
- lwIP source: https://git.savannah.gnu.org/cgit/lwip.git ; mirror https://github.com/lwip-tcpip/lwip
- ReactOS: https://github.com/reactos/reactos (AFD, TCPIP/NDIS behavior, process/IRP lifecycle; inspect exact version).
- OASIS VirtIO: https://docs.oasis-open.org/virtio/virtio/v1.3/virtio-v1.3.html
- Mbed TLS source, porting and license: https://github.com/Mbed-TLS/mbedtls
- Microsoft WDK: NDIS miniport, AFD/WSK and native I/O documentation; Microsoft Learn Winsock/WinHTTP/Schannel references.
- Protocol specifications: RFC 9293 (TCP), RFC 8200 (IPv6), RFC 768 (UDP), RFC 826 (ARP), RFC 2131 (DHCP), RFC 1034/1035 (DNS), RFC 8446 (TLS 1.3), RFC 9110/9112 (HTTP semantics / HTTP/1.1).

## 15. Definition of done for M10

M10's **initial functional closure (N0–N5)** requires one repeatable native QEMU scenario:

1. Guest discovers and operates `virtio-net` through MicaNT NDIS.
2. Guest receives DHCP settings and resolves a DNS name.
3. Guest process creates a Winsock socket, connects and exchanges actual TCP data.
4. Guest process performs HTTPS via WinHTTP with correct trust and hostname validation.
5. All connections, threads, DMA buffers, Ob objects and PFN allocations are released at shutdown.
6. Packet loss and malformed input do not crash or deadlock the kernel.

**A completed hosted simulation, a header-only network library, a responding ping helper, or HTTP code that fabricates a response does not meet this definition.**

The guiding metric is not the number of network APIs declared; it is the number of real native guest operations that successfully cross **Win32 → NT I/O → TCPIP → NDIS → hardware** and return with correct state and error semantics.
