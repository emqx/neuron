# Neuron Engineering and Review Guide

## Repository context

Neuron is the open-source C gateway/core and built-in plugin repository. `plugins/restful/` contains management handlers; `plugins/mqtt/` handles broker interactions; `src/utils/`, `src/adapter/`, `src/core/`, and `src/persist/` contain shared decoding, runtime, plugin, and storage logic. This guide covers code maintained in this repository.

Read [the threat model](docs/threat-model.md) and [security policy](SECURITY.md). Preserve more specific directory instructions and generated-file workflows. Match existing naming, ownership conventions, and error handling. Do not manually edit generated parsers when their generation workflow is authoritative.

## Code Review Rules

### Establish the actual security boundary

- Establish the effective management binding, authentication configuration, and access path. The checked-in loopback/auth defaults are configurable. When a reverse proxy is present, inspect forwarding and authorization without assuming it preserves the gateway's security boundary. Trace the reachable transport before claiming or excluding remote exposure.
- For each finding, identify attacker-controlled input, privileges, enabling configuration, changed lines, sink, and concrete effect. Review introduced regressions and impacted callers, not merely suspicious API names. Separate verified behavior from assumptions about releases or packaging.
- An authenticated caller is not automatically authorized for user management, credential changes, plugin operations, or device writes. Check each operation's actual identity policy and alternate route/method. Derive roles and authority from Neuron's own supported identity model.

### Fail closed at authentication and trust boundaries

- Require appropriate JWT signature, algorithm, issuer/key association, audience, time, and identity validation. External tokens and missing claims must not gain authority through an undocumented fallback. Handle all validation errors, not one named mismatch case.
- Account deletion, password change, and token revocation are separate mechanisms. Test the intended behavior without assuming signed tokens instantly disappear.
- Treat configured credentials and key material as secrets. Validate TLS peer identity and certificate trust; never admit a shared production private key merely because it ships with the application.

### Validate memory and lifetime at the consuming boundary

- Check actual source bytes and destination capacity separately. Validate counts/types, arithmetic, signedness, narrowing, termination, and allocation/iteration agreement before copying or dereferencing. Include singular/batch, malformed, empty, partial, and oversized inputs.
- Trace ownership across message queues, callbacks, reconnection, configuration replacement, and shutdown. Retain/copy borrowed data as needed; stop callbacks before releasing state. Include allocation and partial-failure cleanup.
- Configuration, imports, restored databases and protocol files remain parsing inputs. Constrain paths and symlinks, preserve valid state on failure, and avoid secrets in errors/logs/exports. UI/schema validation is not sufficient runtime validation.

### Review intended capabilities accurately

- Metrics intentionally permits unauthenticated Prometheus scraping. Report added sensitive disclosure, control behavior, or resource abuse—not simply the documented lack of JWT enforcement.
- Native plugins share service-process privileges. Authorized extension loading is not itself host escape. Lower-trust access to loading or memory corruption still warrants review. Prove actual privileges and distinguish service execution, container-root execution, and host compromise.
- Broker ACLs and device-write rights are independent from REST authentication. Follow publish/subscribe directions and configured write targets; do not infer arbitrary downstream takeover from a scoped action.
- Supervision is deployment-dependent. Recovery may shorten a crash but does not eliminate repeated DoS, persistent corruption, or RCE. Do not infer RCE from an unweaponized crash or assume historical binary hardening still applies.

### Findings and regression evidence

State prerequisites and unaffected cases. Cite the narrow code location and reachable call chain; avoid generic severity boilerplate. A safe fix should cover shared helpers and sibling paths, not just the reported route. Add targeted boundary/negative tests, including failure branches and ownership during teardown. Keep private reports, unpublished exploit details, customer data, credentials and reward discussions out of public review comments and repository documentation.

## Build and validation

Use the documented supported build environment and dependencies. From the repository root, a normal CMake build can use:

```sh
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Check `CMakeLists.txt` and CI for the target toolchain and options. Current Debug builds enable ASan unless disabled; do not assume Release does. Functional pytest suites under `tests/ft/` require gateway/broker/simulator dependencies and change runtime state. Run only targeted authorized lab tests; never point them at production OT equipment. Report commands actually run and any missing prerequisites rather than claiming validation from source inspection alone.
