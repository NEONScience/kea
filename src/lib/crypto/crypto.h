// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <string>
#include <dns/buffer.h>
#include <exceptions/exceptions.h>

#include <boost/noncopyable.hpp>

#ifndef _ISC_CRYPTO_H
#define _ISC_CRYPTO_H

namespace isc {
namespace crypto {

/// General exception class that is the base for all crypto-related
/// exceptions
class CryptoError : public Exception {
public:
    CryptoError(const char* file, size_t line, const char* what) :
        isc::Exception(file, line, what) {}
};

/// This exception is thrown when a cryptographic action is requested
/// for an algorithm that is not supported by the underlying algorithm.
class UnsupportedAlgorithm : public CryptoError {
public:
    UnsupportedAlgorithm(const char* file, size_t line, const char* what) :
        CryptoError(file, line, what) {}
};

/// This exception is thrown when the underlying library could not
/// handle this key
class BadKey : public CryptoError {
public:
    BadKey(const char* file, size_t line, const char* what) :
        CryptoError(file, line, what) {}
};

/// Forward declaration, pimpl style
class HMACImpl;

/// \brief HMAC support
///
/// This class is used to create and verify HMAC signatures
///
class HMAC : public boost::noncopyable {
public:
    enum HashAlgorithm {
        MD5 = 0,
        SHA1 = 1,
        SHA256 = 2,
        UNKNOWN = 3
    };

    /// \brief Constructor from a secret and a hash algorithm
    ///
    /// \exception UnsupportedAlgorithmException if the given algorithm
    ///            is unknown or not supported by the underlying library
    /// \exception InvalidKeyLength if the given key secret_len is bad
    ///
    /// Notes: if the secret is longer than the block size of its
    /// algorithm, the constructor will run it through the hash
    /// algorithm, and use the digest as the secret for this HMAC
    /// operation
    ///
    /// \param secret The secret to sign with
    /// \param len The length of the secret
    /// \param hash_algorithm The hash algorithm
    explicit HMAC(const void* secret, size_t secret_len,
                  const HashAlgorithm hash_algorithm);

    /// \brief Destructor
    ~HMAC();

    /// \brief Add data to digest
    ///
    /// \param data The data to add
    /// \param len The size of the data
    void update(const void* data, const size_t len);

    /// \brief Calculate the final signature
    ///
    /// The result will be appended to the given outputbuffer
    ///
    /// \param result The OutputBuffer to append the result to
    void sign(isc::dns::OutputBuffer& result);

    /// \brief Verify an existing signature
    ///
    /// \param sig The signature to verify
    /// \param len The length of the sig
    /// \return true if the signature is correct, false otherwise
    bool verify(const void* sig, size_t len);

private:
    HMACImpl* impl_;
};

/// \brief Create an HMAC signature for the given data
///
/// This is a convenience function that calculates the hmac signature,
/// given a fixed amount of data. Internally it does the same as
/// creating an HMAC object, feeding it the data, and calculating the
/// resulting signature.
///
/// \exception UnsupportedAlgorithm if the given algorithm is unknown
///            or not supported by the underlying library
/// \exception BadKey if the given key secret_len is bad
///
/// Notes: if the secret is longer than the block size of its
/// algorithm, the constructor will run it through the hash
/// algorithm, and use the digest as the secret for this HMAC
/// operation
///
/// \param data The data to sign
/// \param data_len The length of the data
/// \param secret The secret to sign with
/// \param secret_len The length of the secret
/// \param hash_algorithm The hash algorithm
/// \param result The signature will be appended to this buffer
void signHMAC(const void* data,
              const size_t data_len,
              const void* secret,
              size_t secret_len,
              const HMAC::HashAlgorithm hash_algorithm,
              isc::dns::OutputBuffer& result);

/// \brief Verify an HMAC signature for the given data
///
/// This is a convenience function that verifies an hmac signature,
/// given a fixed amount of data. Internally it does the same as
/// creating an HMAC object, feeding it the data, and checking the
/// resulting signature.
///
/// \exception UnsupportedAlgorithm if the given algorithm is unknown
///            or not supported by the underlying library
/// \exception BadKey if the given key secret_len is bad
///
/// Notes: if the secret is longer than the block size of its
/// algorithm, the constructor will run it through the hash
/// algorithm, and use the digest as the secret for this HMAC
/// operation
///
/// \param data The data to verify
/// \param data_len The length of the data
/// \param key The TSIGKey to verify with
/// \param mac The signature to verify
/// \return True if the signature verifies, false if not
bool verifyHMAC(const void* data,
                const size_t data_len,
                const void* secret,
                size_t secret_len,
                const HMAC::HashAlgorithm hash_algorithm,
                const void* sig,
                const size_t sig_len);

} // namespace crypto
} // namespace isc

#endif // _ISC_CRYPTO_H
