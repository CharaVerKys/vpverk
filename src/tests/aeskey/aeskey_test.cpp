#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "cvkaes.hpp"

int main() {
    auto key = aig::AesSession::random_bytes<32>();
    auto iv1 = aig::AesSession::random_bytes<16>();
    auto iv2 = aig::AesSession::random_bytes<16>();

    // sender encrypts with iv1, decrypts with iv2
    auto sender = aig::AesSession::create(key, iv1, iv2);
    if (!sender) {
        std::cerr << "sender create: " << sender.error().message() << '\n';
        return 1;
    }

    // receiver: ivs swapped
    auto receiver = aig::AesSession::create(key, iv2, iv1);
    if (!receiver) {
        std::cerr << "receiver create: " << receiver.error().message() << '\n';
        return 1;
    }

    std::vector<size_t> sizes = {0, 1, 2, 3, 4, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 100, 255, 256, 500, 1024, 1500, 4096, 65535};

    srand(42);

    for (size_t sz : sizes) {
        std::vector<uint8_t> plain(sz);
        for (auto& b : plain) b = rand() & 0xFF;

        std::vector<uint8_t> encrypted(sz);
        std::vector<uint8_t> decrypted(sz);

        auto ec = sender->encrypt(plain, encrypted);
        if (ec) {
            std::cerr << "encrypt fail sz=" << sz << ": " << ec.message() << '\n';
            return 1;
        }

        // ciphertext must differ from plaintext (unless empty or astronomically unlucky)
        if (sz > 0 && plain == encrypted) {
            std::cerr << "encrypt produced identical output sz=" << sz << '\n';
            return 1;
        }

        ec = receiver->decrypt(encrypted, decrypted);
        if (ec) {
            std::cerr << "decrypt fail sz=" << sz << ": " << ec.message() << '\n';
            return 1;
        }

        if (plain != decrypted) {
            std::cerr << "mismatch sz=" << sz << '\n';
            for (size_t i = 0; i < sz; ++i) {
                if (plain[i] != decrypted[i]) {
                    std::cerr << "  first diff at " << i << ": " << (int)plain[i] << " vs " << (int)decrypted[i] << '\n';
                    break;
                }
            }
            return 1;
        }

        std::cout << "ok sz=" << sz << '\n';
    }

    // test that CTR stream is stateful: encrypting same plaintext twice gives different ciphertext
    {
        auto s2 = aig::AesSession::create(key, iv1, iv2);
        std::vector<uint8_t> plain(32, 0xAA);
        std::vector<uint8_t> enc1(32), enc2(32);

        auto ec = s2->encrypt(plain, enc1);
        auto ec2= s2->encrypt(plain, enc2);

        if (enc1 == enc2) {
            std::cerr << "CTR state broken: same plaintext gave same ciphertext\n";
            return 1;
        }
        std::cout << "ok ctr-stateful\n";
    }

    std::cout << "all passed\n";
}
