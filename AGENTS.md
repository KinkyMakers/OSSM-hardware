# AGENTS.md

## Branch and Release Policy

- Persistent branches are lowercase `staging` and `main`.
- Start feature and release work from `staging` and merge it into `staging` first.
- Promote normal releases with a `staging` to `main` pull request only after every required build, unit, integration, artifact-verification, and OSSM hardware validation succeeds.
- Only urgent production bug fixes may target `main` directly. Merge each main hotfix back into `staging` immediately.
- Use matching branch names and linked pull requests for changes spanning RAD App or another firmware repository.
- Staging firmware reports the `staging` track and checks `https://staging.researchanddesire.com`. Main firmware reports `main` and checks `https://dashboard.researchanddesire.com`.
- Firmware is update-eligible only after its immutable Supabase artifacts and all required validation records verify. Missing hardware runners leave a release in `validating`; never bypass a gate.

## Development Safety

- Preserve the dedicated TLS/update task and MQTT pause behavior. Keep update protocol, checksum, reboot, and rollback behavior covered by native tests.
- Never commit credentials, local build products, or generated secrets.
