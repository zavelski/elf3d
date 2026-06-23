# Security Policy

## Supported Versions

Elf3D 0.1.0 is the first public baseline. Security fixes are considered for the
latest public release and the active development branch.

## Reporting A Vulnerability

Use a private GitHub security advisory when available. If that is not available,
open a GitHub issue with a minimal public description and avoid publishing
exploit details until a maintainer responds.

Do not include secrets, credentials, private models, customer data, or
proprietary assets in reports. If a model is required to demonstrate a security
issue, describe its relevant characteristics and offer a synthetic reproduction
instead.

## Scope

Security-sensitive areas include:

- glTF/GLB parsing and resource limits;
- PNG/JPEG image decoding boundaries;
- file path handling;
- malformed model handling;
- GPU resource lifetime and shutdown;
- public DLL boundary error handling.

Elf3D does not currently provide a stable C ABI, network service, scripting
runtime, or plugin ABI.
