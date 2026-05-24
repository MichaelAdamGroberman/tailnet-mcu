# Security Policy

## Supported versions

| Version | Supported |
|---------|-----------|
| 0.1.x (current) | Yes |
| < 0.1.0 | No |

## Reporting a vulnerability

**Please do not file a public GitHub issue for security vulnerabilities.**

Report vulnerabilities privately via
[GitHub Security Advisories](https://github.com/MichaelAdamGroberman/tailnet-mcu/security/advisories/new).
GitHub routes the report directly to the maintainer without public disclosure.

You can also reach the maintainer via LinkedIn:
[Michael Groberman](https://www.linkedin.com/in/michael-adam-groberman/)

Please include:
- A description of the vulnerability and the affected component.
- Steps to reproduce or a minimal proof of concept.
- The board and firmware configuration you were testing against.
- Any relevant logs or Serial output.

## Disclosure timeline

We aim to acknowledge receipt within **72 hours** and to provide a remediation
plan or patch within **14 days** for confirmed issues. We follow a
coordinated disclosure model: please give us reasonable time to fix before
public disclosure.

## Security documentation

See [docs/security-model.md](docs/security-model.md) for the full threat
model and [gateway/harden.md](gateway/harden.md) for gateway hardening steps.
