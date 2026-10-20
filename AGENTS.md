# AGENTS.md

## Repository Role

- This is the OSSM community repository. Official R+D firmware, hardware, and
  assembly documentation live in `researchanddesire/OSSM`, which is the only
  source of official firmware releases. Open changes meant for official
  firmware there.
- Builds from this repository are community firmware: unsigned and never
  official releases. Do not add release-signing keys, firmware publish tokens,
  or calls to the dashboard's internal firmware API here.
- Do not enable GitHub Pages.

## Community Firmware Publishing

- `publish_firmware.yml` builds `main` and `staging` after a reviewed merge
  and publishes to the public `ossm-firmware` bucket under `kinkymakers/`:
  V2 (16 MB) in `kinkymakers/v2/<branch>/`, and on `main` V1 (4 MB, from the
  pinned Bluetooth-capable legacy source) in `kinkymakers/v1/main/`. The
  branch root holds the latest build; each build also stays in its
  short-SHA folder. `registry_rebuild.yml` then regenerates `registry.json`.
- Never write outside `kinkymakers/` except `registry.json`. The bucket-root
  `master`, `alpha`, and `production` folders are retired official aliases.
- Supabase target: production project ref `acjajruwevyyatztbkdf`, reached only
  by these workflows with the repository secret `SUPABASE_ACCESS_TOKEN`. Every
  workflow names that ref explicitly; never infer it from a secret, URL, or
  linked CLI project.

## Branch Policy

- Persistent branches are lowercase `staging` and `main`.
- Fetch before branching. Start feature work from `origin/staging`, then open the pull request back into `staging`.
- Only urgent fixes may target `main` directly. Start them from `origin/main`, open the pull request into `main`, and make the standalone word `hotfix` the first word of the PR title (case-insensitive).
- Promote `staging` to `main` with a same-repository pull request only after every required check succeeds.
- Immediately merge every main hotfix back into `staging` with a `main` to `staging` pull request.
- Squash feature and hotfix pull requests into one focused commit with a concise, imperative title. Merge long-lived branch synchronization pull requests (`staging` to `main` and `main` to `staging`) with a merge commit; never squash or rebase them.

## Development Safety

- Preserve the dedicated TLS/update task and MQTT pause behavior. Keep update protocol, checksum, reboot, and rollback behavior covered by native tests.
- Never commit credentials, local build products, or generated secrets.
