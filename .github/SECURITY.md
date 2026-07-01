# Security Policy

We take the security of EMQX Neuron and NeuronEX seriously and appreciate the
efforts of security researchers who help keep our users safe.

## Reporting a Vulnerability

**Please do not report security vulnerabilities through public GitHub issues,
discussions, or pull requests.** Public disclosure before a fix is available puts
users at risk.

Instead, report privately through GitHub's **Private Vulnerability Reporting**:

1. Go to the **[Security](https://github.com/emqx/neuron/security)** tab of this
   repository.
2. Click **Report a vulnerability**.
3. Fill in the advisory form with the details below.

This opens a private channel visible only to the maintainers.

### What to include

A good report helps us triage quickly. Where possible, please provide:

- The affected product and version(s) (e.g. Neuron 2.15.0, NeuronEX 3.9.0), and
  the commit or image digest you tested.
- The component / code path (file and function, if known).
- A clear description of the vulnerability and its impact.
- Step-by-step reproduction instructions, a proof-of-concept, or the exact
  payloads. A reproduction package is welcome.
- The exact configuration required to trigger it (network binding, TLS mode,
  broker setup, plugins/nodes configured, etc.), and any conditions under which
  the issue does **not** occur.
- Your assessment of severity, if you have one (e.g. a CVSS vector).

## Supported Versions

Security fixes are provided for the active release tracks. Older versions should
be upgraded to a fixed release.

| Product        | Supported tracks |
|----------------|------------------|
| EMQX Neuron    | 2.12.x, 2.13.x, 2.14.x, 2.15.x |
| EMQX NeuronEX  | 3.6.x, 3.7.x, 3.8.x, 3.9.x |

## Our Process

1. **Acknowledgement** — we aim to acknowledge your report within a few business
   days.
2. **Assessment** — we validate the issue and assess severity (CVSS for the
   published advisory, plus our internal reward tiering where applicable).
3. **Fix** — we develop and backport the fix across the affected supported tracks.
4. **Advisory & CVE** — we publish a GitHub Security Advisory (GHSA) and request a
   CVE identifier.
5. **Disclosure** — to give users time to upgrade, we generally publish the
   advisory **90 days** after the fixed releases are available. We are happy to
   coordinate the disclosure timeline with you.
6. **Credit** — we credit reporters in the advisory by their preferred name or
   GitHub handle, unless you prefer to remain anonymous.

## Scope

Vulnerabilities in the Neuron / NeuronEX code and its official plugins are in
scope. When assessing impact we consider the intended deployment model (for
example, control-plane and broker interfaces are typically deployed on private,
trusted networks); please describe the exposure your report assumes.

Thank you for helping make the Neuron ecosystem safer.
