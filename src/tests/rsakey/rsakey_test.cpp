#include <array>
#include <cstring>
#include <iostream>
#include <span>
#include <string>

#include "cvkrsa.hpp"

int main() {
    auto priv = aig::RsaKey::from_pem_file("private.pem", aig::RsaKeyType::Private);
    if (!priv) {
        std::cerr << priv.error() << '\n';
        return 1;
    }

    auto pub = aig::RsaKey::from_pem_file("public.pem", aig::RsaKeyType::Public);
    if (!pub) {
        std::cerr << pub.error() << '\n';
        return 1;
    }

    const char* msg = "hello rsa";
    std::array<uint8_t, 256> encrypted{};
    std::array<uint8_t, 256> decrypted{};

    auto enc = pub->encrypt(
        std::span<const uint8_t>{(const uint8_t*)msg, std::strlen(msg)},
        encrypted
    );
    if (!enc) {
        std::cerr << enc.error() << '\n';
        return 1;
    }
    assert(enc.value() == 256);

    auto dec = priv->decrypt(
        std::span<const uint8_t>{encrypted.data(), *enc},
        decrypted
    );
    if (!dec) {
        std::cerr << dec.error() << '\n';
        return 1;
    }
    std::cout << *dec <<std::endl;
    assert(dec.value() == strlen(msg));

    std::string out(reinterpret_cast<const char*>(decrypted.data()), *dec);
    std::cout << out << '\n';
}
