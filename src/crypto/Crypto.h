#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>

class Crypto {
public:
    // Verify signature: pubKeyPem = "-----BEGIN PUBLIC KEY-----..."; 
    // message is the original string signed; signatureBase64 is base64-encoded DER signature (WebCrypto output).
    static bool verifySignaturePEM(const std::string &pubKeyPem,
                                   const std::string &message,
                                   const std::string &signatureBase64);

    // optional helper: sha256 hex of a string
    static std::string sha256_hex(const std::string &data);
};

#endif // CRYPTO_H