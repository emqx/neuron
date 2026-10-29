# Neuron Threat Model

## Purpose and scope

This model guides security review of the Neuron core, built-in plugins, and their integration boundaries. It describes intended security requirements and deployment assumptions, not a certification that every path implements them. Check the actual changed code, configuration, packaging, and release when assessing a finding.

This model covers code maintained in this open-source repository. Describe external inputs and deployment dependencies in terms of the interfaces they expose; derive security requirements from Neuron's own behavior and supported identity model.

## Deployment profiles

| Profile | Entry and authentication boundary | Review implications |
| --- | --- | --- |
| Direct management access | Management HTTP API, normally port 7000; checked-in configuration uses `127.0.0.1` and enables authentication | Binding, authentication, keys, process identity, and TLS termination are configurable. State the actual profile, not just defaults. |
| Management through an operator-managed reverse proxy | Client requests reach Neuron through an intermediary | Verify exposed routes, forwarding, TLS termination, and effective gateway/proxy authentication and authorization. Do not assume a proxy preserves the intended boundary. |
| Device and broker integrations | Protocol sockets, broker subscriptions, and configured remote endpoints | These paths do not inherit REST authentication. Trace their own peer identity, topic permissions, message handling, and device-write controls. |

Loopback reduces direct remote exposure but does not exclude access through a proxy, SSRF, a local process, shared network namespaces, or modified packaging. Conversely, a source-level handler defect does not by itself establish external reachability in a particular deployment.

Production deployments commonly segment management and OT networks. Segmentation affects exploit prerequisites; it is not a parser safety guarantee or a reason to exclude a compromised device or authorized malicious publisher.

## Assets and actors

Protect management identities and JWT signing material; device/broker credentials; configuration and persisted state; plugin libraries and schemas; process memory; collected data; device-write capability; and acquisition/forwarding availability.

Relevant actors include unauthenticated management peers, ordinary authenticated users, administrators, holders of tokens from configured issuers, malicious or compromised devices/brokers/publishers, and local processes. Administrators and installed native code possess substantial intended authority, but that does not authorize lower-trust actors to acquire it.

Use synthetic secrets and simulators for validation. Never direct a security test at customer equipment without explicit authorization.

## Data flows and trust boundaries

| Flow | Boundary | Required checks |
| --- | --- | --- |
| Client → REST handler | Network caller → management authority | Explicit authentication and operation-specific identity/authorization checks; complete error handling |
| JWT → identity | Externally supplied claims → trusted principal | Signature, permitted algorithm, configured issuer/key association, intended audience, applicable time constraints, and explicit identity semantics |
| Broker → MQTT write request | Authorized publisher → gateway/device action | Broker identity and direction-specific topic ACLs, message validation, configured target and write permissions |
| Device reply → parser/cache | Remote bytes → process memory and tag values | Actual frame lengths, destination bounds, counts/types, resource limits, and object lifetimes |
| Configuration/import/database → runtime | Stored or supplied data → allocations, paths, plugins, and connections | Revalidation, bounded decoding, path containment, safe persistence, and secret protection |
| Plugin library → gateway process | Native extension → service privileges | Authorized loading and trusted artifact admission; no assumed plugin sandbox |
| Driver write → field device | Management/integration command → operational state | Caller authority, configured destination, writable tag attributes, and value validation |

## Security requirements

### Management identity and JWT

A valid signature is necessary but not sufficient for accepting a token. Bind verification to the expected audience and configured issuer/key relationship; reject missing or invalid claims when required by that token profile. Do not treat tokens minted for other services as Neuron credentials merely because a signing key is shared.

Claims must not implicitly create administrative authority without an explicit supported identity policy. Distinguish legacy local-token conventions from admission of externally issued tokens. User creation, deletion, credential changes, and other sensitive actions require their own authorization decisions; a valid JWT alone does not establish that every operation is permitted.

Trace credential removal and token lifetime separately. Deleting an account need not invalidate an already signed stateless token unless a revocation mechanism exists; do not claim immediate revocation without evidence.

### Parsing, memory, and concurrency

Validate source bytes remaining and destination capacity independently. Check arithmetic before allocation, copy, pointer movement, or narrowing conversion. Reject inconsistent frame lengths, invalid types, excessive counts, oversized strings, and incomplete fragments. Keep allocation sizes consistent with iteration bounds.

For asynchronous messages, establish whether each buffer is borrowed, owned, copied, or transferred. Retain data until the last callback finishes. Cancellation, reconnection, configuration changes, and shutdown must not leave callbacks accessing freed state. Include failure and partial-initialization cleanup paths.

Treat persisted/imported data as input. Schema or UI checks are not substitutes for runtime validation, including when old configuration is restored after an upgrade.

### Cryptography and secrets

Validate peer identity as well as chain trust when the protocol requires it. Keep TLS server verification separate from possession of a client credential. Public certificates are not private keys; do not confuse their sensitivity. Shipped production credentials and reusable trusted CA private keys must not be public image material. Protect key storage, rotation, permissions, logs, backups, and exports.

### Metrics and intended capabilities

Prometheus metrics intentionally supports unauthenticated scraping. Absence of JWT verification on the documented metrics endpoint is not alone an authorization defect. Still review added labels/data for secrets, unexpected sensitive values, unbounded cardinality, expensive requests, and newly introduced control actions. Operators must place the endpoint appropriately.

Native plugins execute within the gateway process with its service-account authority. Authorized loading of native code is an intended capability, not evidence of container escape. A lower-trust path reaching plugin loading or corrupting plugin memory is a separate security question. Process-user execution, container-root execution, and host compromise require different evidence.

## Availability and impact limits

Identify the actual supervisor and restart policy. Recovery is deployment-dependent, not universal. Automatic recovery can reduce a single crash's duration, while repeated inputs may sustain a crash loop. It does not undo credential exposure, state corruption, unsafe device writes, or arbitrary execution.

Do not infer code execution from every invalid memory access. Conversely, do not reduce established memory corruption to harmless failure just because the process restarts. Account for build architecture, hardening, actual privileges, and demonstrated primitives. Do not extrapolate historical non-PIE or sanitizer assumptions to current binaries.

## Evidence and maintenance

Useful anchors are `neuron.json`, `src/argparse.c`, `src/utils/neu_jwt.c`, `include/neuron/utils/http_handler.h`, `plugins/restful/`, `plugins/mqtt/`, `src/core/plugin_manager.c`, `src/persist/`, and `CMakeLists.txt`.

A review should identify the changed boundary, attacker-controlled input, necessary configuration, actual effects, and a safe regression test. Separate implementation evidence from packaging assumptions and unverified downstream consequences.

Update this model when changing binding/authentication defaults, token profiles, metrics contents, user policies, plugin admission, write routing, persistence formats, process privileges, or supervision. Follow the [security policy](../SECURITY.md) for private disclosure and the [review rules](../AGENTS.md) for engineering guidance.
