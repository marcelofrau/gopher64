# Agent instructions

- Read `CLAUDE.md` and `docs/xbox-port/memory-handoff.md` before working on the Xbox port.
- After every interaction, append an English entry to `docs/xbox-port/memory-handoff.md` with the work or decisions, evidence, failed attempts, tests, and remaining work. Mark untested items explicitly; never include secrets, console addresses, or machine-specific absolute paths.
- Commit the agent's own changes in logical increments, including the handoff update. Do not stage unrelated user work or push unless the user asks.
- Never connect to, deploy to, or launch on the user's Xbox. Only the user performs console deployment or F5; agent tests run on PC.
