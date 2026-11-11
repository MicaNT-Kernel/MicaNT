# Repository agent instructions

Follow the repository-wide engineering and compatibility rules in [`AGENT.md`](AGENT.md), along with `CONTRIBUTING.md` and the subsystem documentation relevant to the change.

## Single modular kernel policy

MicaNT has one canonical modular kernel. All kernel subsystem development must target the modular `ntoskrnl_core` architecture and its bare-metal entry point. The archived monolithic kernel under `old/` is historical reference only and must never be built, linked, staged, or used to satisfy missing symbols.
