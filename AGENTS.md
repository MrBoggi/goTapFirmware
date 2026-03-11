# AGENTS.md - Context & Rules for AI Agents

Welcome to **goTapFirmware**. You are assisting in the development of firmware for an ESP32-S3 tap tower display, built as part of the goTapList ecosystem.

---

## 🎯 Project Overview
- **Core Goal**: Display beer logos and allow touch-based pour registration on a 1.75" AMOLED screen on each tap.
- **Hardware**: ESP32-S3 with a round capacitive touch screen.
- **API Backend**: goTapList HTTP REST API.
- **Framework & Libraries**: Arduino Framework, PlatformIO, LVGL (Light and Versatile Graphics Library).

---

## 🤖 Agent Configuration

For this project, we utilize specialized agent roles to ensure high quality and consistency.

### Skill: Architect (Planning)
- **Role**: Analyze Git Issues and create detailed implementation plans.
- **Model**: `gemini-3.1-pro-high`
- **Responsibility**: Generate step-by-step plans in `ISSUE_TEMPLATE` or `implementation_plan.md`.

### Skill: Coder (Implementation)
- **Role**: Write code based on an approved plan.
- **Model**: `gemini-3-flash`
- **Responsibility**: Execute file changes, handle logic, and create Pull Requests.

### Skill: Reviewer (Validation)
- **Role**: Validate that the code matches the plan and follows all project rules.
- **Model**: `claude-4.6-sonnet-thinking`
- **Responsibility**: Perform code reviews and verify tests/builds.

---

## 🛠 Technology Stack & Absolute Rules

### 1. C++ / ESP32 (Arduino Framework) Guidelines
- **Memory Management**: ESP32 has limited RAM. Avoid dynamic memory allocation (e.g., heavily mutating `String` objects, `new`, `malloc`) in loop-heavy code. Use `char` arrays (`snprintf`), `std::array`, or statically allocated buffers where possible.
- **Non-blocking Code**: Never use `delay()` for long logical delays. Use `millis()`-based state machines or FreeRTOS tasks/timers to keep `loop()` responsive, especially for LVGL UI updates and network operations.
- **Interrupts (ISRs)**: Keep Interrupt Service Routines as short as possible. Use flags or FreeRTOS queues to defer processing to the main loop. Make sure variables modified in ISRs are declared `volatile`.
- **PlatformIO Structure**: 
  - `src/`: Main source files (`.cpp`).
  - `include/`: Header files (`.h`).
  - Dependencies are managed in `platformio.ini` via `lib_deps`.
- **Configuration**: Use `config.h` for sensitive data (SSID, keys) and system-specific definitions. Never hardcode credentials in source files. `config.h` must remain git-ignored.

### 2. Git Workflow (MANDATORY)
Alt git-arbeid SKAL utføres i henhold til beste praksis og med faste strategier:
- **Aldri commit direkte til faste grener som `main` eller `development` (med mindre det er avtalt ved oppsett).**
- **Branching**: Bruk faste prefix for branches: `feature/`, `fix/`, `refactor/`.
- **Commits**: Bruk Conventional Commits (e.g., `feat: ...`, `fix: ...`, `docs: ...`).

### 3. Terminal & Commands
- **Execution**: Wait for commands to finish before proceeding.
- **Errors**: If a command fails (e.g., compilation error, merge conflict), **STOP** and resolve it or ask for guidance. Do not guess.
- **Build & Verify**: Always verify your code by running PlatformIO build commands (e.g. `pio run`) to ensure it compiles for the ESP32.

### 4. General Agent Rules
- **No placeholders**: Write complete, working code. Never output `// ... rest of code`.
- **Conventions**: Emulate existing style and structure. Follow C++ naming conventions.
- **Proactivity**: Be proactive, but if a requirement is ambiguous, stop and ask the user for clarification rather than making assumptions.

---

## 📁 Available Skills

| Skill | Description | Location |
| :--- | :--- | :--- |
| **Git Workflow** | Streng arbeidsflyt for branscher og PRs | `Skills/git-workflow/SKILL_git-workflow.md` |
| **Planner** | Genererer klare, atomiske sjekklister | `Skills/planner/SKILL_planner.md` |
| **Clean Code** | Retningslinjer for ren og vedlikeholdbar kode | `Skills/clean-code/SKILL_clean-code.md` |
| **Skill Creator** | Hjelper med å lage nye skills | `Skills/skill-creator/SKILL_skill-creator.md` |
| **ESP32 C++ Expert** | Spesifikk ekspertise for ESP32, FreeRTOS, LVGL og PlatformIO | `Skills/esp32-cpp-expert/SKILL_esp32-cpp-expert.md` |

---

"Brew smarter, code safer. 🍺"
