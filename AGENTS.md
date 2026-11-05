# Agent instructions

- Read `CLAUDE.md` and `docs/xbox-port/memory-handoff.md` before working on the Xbox port.
- Identify the current machine at the start of work (`$env:COMPUTERNAME` in PowerShell; otherwise `hostname`). Use the reported name in machine-specific memories and handoff entries; if unavailable, record it as unknown. Do not infer machine capabilities from the agent or model name.
- After every interaction, append an English dated entry to `docs/xbox-port/memory-handoff.md` with the machine name, work or decisions, evidence, failed attempts, tests, and remaining work. Mark untested items explicitly; never include secrets, console addresses, or machine-specific absolute paths.
- Commit the agent's own changes in logical increments, including the handoff update. Do not stage unrelated user work or push unless the user asks.
- Never connect to, deploy to, or launch on the user's Xbox. Only the user performs console deployment or F5; agent tests run on PC.
