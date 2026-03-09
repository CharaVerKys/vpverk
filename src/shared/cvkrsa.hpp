#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <expected>
#include <cussert.hpp>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

namespace aig { //ai generated code

    // well i have to do something myself, since it apparently not work even with just clangd checks

enum class RsaKeyType : uint8_t { Private, Public };

class RsaKey {
    //struct Deleter { void operator()(EVP_PKEY* p) { if(p) EVP_PKEY_free(p); } };
    //std::unique_ptr<EVP_PKEY, Deleter> key;
    RsaKeyType type;
    EVP_PKEY* key = nullptr;

    RsaKey(RsaKeyType type, EVP_PKEY* key) : type{type},key{key} {cussert(key);}

public:
    RsaKey(RsaKey&&o):type(o.type),key{o.key}{o.key = nullptr;}
    ~RsaKey() {if(key) EVP_PKEY_free(key);}
    RsaKey& operator=(RsaKey&&o) = delete;
    RsaKey(RsaKey const&) = delete;
    RsaKey& operator=(RsaKey const&) = delete;

    [[nodiscard]] RsaKeyType loaded_type() const { return type; }

    static tl::expected<RsaKey, std::string> from_pem_file(const char* path, RsaKeyType key_type) {
        FILE* f = fopen(path, "r");
        if (!f)
            return tl::unexpected(std::string("cannot open ") + path);

        EVP_PKEY* pkey = nullptr;
        switch (key_type) {
            case RsaKeyType::Private:
                pkey = PEM_read_PrivateKey(f, nullptr, nullptr, nullptr); //docks checked // https://docs.openssl.org/master/man3/PEM_read_bio_PrivateKey/#synopsis
                break;
            case RsaKeyType::Public:
                pkey = PEM_read_PUBKEY(f, nullptr, nullptr, nullptr); //docks checked
                break;
        }
        fclose(f);

        if (!pkey)
            return tl::unexpected(
                std::string("failed to parse ") +
                (key_type == RsaKeyType::Private ? "private" : "public") +
                " key PEM"
            );

        //RsaKey r;
        //r.key.reset(pkey); //stupid ai allocate incomplete type //idk why clangd doesnt throw errors here but that kind of not right
        //r.type = key_type;
        return {{key_type,pkey}};
    }

    // RSA-OAEP SHA-256 decrypt — requires Private
    tl::expected<size_t, std::string> decrypt(
        std::span<const uint8_t> in,
        std::span<uint8_t> out
    ) const {
        cussert(in.size() == 256);
        //idk, ai try to use my stile, but ignoring {} while keeping raii lifetimes and expected returns is kind of... this std::string as error......
        if (type != RsaKeyType::Private)
            return tl::unexpected(std::string("decrypt requires private key"));

        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, nullptr); //checked
        if (!ctx)
            return tl::unexpected(std::string("CTX alloc failed"));

        struct Guard { EVP_PKEY_CTX* c; ~Guard(){ EVP_PKEY_CTX_free(c); } } guard{ctx};

        if (EVP_PKEY_decrypt_init(ctx) <= 0) // docks checked, literally example // https://docs.openssl.org/master/man3/EVP_PKEY_decrypt/#examples
            return tl::unexpected(std::string("decrypt_init failed"));

        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) // ai used RSA_OAEP_PADDING  (not exist) -> like in example use <> enum-like-macros
          return tl::unexpected(std::string("set_padding failed"));

        // not called in example, so remove now
        // added back with sha3 cuz why not, havent verified what id doing, but ai so sure that it is correct // it say sha-1 used instead (which is ok for rsa)
        // tho found names in docs, but i not understand what exactly it doing from docs
        // reasonable that it has to be 256 (same as rsa msg size), it is hash...
        if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha3_256()) <= 0)
            return tl::unexpected(std::string("set_oaep_md failed"));

        //example offer use of -> out = OPENSSL_malloc(outlen);
        //but i use pre-allocated out buffer


        // use example flow instead, idk maybe no-op but just in case outlen have to be correct
        // size_t outlen = out.size();
        // if (EVP_PKEY_decrypt(ctx, out.data(), &outlen, in.data(), in.size()) <= 0)
        //     return tl::unexpected(std::string("decrypt failed"));

        size_t outlen;
        /* Determine buffer length */
        if (EVP_PKEY_decrypt(ctx, NULL, &outlen, in.data(), in.size()) <= 0)
          return tl::unexpected(std::string("get outlen (decrypt) fail"));

        if(outlen > out.size()){
          return tl::unexpected("buffer size (decrypt len > buffer.size)");
        }

        if (EVP_PKEY_decrypt(ctx, out.data(), &outlen, in.data(), in.size()) <= 0)
          return tl::unexpected(std::string("get decrypt fail"));

        return outlen;
    }

    //will not repeat for same changes and comments from decrypt

    // RSA-OAEP SHA-256 encrypt — requires Public
    tl::expected<size_t, std::string> encrypt(
        std::span<const uint8_t> in,
        std::span<uint8_t> out
    ) const {
        if (type != RsaKeyType::Public)
            return tl::unexpected(std::string("encrypt requires public key"));

        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, nullptr);
        if (!ctx)
            return tl::unexpected(std::string("CTX alloc failed"));
        struct Guard { EVP_PKEY_CTX* c; ~Guard(){ EVP_PKEY_CTX_free(c); } } guard{ctx};

        if (EVP_PKEY_encrypt_init(ctx) <= 0)
            return tl::unexpected(std::string("encrypt_init failed"));

        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
            return tl::unexpected(std::string("set_padding failed"));

        if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha3_256()) <= 0)
            return tl::unexpected(std::string("set_oaep_md failed"));

        // size_t outlen = out.size();
        // if (EVP_PKEY_encrypt(ctx, out.data(), &outlen, in.data(), in.size()) <= 0)
        //     return tl::unexpected(std::string("encrypt failed"));

        size_t outlen;
        /* Determine buffer length */
        if (EVP_PKEY_encrypt(ctx, NULL, &outlen, in.data(), in.size()) <= 0)
          return tl::unexpected(std::string("get outlen (encrypt) fail"));

        if(outlen > out.size()){
          return tl::unexpected("buffer size (encrypt len > buffer.size)");
        }

        if (EVP_PKEY_encrypt(ctx, out.data(), &outlen, in.data(), in.size()) <= 0)
          return tl::unexpected(std::string("get encrypt fail"));

        return outlen;
    }
};

} // namespace aig

