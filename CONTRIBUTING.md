# Contributing to this project

Contributions to **Anos** are very welcome!

Fork the repo, make your changes on a branch, and open a pull request (PR) against `main`.

This document sets out what we expect so reviews go smoothly.

## Licensing, copyright & ownership

By submitting a PR or otherwise contributing code to this project, you confirm that:

- Your contribution, if accepted, will be licensed under the project’s primary license (**GPL-2.0**).
- You have the right to submit the contribution and it does not infringe third-party rights.
- You agree to include a Developer Certificate of Origin (DCO) sign-off on each commit:

```
Signed-off-by: Your Name you@example.com
```

If you cannot sign off, please explain why in the PR; we can’t merge without a clear chain of custody.

## Languages & scope

- The **kernel** is written in **C** (with architecture/platform assembly where necessary).  
PRs that add or modify primary kernel code in other languages will be rejected.

- **Userspace** is more flexible: any language is fine provided the code:
  - builds and runs with the existing toolchain (or adds it in a contained way),
  - is correct and testable,
  - fits with the userspace libraries/runtimes,
  - can be reasonably reviewed by other **committers**.

## AI-assisted contributions

### Disclosure

If any part of a PR was authored with AI assistance (generation or substantial edits), **state this prominently** in the PR description.  

Undisclosed AI-assisted PRs may be closed without further comment.

> “AI-assisted” means the model produced or rewrote non-trivial code or tests.  
> It **does not** include formatting, refactoring tools, or boilerplate expansion from IDEs.

### Directories

- `kernel/**`  
**⛔ Not accepted.** AI-authored or AI-assisted changes in the kernel will be rejected.

- `system/**`, `servers/**`  
**🧐 Allowed with extra scrutiny.** Be prepared for deeper review and possible rejection.

- `**/tests/**`  
**👍 Allowed.** AI-assisted tests and test infrastructure are welcome, subject to normal review.

## How to get your PR merged

- Keep changes focused and small; large PRs are slower to review.
- Ensure `make clean test` passes; include new tests when you add behavior.
- Be sure to test your changes under all target architectures!
- Follow the project’s style (run formatters/linters where provided).
- Write a clear PR description: what changed, why, and any caveats.
- Link related issues, and include benchmarks or logs when performance/latency is relevant.

## Security

If you believe you’ve found a security issue, open a public issue as normal.
There is no distinction made between regular and security issues, and no bounty or other reward is available.

## Code of Conduct

Be respectful. We follow the [Contributor Covenant](./CODE_OF_CONDUCT.MD).
