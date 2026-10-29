# Security Policy

## Private reporting

The canonical [Neuron security policy](.github/SECURITY.md) governs vulnerability reporting, supported release tracks, and coordinated disclosure. This page adds reporting context for the open-source Neuron code maintained in this repository; it does not establish a separate support or reward policy.

**Do not disclose vulnerabilities through public issues, discussions, or pull requests.** Open the [Neuron Security tab](https://github.com/emqx/neuron/security) and select **Report a vulnerability** to report privately.

## Neuron report context

Include the affected version/commit or image digest, component, safe reproduction, observed impact, and affected/unaffected conditions. In particular, specify:

- Neuron deployment/package and whether access is direct or through an operator-managed reverse proxy.
- Effective management binding, authentication/token configuration, and direct or proxied access path.
- Attacker identity/permissions and controlled input: REST request, device response, broker message, import, file, or other source.
- Relevant plugin, transport/TLS mode, broker topic/direction ACLs, and device-write prerequisites.
- Whether the demonstrated effect is disclosure, denied/unauthorized action, crash, persistent availability loss, memory corruption, or execution. Distinguish observed effects from downstream hypotheses.

Use synthetic credentials and isolated services/simulators. Do not test customer systems or real equipment without authorization, and transmit sensitive evidence only in the private report.

The [threat model](docs/threat-model.md) explains deployment boundaries and intended capabilities. Loopback/private networking and authenticated peers are assessment context, not blanket exclusions. Anonymous metrics and authorized native extensions have intended uses; report a concrete confidentiality, integrity, availability, or authorization violation rather than the capability alone.

Maintainers and automated reviewers should also follow [AGENTS.md](AGENTS.md). Do not place private vulnerability details in public review comments.
