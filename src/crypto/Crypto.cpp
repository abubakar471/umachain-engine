// debug version - paste into Crypto.cpp (temporary)
#include "Crypto.h"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <vector>

// Remove whitespace (including newline) from a string
static std::string remove_whitespace(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
        if (!std::isspace((unsigned char)c))
            out.push_back(c);
    return out;
}

// wrap base64 into lines of length 64 with newline at end
static std::string wrap_base64(const std::string &b64)
{
    std::string out;
    for (size_t i = 0; i < b64.size(); i += 64)
    {
        out += b64.substr(i, std::min<size_t>(64, b64.size() - i));
        out += "\n";
    }
    return out;
}

// reuse normalize_pubkey_pem and base64_decode from your working Crypto.cpp
static std::string normalize_pubkey_pem(const std::string &rawPem)
{
    const std::string header = "-----BEGIN PUBLIC KEY-----";
    const std::string footer = "-----END PUBLIC KEY-----";

    // If input contains newlines, assume already PEM-style; ensure ending newline
    if (rawPem.find('\n') != std::string::npos)
    {
        if (!rawPem.empty() && rawPem.back() != '\n')
            return rawPem + "\n";
        return rawPem;
    }

    // No newlines: find header/footer or treat as bare base64
    size_t hpos = rawPem.find(header);
    size_t fpos = rawPem.find(footer);

    if (hpos == std::string::npos || fpos == std::string::npos)
    {
        // treat rawPem as base64 content
        std::string b64 = remove_whitespace(rawPem);
        return header + "\n" + wrap_base64(b64) + footer + "\n";
    }

    // Extract what's between header and footer
    size_t start = hpos + header.size();
    size_t len = (fpos > start) ? (fpos - start) : 0;
    std::string between = rawPem.substr(start, len);
    std::string b64 = remove_whitespace(between);
    return header + "\n" + wrap_base64(b64) + footer + "\n";
}

// Base64 decode using OpenSSL BIO; returns vector of bytes
static std::vector<unsigned char> base64_decode_vec(const std::string &in)
{
    BIO *bio = nullptr;
    BIO *b64 = nullptr;
    std::vector<unsigned char> out;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf(in.data(), (int)in.size());
    if (!bio || !b64)
    {
        if (bio)
            BIO_free(bio);
        if (b64)
            BIO_free(b64);
        return {};
    }
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    // allocate output buffer (rough upper bound)
    out.resize(in.size());
    int decoded_len = BIO_read(bio, out.data(), (int)out.size());
    BIO_free_all(bio);
    if (decoded_len <= 0)
        return {};
    out.resize(decoded_len);
    return out;
}

static void print_openssl_errors()
{
    unsigned long e;
    while ((e = ERR_get_error()))
    {
        char buf[256];
        ERR_error_string_n(e, buf, sizeof(buf));
        std::cerr << "OpenSSL error: " << buf << std::endl;
    }
}

static std::string bytes_to_hex_prefix(const unsigned char *data, size_t len, size_t prefix = 8)
{
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    size_t n = std::min(prefix, len);
    for (size_t i = 0; i < n; i++)
        ss << std::setw(2) << (int)data[i];
    return ss.str();
}

static std::string sha256_hex_str(const std::string &msg)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)msg.data(), msg.size(), hash);
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        ss << std::setw(2) << (int)hash[i];
    return ss.str();
}

bool Crypto::verifySignaturePEM(const std::string &pubKeyPem, const std::string &message, const std::string &signatureBase64)
{
    std::cerr << "---- verifySignaturePEM debug start ----\n";
    std::string normalized = normalize_pubkey_pem(pubKeyPem);
    std::cerr << "Normalized PEM (first 200 chars):\n"
              << normalized.substr(0, std::min<size_t>(normalized.size(), 200)) << "\n";
    std::vector<unsigned char> sig = base64_decode_vec(signatureBase64);

    std::vector<unsigned char> derSig; // will hold final DER signature bytes

    if (sig.size() == 64)
    {
        // Interpret as raw r||s (r: first 32, s: last 32)
        const unsigned char *rbytes = sig.data();
        const unsigned char *sbytes = sig.data() + 32;

        // Convert to BIGNUMs
        BIGNUM *r = BN_bin2bn(rbytes, 32, nullptr);
        BIGNUM *s = BN_bin2bn(sbytes, 32, nullptr);
        if (!r || !s)
        {
            if (r)
                BN_free(r);
            if (s)
                BN_free(s);
            std::cerr << "Crypto: BN_bin2bn failed for raw signature\n";
            // cleanup and return false (or continue to try DER path if you want)
            return false;
        }

        // Create an ECDSA_SIG and set r and s
        ECDSA_SIG *ecs = ECDSA_SIG_new();
        if (!ecs)
        {
            BN_free(r);
            BN_free(s);
            std::cerr << "Crypto: ECDSA_SIG_new failed\n";
            return false;
        }

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
        // In modern OpenSSL use ECDSA_SIG_set0 which takes ownership of BNs
        if (ECDSA_SIG_set0(ecs, r, s) != 1)
        {
            ECDSA_SIG_free(ecs);
            BN_free(r);
            BN_free(s); // if not consumed
            std::cerr << "Crypto: ECDSA_SIG_set0 failed\n";
            return false;
        }
#else
        // Older OpenSSL: set r and s manually (be careful with ownership)
        ecs->r = r;
        ecs->s = s;
#endif

        // Convert ECDSA_SIG to DER bytes
        int derLen = i2d_ECDSA_SIG(ecs, nullptr);
        if (derLen <= 0)
        {
            ECDSA_SIG_free(ecs);
            std::cerr << "Crypto: i2d_ECDSA_SIG failed (len)\n";
            return false;
        }

        derSig.resize(derLen);
        unsigned char *p = derSig.data();
        int derLen2 = i2d_ECDSA_SIG(ecs, &p);
        ECDSA_SIG_free(ecs);

        if (derLen2 != derLen)
        {
            std::cerr << "Crypto: DER length mismatch\n";
            return false;
        }

        // Replace 'sig' with derSig for verification
        sig = derSig;
        std::cerr << "Crypto: converted raw 64-byte signature -> DER len=" << sig.size() << "\n";
    }

    std::cerr << "signatureBase64 length: " << signatureBase64.size() << " -> decoded bytes: " << sig.size() << "\n";
    if (!sig.empty())
        std::cerr << "sig hex prefix: " << bytes_to_hex_prefix(sig.data(), sig.size()) << "\n";
    std::cerr << "Message length: " << message.size() << " message: [" << message << "]\n";
    std::cerr << "Message SHA256: " << sha256_hex_str(message) << "\n";

    BIO *bio = BIO_new_mem_buf(normalized.data(), (int)normalized.size());
    if (!bio)
    {
        print_openssl_errors();
        std::cerr << "BIO_new_mem_buf fail\n";
        return false;
    }
    EVP_PKEY *pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    if (!pkey)
    {
        std::cerr << "PEM_read_bio_PUBKEY failed\n";
        print_openssl_errors();
        BIO_free(bio);
        return false;
    }
    std::cerr << "PKEY loaded ok\n";

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    int v = -1;
    if (!mdctx)
    {
        std::cerr << "EVP_MD_CTX_new fail\n";
        EVP_PKEY_free(pkey);
        BIO_free(bio);
        return false;
    }

    if (EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, pkey) != 1)
    {
        std::cerr << "EVP_DigestVerifyInit failed\n";
        print_openssl_errors();
        goto cleanup;
    }
    if (EVP_DigestVerifyUpdate(mdctx, message.data(), message.size()) != 1)
    {
        std::cerr << "EVP_DigestVerifyUpdate failed\n";
        print_openssl_errors();
        goto cleanup;
    }

    v = EVP_DigestVerifyFinal(mdctx, sig.data(), sig.size());
    if (v == 1)
    {
        std::cerr << "Signature VERIFIED OK\n";
    }
    else if (v == 0)
    {
        std::cerr << "Signature verification FAILED (v==0)\n";
    }
    else
    {
        std::cerr << "EVP_DigestVerifyFinal returned error\n";
        print_openssl_errors();
    }

cleanup:
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    BIO_free(bio);
    std::cerr << "---- verifySignaturePEM debug end ----\n";
    return v == 1;
}
