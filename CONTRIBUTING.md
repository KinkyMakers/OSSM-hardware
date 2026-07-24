# Contributing to OSSM (Open Source Sex Machine)

Welcome to the OSSM project, run by the Kinky Makers! We’re excited to have you
contribute to the world’s most open, hackable, and community-driven sex machine.
Please read this guide to help you get started and make your contributions as
smooth as possible.

## 📚 Project Overview

- **Project Home:**
  [Kinky Makers OSSM Documentation](https://docs.researchanddesire.com/open-source-sex-machine/software/docs)
- **Main Repository:** This repo contains hardware, software, and documentation
  for OSSM.
- **Community:** [Join our Discord](https://discord.gg/MmpT9xE) for support,
  discussion, and collaboration.

## 🚀 Getting Started

1. **Fork the repository** and clone it to your local machine:
   ```bash
   git clone https://github.com/YOUR-USERNAME/OSSM-hardware.git
   cd OSSM-hardware/Software
   ```
2. **Install dependencies:**
   - For firmware: [PlatformIO](https://platformio.org/) (recommended) or
     Arduino IDE
   - For hardware: See the [Bill of Materials](../README.md#bill-of-materials)
3. **Set up your environment:**
   - PlatformIO: Open the `Software` folder in VSCode and install the PlatformIO
     extension.
   - Arduino IDE: Open the `.ino` file in `src/` and copy libraries from `lib/`
     to your Arduino libraries folder.
   - See [Software/README.md](Software/README.md) for more details.

## 🧑‍💻 Coding Standards

- **C++ Formatting:**
  - Code must be formatted using `clang-format` (see `.clang-format` in
    `Software/`).
  - Pre-commit hooks are set up via `.pre-commit-config.yaml` to enforce
    formatting. Install with:
    ```bash
    pip install pre-commit
    pre-commit install
    ```
- **EditorConfig:**
  - The project uses `.editorconfig` for consistent indentation, line endings,
    and file encoding.
- **General Guidelines:**
  - Write clear, descriptive commit messages.
  - Comment your code where necessary.
  - Follow existing code structure and naming conventions.

## 🧪 Testing

- **Test Framework:** Unity (for C++)
- **Test Structure:**
  - Add new tests in `Software/test/` as a new directory: `test_<feature>`
  - Each test directory should have a `main.cpp` file. See
    [Software/test/README.md](Software/test/README.md) for a template.
- **Running Tests:**
  - From the `Software` directory, run:
    ```bash
    pio test -e test
    ```

## 🌳 Branching & Pull Requests

- Fetch `origin`, branch feature work from `origin/staging`, and open the pull
  request back into `staging`.
- Reserve direct production hotfixes for branches created from `origin/main`.
  Open those pull requests into `main` and begin the PR title with the standalone
  word `hotfix` (case-insensitive).
- Run the relevant tests and reference related issues in the PR description.
- Keep commits focused. Feature and hotfix PRs are squash-merged into one commit
  with a concise, imperative title.
- Promotions from `staging` to `main` and hotfix synchronization from `main` to
  `staging` use merge commits so the long-lived branches retain shared ancestry;
  never squash or rebase these synchronization PRs.
- Be responsive to code review feedback.

## 🛟 Getting Help

- **Documentation:**
  [Project Docs](https://docs.researchanddesire.com/open-source-sex-machine/software/docs)
- **FAQ:** [FAQ.md](FAQ.md)
- **Discord:** [Join our Discord](https://discord.gg/MmpT9xE)
- **Issues:** If you find a bug or have a feature request, open an issue in the
  repo.

## 💡 Tips for Contributors

- Be respectful and inclusive—this is a diverse, welcoming community.
- If you’re unsure, open a draft PR or ask in Discord before spending lots of
  time on a big change.
- Hardware, software, and documentation contributions are all welcome!

Thank you for helping make OSSM better for everyone!
