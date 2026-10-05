## Description of Changes
<!-- Provide a clear, detailed technical summary of what this Pull Request introduces, fixes, or refactors. -->

---

## Clean-Room Provenance & Compliance Checklist
<!-- ALL contributors must review and check the following: -->
- [ ] **Clean-Room Non-Contamination Affirmation**: I certify that this code was authored cleanly from first principles or public specifications without referencing leaked Windows source code (NT4, Win2k, WRK) or proprietary disassembler dumps (IDA Pro / Ghidra).
- [ ] **Interface Attribution**: All Win32 / NT API signatures and types trace directly to Microsoft's MIT-licensed `microsoft/win32metadata` repository or public Microsoft Learn / MSDN documentation.
- [ ] **C++23 Standards**: Code adheres to modern ISO C++23 standards (RAII, zero raw pointer leaks, smart pointers, atomics).
- [ ] **Test Coverage**: Tested against MicaNT test battery with 100% pass rate (`bin/micant_tests.exe`).

---

## 🔷 Microsoft Employee / OSPO Corporate Contribution Section
<!-- Complete this section ONLY if you are contributing on behalf of Microsoft Corporation or as a Microsoft employee: -->
- [ ] **Microsoft Contributor**: This contribution is submitted by a Microsoft employee or on behalf of Microsoft Corporation under corporate open-source authorization.
- **Author Email**: `alias@microsoft.com`
- **MS-OSPO Approval Ticket / Tracking Ref**: `[e.g. OSPO-2026-xxxxx]`
- **Microsoft Legal Clearance Ref (LCA)**: `[e.g. LCA-OSS-xxxxx]`
- **Commit Trailers Included**: Verified commit headers contain `MS-OSPO-Approved` and `Signed-off-by`.

---

## Related Issues / Discussions
<!-- Link any related issues, discussions, or specifications: -->
Closes #
