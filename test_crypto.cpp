#include "Crypto/Crypto.h"
#include <iostream>

using namespace std;
using namespace umachain::crypto;

int main() {
    string pubPem, privPem;
    if (!generateKeypairPEM(pubPem, privPem)) {
        cerr << "Keygen failed\n"; return 1;
    }
    cout << "Public PEM:\n" << pubPem << "\n";
    cout << "Private PEM:\n" << privPem.substr(0, 200) << "...\n"; // don't print whole priv in logs

    string msg = "hello umachain " + to_string(time(NULL));
    string sig = signMessageBase64(privPem, msg);
    cout << "Signature (base64): " << sig << "\n";

    bool ok = verifySignaturePEM(pubPem, msg, sig);
    cout << "Verify: " << (ok ? "OK" : "FAIL") << "\n";

    cout << "SHA256(hex) of 'abc': " << sha256Hex("abc") << "\n";
    return ok ? 0 : 2;
}
