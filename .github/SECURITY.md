# Security Policy for MicaNT

MicaNT takes systems security, memory safety, and cryptographic integrity seriously. As a modern C++23 sovereign operating system executive, our goal is to provide a deterministic, zero-telemetry, memory-safe foundation for computing.

---

## 1. Supported Versions

Security advisories, fixes, and updates are applied to the active development branches:

| Version / Branch | Supported | Notes |
| :--- | :--- | :--- |
| `main` (Latest) | :white_check_mark: | Active development & bleeding-edge kernel tree |
| Release tags | :white_check_mark: | Official release milestones |

---

## 2. Reporting a Vulnerability

If you discover a security vulnerability (such as a memory corruption issue, privilege escalation flaw, cryptographic weakness, or sandbox escape):

### Preferred Method: GitHub Private Vulnerability Reporting
Please submit your report through **[GitHub Private Vulnerability Reporting](https://github.com/MicaNT-Kernel/MicaNT/security/advisories/new)**. This ensures the report remains confidential while we investigate and prepare a patch.

### What to Include in Your Report:
- Detailed technical description of the vulnerability.
- Steps to reproduce or proof-of-concept (PoC) code.
- Affected subsystem (e.g. `NexusOB`, `DaytonaMM`, `SentinelSec`, `AegisSandbox`, `EmeraldCrypt`).
- Any potential impact on system stability or confidentiality.

### Response SLA:
- **Acknowledgment**: Within 48 hours of initial submission.
- **Triage & Remediation Plan**: Within 7 business days.
- **Public Disclosure**: Coordinated after a fix is verified and merged into `main`.

---

## 3. Security Design Commitments

1. **Zero Telemetry Guarantee**: Security fixes will never introduce background telemetry daemons, phone-home beacons, or user tracking.
2. **Clean-Room Integrity**: Security patches must adhere strictly to MicaNT's clean-room non-contamination principles. We will not accept patches containing proprietary or leaked source code under any circumstances.
3. **Memory Safety**: MicaNT enforces modern ISO C++23 memory safety abstractions (RAII, bounds checking, typed handles, atomic synchronization).
