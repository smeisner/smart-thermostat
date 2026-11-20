# Project Governance

## 1. Core Philosophy and Intent
The fundamental objective of this project is to build hardware and software that yields **complete local control to the hardware owner**. Every design, code path, and ecosystem decision must align with the following core tenets:
* **True Sovereignty:** The device firmware must remain open source and entirely independent of mandatory cloud infrastructure or external vendor services.
* **System Integrity:** The entire system—encompassing both the physical hardware architecture and the underlying software/firmware stacks—must remain completely transparent, audit-ready, and entirely free of telemetry, hidden tracking, or subversive code.
* **Intuitive Self-Sufficiency:** The local user interface must be expressive and clear. End-users must never be forced to parse raw system logs, examine codebase files, or guess at system behaviors. If an exceptional or unusual system state occurs, the user interface must immediately make both the issue and its remediation clear to a non-technical operator.

---

## 2. Governance Structure: Founder-Leader Model
This project operates under a **Founder-Leader** governance structure (historically known as a *Benevolent Dictator for Life* or *BDFL* model). 

* **Authority:** Lines of power and project administration are straightforward. The Founder-Leader serves as the custodian of the project's foundational intent, final architecture arbiter, and ultimate decision-maker for all structural, cryptographic, or technical disputes.
* **Scaling Commitments:** To prevent decision bottlenecks or insular development "castes" as the contributor ecosystem expands, the Founder-Leader actively delegates discrete repository permissions to trusted community members and prioritizes transparent communication regarding the long-term vision.

---

## 3. Core Community Roles
The community is organized into three progressive, merit-based tiers:

* **Contributor:** Anyone who participates constructively in the project ecosystem. This includes reporting hardware/software bugs, submitting issue documentation, offering feature feedback, opening Pull Requests (PRs), or helping others in community discussions.
* **Committer:** A trusted community member who has demonstrated a prolonged commitment to the project's engineering standards and philosophical intent. Committers are granted direct write/merge permissions to specific components of the source repository.
* **Maintainer:** Core leaders who work alongside the Founder-Leader to steer the overarching technical vision, curate the development roadmap, review highly critical subsystems, and establish final implementation patterns.

---

## 4. Pull Request & Code Acceptability Standards
To protect the security, long-term survival, and user-experience goals of the project, all incoming code contributions must meet strict validation criteria. Pull Requests will be evaluated against the following rules:

### A. Strict Philosophical Alignment
Any code changes that shift the system away from local-first execution, introduce hidden telemetry, or require cloud dependencies will be systematically rejected. All additions must serve to make the end-user experience highly intuitive to navigate.

### B. AI-Assisted Code Authorship
The use of AI-generation tools (e.g., Copilots, LLMs) for drafting code is fully permitted. However, the human author opening the PR assumes **100% accountability** for the contribution. The author must possess the technical capacity to fully explain, defend, and document every line, flag, and architectural detail of their submitted code during peer review.

### C. Maintainability and Lifecycle Quality
Code must be engineered for long-term survival. If the complexity, architecture, or dependency footprint of a Pull Request threatens future maintainability, it will be refused or returned to the author with an explicit request for refinement and simplification.

### D. Code Cleanliness and Organization
Unorganized, undocumented, sloppy, or structurally messy code will be rejected immediately. We value high readability, idiomatic patterns, defensive programming, and well-structured software architectures.
