# Agent unit-test mount fixture

This directory must exist for the AgentSession tests: session-start validation
resolves both the configured work directory and the requested C mount through
normal path validation. The fake runtime does not execute AGENTFIX.COM.
No emulator binary or generated guest files are required here for unit tests.
