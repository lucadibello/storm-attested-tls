# Attested TLS Server Implementation

This directory contains an implementation of an attested TLS server using the Open Enclave SDK. The server runs inside a secure enclave and provides mutual TLS authentication with remote attestation capabilities.

## Overview

The attested TLS server implementation is based on the [Open Enclave SDK attested TLS sample](https://github.com/openenclave/openenclave/tree/master/samples/attested_tls/server) and consists of two main components:

### 1. Enclave Component (`enclaves/atls_server/`)

The enclave handles the actual TLS server logic and runs in a trusted execution environment (TEE):

- **`atls_server.edl`**: Enclave Definition Language file defining the interface between trusted (enclave) and untrusted (host) code
  - Defines ECALL: `ecall_set_up_tls_server()` - Main entry point to start the TLS server
  - Defines OCALLs: Socket operations (`socket`, `bind`, `listen`, `accept`, `send`, `recv`, etc.)

- **`atls_server_impl.cpp`**: Core server implementation
  - Certificate generation with SGX attestation evidence embedded
  - TLS configuration using mbedTLS
  - Client connection handling with mutual authentication
  - Certificate verification using attestation evidence
  
- **`atls_server.conf`**: Enclave configuration (heap size, stack size, etc.)

### 2. Host Component (`host/`)

The host application manages the enclave and implements untrusted operations:

- **`host.cpp`**: CLI application for testing the server
  - Creates and initializes the enclave
  - Invokes the server ECALL with configured port
  - Handles simulation mode for testing without SGX hardware

- **`jni_bridge.cpp`**: JNI interface for Apache Storm integration
  - Provides Java-accessible methods to create/destroy enclaves
  - Allows Apache Storm to start the attested TLS server from Java code

- **`ocalls_host.cpp`**: Implementation of OCALL functions
  - Socket operations: `ocall_socket()`, `ocall_bind()`, `ocall_listen()`, etc.
  - These operations must be performed outside the enclave

## Building

```bash
cd /workspaces/project/storm-client/src/native
mkdir -p build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_HOST_EXE=ON

# Build
make -j$(nproc)
```

Build artifacts will be placed in:
- **Signed enclave**: `build/artifacts/enclaves/atls_server/atls_server.signed`
- **Host CLI**: `build/artifacts/host/oe_cli`
- **JNI library**: `build/artifacts/host/liboe_jni.so`

## Running the Test Server

```bash
cd build

# Run in simulation mode (no SGX hardware required)
./artifacts/host/oe_cli --simulate artifacts/enclaves/atls_server/atls_server.signed -port:8443

# Run on actual SGX hardware
./artifacts/host/oe_cli artifacts/enclaves/atls_server/atls_server.signed -port:8443
```

Or use the convenience target:
```bash
make run
```

## How It Works

### 1. Certificate Generation with Attestation

When the server starts, it generates a self-signed X.509 certificate with embedded SGX attestation evidence:

```cpp
oe_generate_attestation_certificate(
    subject_name,
    private_key, private_key_size,
    public_key, public_key_size,
    &output_cert, &output_cert_size);
```

This certificate contains:
- Standard X.509 certificate fields
- SGX quote (attestation evidence) in a custom extension
- Measurement of the enclave code (MRENCLAVE)
- Enclave signer identity (MRSIGNER)

### 2. Client Connection and Handshake

When a client connects:
1. TCP connection is established via OCALL
2. TLS handshake begins using mbedTLS
3. Server presents its attested certificate
4. Client must also provide an attested certificate (mutual TLS)
5. Both sides verify the attestation evidence

### 3. Certificate Verification

The `verify_certificate()` callback checks:
- Standard X.509 validation
- Extracts and verifies the SGX quote
- Validates the enclave measurements
- Ensures the remote party is running in a genuine SGX enclave

```cpp
oe_verify_attestation_certificate(
    cert_buf, cert_size,
    nullptr, 0);
```

### 4. Secure Communication

After successful attestation:
- All communication is encrypted via TLS 1.2/1.3
- Both parties are confident they're talking to a genuine enclave
- Data exchanged cannot be intercepted or modified

## Integration with Apache Storm

The JNI bridge allows Apache Storm (Java) to:

```java
// Create enclave
long handle = TlsServerEnclave.create("/path/to/atls_server.signed", false);

// Start server
int result = TlsServerEnclave.setup_tls_server("8443", true);

// Destroy when done
TlsServerEnclave.destroy(handle);
```

This enables Storm to use the attested TLS channel as a secure transport layer, with Netty connecting to the enclave server and performing attestation.

## Security Considerations

1. **Enclave Signing Key**: The private key in `build/signing/private.pem` is auto-generated. For production, use a managed signing key.

2. **Attestation Policy**: Currently accepts any valid SGX enclave. In production, implement policy checks:
   - Verify specific MRENCLAVE values
   - Check minimum security version numbers (SVN)
   - Validate enclave signer identity

3. **Certificate Validation**: The current implementation requires client certificates. Configure this based on your threat model.

4. **Network Security**: The enclave performs TLS but socket operations are handled by untrusted host code. The host cannot read/modify encrypted data, but can perform denial-of-service attacks.

## Customization

To customize the server behavior:

- **Port binding**: Modify the port parameter when calling `ecall_set_up_tls_server()`
- **TLS configuration**: Edit `setup_tls_config()` in `atls_server_impl.cpp`
- **Certificate subject**: Change the subject name in `generate_certificate_and_pkey()`
- **Attestation policy**: Implement custom verification in `verify_certificate()`
- **Application protocol**: Modify `handle_client_connection()` to implement your protocol

## Troubleshooting

**Build errors**: Ensure Open Enclave SDK is installed and CMake can find it:
```bash
export CMAKE_PREFIX_PATH=/opt/openenclave/lib/openenclave/cmake
```

**Runtime errors**: Check if SGX is available:
```bash
oe_is_enclave_supported
```

**Attestation failures**: Verify the AESM service is running:
```bash
systemctl status aesmd
```

## References

- [Open Enclave SDK Documentation](https://openenclave.io/sdk/)
- [Attested TLS Sample](https://github.com/openenclave/openenclave/tree/master/samples/attested_tls)
- [SGX Remote Attestation](https://www.intel.com/content/www/us/en/developer/articles/technical/quote-verification-attestation-with-intel-sgx-dcap.html)

