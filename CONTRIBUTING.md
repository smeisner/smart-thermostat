# Contributing to the Project

Thank you for your interest in contributing! This project is dedicated to building hardware and software that gives complete, independent control to the device owner. To protect this vision and keep the system secure, reliable, and clean, all contributors must follow these guidelines.

---

## 1. Ground Rules & Philosophical Alignment

Before writing any code or designing hardware modules, ensure your work strictly aligns with our foundational tenets:
* **Zero Cloud Dependencies:** All features must execute locally on the hardware. Any pull request (PR) that introduces telemetry, mandatory internet-facing cloud sync, or external tracking will be rejected.
* **Intuitive User Interface:** The user interface must be self-sufficient. Users should never have to parse logs or read raw code to understand an error. If your PR introduces a new system state, you must also provide clear UI elements that explain the situation and provide an immediate remediation path for a non-technical user.

---

## 2. Pull Request & Code Standards

To keep the repository clean and maintainable for the long term, we enforce strict review criteria:

### Code Cleanliness and Compilation Warnings
* **No Messy Code:** Unorganized, undocumented, sloppy, or hastily written code will be rejected immediately. 
* **Readability over Cleverness:** Write expressive, well-structured, and clean code. Follow established idiomatic patterns for the specific language or framework used in this repository.
* **Compilation Warnings:** While a completely warning-free compilation is ideal, we understand some warnings are permissible. However, you must disclose and justify any compilation or linter warnings in your pull request description. Maintainers evaluate warnings on a case-by-case basis and retain the full right to push back on or reject PRs with unresolved, sloppy, or risky warnings.

### Use of AI-Assisted Tools
* You are fully permitted to use AI generation tools (e.g., Copilots, LLMs) to assist you in drafting software or configurations.
* **The accountability rule:** As the human author of the PR, you assume 100% responsibility for the code. You must be able to fully explain, defend, and document every line and detail of the committed code during the peer review process. If you cannot explain it, it will not be merged.

### Long-Term Maintainability
* Code must be engineered for long-term survival. If a submission introduces unnecessary complexity or an excessive footprint of external dependencies, it will be refused or returned to you for refactoring.

---

## 3. How to Contribute

### Step 1: Find or Open an Issue
* Search the issue tracker to see if your feature idea or bug fix is already being discussed.
* If it is a new proposal, open an issue first to discuss the architectural approach with the project Maintainers or Founder-Leader before writing extensive code.

### Step 2: Make Your Changes
* Fork the repository and create a descriptive feature branch (e.g., `feature/intuitive-wifi-error` or `fix/display-flicker`).
* Keep your commits focused. Avoid mixing unrelated changes (e.g., updating a UI layout and fixing an unrelated driver bug) inside a single pull request.

### Step 3: Test Locally
* Ensure your changes do not break any existing local-first behaviors.
* Verify that any new UI states display errors gracefully and intuitively, completely eliminating the need for a user to look at terminal outputs.

### Step 4: Submit a Pull Request
* Fill out the PR template completely.
* Explicitly state how your code preserves **System Integrity** and how it keeps the user independent of cloud ecosystems.
* Be prepared for active peer review.

---

## 4. Community Standards
By participating in this project, you agree to abide by our project's **Code of Conduct** at all times. Please treat all community members with empathy and respect.
