#include "erpiko/enveloped-data.h"
#include "erpiko/utils.h"
#include "converters.h"
#include <openssl/pkcs7.h>
#include <openssl/err.h>
#include <iostream>
#include <sstream>

namespace Erpiko {
class EnvelopedData::Impl {
  public:
    STACK_OF(X509)* certs = nullptr;

    std::unique_ptr<ObjectId> oid;
    PKCS7 *pkcs7 = nullptr;
    BIO* bio = nullptr;
    std::string smimePartial;

    bool success = false;
    bool imported = false;
    bool isSMime = false;
    bool fromSMimePartial = false;
    bool finalized = false;

    Impl() {
      OpenSSL_add_all_algorithms();

      certs = sk_X509_new_null();
    }

    virtual ~Impl() {

      if (bio != nullptr) {
        BIO_free(bio);
      }
      if (pkcs7) {
        pkcs7->d.enveloped->recipientinfo = nullptr;
        PKCS7_free(pkcs7);
      }
      sk_X509_free(certs);
    }

    void appendCertificate(const X509* cert) {
      sk_X509_push(certs, cert);
    }

    void fromDer(const std::vector<unsigned char> der) {
      imported = true;
      BIO* mem = BIO_new_mem_buf((void*) der.data(), der.size());
      pkcs7 = d2i_PKCS7_bio(mem, NULL);
      auto ret = (pkcs7 != nullptr);

      if (ret) {
        success = true;
        return;
      }
    }

    void fromPem(const std::string pem) {
      imported = true;
      BIO* mem = BIO_new_mem_buf((void*) pem.c_str(), pem.length());
      pkcs7 = PEM_read_bio_PKCS7(mem, NULL, NULL, NULL);

      auto ret = (pkcs7 != nullptr);

      if (ret) {
        success = true;
        return;
      }
    }

    void fromSMime(const std::string smime) {
      isSMime = true;
      imported = true;
      BIO* mem = BIO_new_mem_buf((void*) smime.c_str(), smime.length());
      BIO* na = NULL;
      pkcs7 = SMIME_read_PKCS7(mem, &na);

      auto ret = (pkcs7 != nullptr);

      if (ret) {
        success = true;
        return;
      }
    }

    void fromSMimeInit(const std::string smime) {
      fromSMimePartial = true;
      finalized = false;
      if (smime.length() > 0) {
        smimePartial = smime;
      }
      return;
    }

    void fromSMimeUpdate(const std::string smime) {
      if (!fromSMimePartial) {
        return;
      }
      if (finalized) {
        smimePartial = "";
        return;
      }
      smimePartial += smime;
      return;
    }

    void fromSMimeFinalize() {
      if (!fromSMimePartial) {
        return;
      }
      if (finalized) {
        return;
      }
      if (pkcs7) {
        PKCS7_free(pkcs7);
        pkcs7 = nullptr;
      }
      isSMime = true;
      imported = true;
      BIO* mem = BIO_new_mem_buf((void*) smimePartial.c_str(), smimePartial.length());
      BIO* na = NULL;
      pkcs7 = SMIME_read_PKCS7(mem, &na);

      auto ret = (pkcs7 != nullptr);
      if (ret) {
        smimePartial = "";
        success = true;
        finalized = true;
        return;
      }
    }

    void fromSMimeFile(const std::string path) {
      isSMime = true;
      imported = true;
      const char *inmode = "r";
      auto f = BIO_new_file(path.c_str(), inmode);
      BIO* na = NULL;
      pkcs7 = SMIME_read_PKCS7(f, &na);

      auto ret = (pkcs7 != nullptr);

      if (ret) {
        success = true;
        return;
      }
    }

    const EVP_CIPHER* getCipher() {
      auto obj = OBJ_txt2obj(oid->toString().c_str(), 0);
      return EVP_get_cipherbyobj(obj);
    }

    void encrypt(const std::vector<unsigned char> data, EncryptingType::Value type, bool finalized = false) {
      if (pkcs7) {
        PKCS7_free(pkcs7);
        pkcs7 = nullptr;
      }

      bio = BIO_new(BIO_s_mem());
      BIO_write(bio, data.data(), data.size());

      auto cipher = getCipher();
      if (cipher != nullptr) {
        if (type == EncryptingType::TEXT) {
          pkcs7 = PKCS7_encrypt(certs, bio, cipher, PKCS7_TEXT | PKCS7_STREAM);
        } else if (type == EncryptingType::BINARY) {
          pkcs7 = PKCS7_encrypt(certs, bio, cipher, PKCS7_BINARY);
        } else {
          pkcs7 = PKCS7_encrypt(certs, bio, cipher, PKCS7_STREAM | PKCS7_DETACHED);
        }
        this->finalized = finalized;
      }
      if (finalized == true) {
        BIO_free(bio);
        bio = nullptr;
      }
    }

    void update(const std::vector<unsigned char> data) {
      if (finalized) {
        return;
      }
      BIO_write(bio, data.data(), data.size());
    }

    void finalize(const std::vector<unsigned char> data, EncryptingType::Value type) {
      if (pkcs7) {
        PKCS7_free(pkcs7);
        pkcs7 = nullptr;
      }
      BIO_write(bio, data.data(), data.size());

      auto cipher = getCipher();
      if (cipher != nullptr) {
        if (type == EncryptingType::TEXT) {
          pkcs7 = PKCS7_encrypt(certs, bio, cipher, PKCS7_TEXT | PKCS7_STREAM);
        } else if (type == EncryptingType::BINARY) {
          pkcs7 = PKCS7_encrypt(certs, bio, cipher, PKCS7_BINARY);
        } else {
          pkcs7 = PKCS7_encrypt(certs, bio, cipher, PKCS7_STREAM | PKCS7_DETACHED);
        }
        finalized = true;
      }
      BIO_free(bio);
      bio = nullptr;
    }

    void finalize(EncryptingType::Value type) {
      if (pkcs7) {
        PKCS7_free(pkcs7);
        pkcs7 = nullptr;
      }

      auto cipher = getCipher();
      if (cipher != nullptr) {
        if (type == EncryptingType::TEXT) {
          pkcs7 = PKCS7_encrypt(certs, bio, cipher, PKCS7_TEXT | PKCS7_STREAM);
        } else if (type == EncryptingType::BINARY) {
          pkcs7 = PKCS7_encrypt(certs, bio, cipher, PKCS7_BINARY);
        } else {
          pkcs7 = PKCS7_encrypt(certs, bio, cipher, PKCS7_STREAM | PKCS7_DETACHED);
        }
        finalized = true;
      }
    }

    const std::vector<unsigned char> decrypt(const Certificate& certificate, const RsaKey& privateKey) {
      EVP_PKEY *pkey = nullptr;
      if (privateKey.onDevice()) {
        // If it's an on-device private key, use public key EVP to mock private key's
        // The LibreSSL will compare type, ameth and rsa->n
        pkey = Converters::rsaPublicKeyToPkey(certificate.publicKey());
      } else {
        pkey = Converters::rsaKeyToPkey(privateKey);
      }
      bio = BIO_new(BIO_s_mem());
      (void)BIO_set_close(bio, BIO_NOCLOSE);
      auto cert = Converters::certificateToX509(certificate);
      auto ret = PKCS7_decrypt(pkcs7, pkey, cert, bio, isSMime ? PKCS7_TEXT : 0);

      if (ret == 0) {
        ret = PKCS7_decrypt(pkcs7, pkey, cert, bio, PKCS7_DETACHED);
      }

      if (ret == 0) {
        std::cerr << "Failed to decrypt\n";
      }

      std::vector<unsigned char> retval;
      while (ret) {
        unsigned char buff[1024];
        int ret = BIO_read(bio, buff, 1024);
        if (ret > 0) {
          for (int i = 0; i < ret; i ++) {
            retval.push_back(buff[i]);
          }
        } else {
          break;
        }
      }

      EVP_PKEY_free(pkey);
      BIO_free(bio);
      bio = nullptr;
      return retval;
    }

    void decrypt(std::function<void(std::string)> onData, std::function<void(void)> onEnd, const Certificate& certificate, const RsaKey& privateKey, int chunkSize = 0) {
      EVP_PKEY *pkey = nullptr;
      if (privateKey.onDevice()) {
        // If it's an on-device private key, use public key EVP to mock private key's
        // The LibreSSL will compare type, ameth and rsa->n
        pkey = Converters::rsaPublicKeyToPkey(certificate.publicKey());
      } else {
        pkey = Converters::rsaKeyToPkey(privateKey);
      }
      auto cert = Converters::certificateToX509(certificate);
      auto ret = PKCS7_decrypt(pkcs7, pkey, cert, bio, isSMime ? PKCS7_TEXT : 0);

      bio = BIO_new(BIO_s_mem());
      (void)BIO_set_close(bio, BIO_NOCLOSE);
      if (ret == 0) {
        ret = PKCS7_decrypt(pkcs7, pkey, cert, bio, PKCS7_DETACHED);
      }
      if (ret == 0) {
        std::cerr << "Failed to decrypt\n" << std::endl;
      }
      if (chunkSize == 0) {
        chunkSize = 1024;
      }
      std::vector<unsigned char> retval;
      unsigned char* buff = new unsigned char[chunkSize + 1];
      while (ret) {
        int r = BIO_read(bio, buff, chunkSize);
        if (r > 0) {
          buff[r] = 0;
          std::string str = (char*)buff;
          onData(str);
        } else {
          break;
        }
      }
      delete[] buff;

      EVP_PKEY_free(pkey);
      BIO_free(bio);
      bio = nullptr;
      onEnd();
    }

    std::vector<std::string> getRecipientSerialNumbers(){
      std::vector<std::string> serialNumbers;
      STACK_OF(PKCS7_RECIP_INFO) *recips = NULL;
      recips = pkcs7->d.enveloped->recipientinfo;
      for (int i = 0; recips && sk_PKCS7_RECIP_INFO_num(recips) > 0 && i < sk_PKCS7_RECIP_INFO_num(recips); i++) {
        PKCS7_RECIP_INFO *recip = sk_PKCS7_RECIP_INFO_value(recips, i);
        PKCS7_ISSUER_AND_SERIAL *issuerSerial = recip->issuer_and_serial;
        auto bn = ASN1_INTEGER_to_BN(issuerSerial->serial, NULL);
        auto dec = Converters::bnToString(bn);
        std::stringstream ss;
        ss << dec;
        auto sn = BigInt::fromString(ss.str());
        serialNumbers.push_back(sn->toHexString());
        PKCS7_RECIP_INFO_free(recip);
        BN_free(bn);
      }
      return serialNumbers;
    }
};

EnvelopedData::EnvelopedData() : impl{std::make_unique<Impl>()} {
}

EnvelopedData::EnvelopedData(const Certificate& certificate, const ObjectId& oid) : impl{std::make_unique<Impl>()} {
  impl->oid.reset(new ObjectId(oid.toString()));
  auto cert = Converters::certificateToX509(certificate);
  sk_X509_push(impl->certs, cert);
}

EnvelopedData* EnvelopedData::fromDer(const std::vector<unsigned char> der) {
  auto p = new EnvelopedData();

  p->impl->fromDer(der);

  if (!p->impl->success) {
    return nullptr;
  }
  return p;
}

void EnvelopedData::addRecipient(const Certificate& certificate) {
  auto cert = Converters::certificateToX509(certificate);
  impl->appendCertificate(cert);
}

EnvelopedData* EnvelopedData::fromPem(const std::string pem) {
  auto p = new EnvelopedData();

  p->impl->fromPem(pem);

  if (!p->impl->success) {
    return nullptr;
  }
  return p;
}



const std::vector<unsigned char> EnvelopedData::toDer() const {
  std::vector<unsigned char> retval;
  int ret;
  BIO* mem = BIO_new(BIO_s_mem());
  (void)BIO_set_close(mem, BIO_NOCLOSE);

  ret = i2d_PKCS7_bio_stream(mem, impl->pkcs7, nullptr, 0);

  while (ret) {
    unsigned char buff[1024];
    int ret = BIO_read(mem, buff, 1024);
    if (ret > 0) {
      retval.insert(retval.end(), buff, buff + ret);
    } else {
      break;
    }
  }
  BIO_free(mem);

  return retval;

}

EnvelopedData::~EnvelopedData() = default;

void EnvelopedData::encrypt(const std::vector<unsigned char> data) {
  impl->encrypt(data, EncryptingType::BINARY, true);
}

const std::string EnvelopedData::toPem() const {
  std::string retval;
  int ret;
  BIO* mem = BIO_new(BIO_s_mem());
  (void)BIO_set_close(mem, BIO_NOCLOSE);

  ret = PEM_write_bio_PKCS7_stream(mem, impl->pkcs7, NULL, 0);

  while (ret) {
    unsigned char buff[1025];
    int ret = BIO_read(mem, buff, 1024);
    if (ret > 0) {
      buff[ret] = 0;
      std::string str = (char*)buff;
      retval += str;
    } else {
      break;
    }
  }
  BIO_free(mem);

  return retval;

}


const std::vector<unsigned char> EnvelopedData::decrypt(const Certificate& certificate, const RsaKey& privateKey) const {
  return impl->decrypt(certificate, privateKey);
}

void EnvelopedData::decrypt(std::function<void(std::string)> onData, std::function<void(void)> onEnd, const Certificate& certificate, const RsaKey& privateKey, int chunkSize = 0) const {
  impl->decrypt(onData, onEnd, certificate, privateKey, chunkSize);
}

void EnvelopedData::toSMime(std::function<void(std::string)> onData, std::function<void(void)> onEnd, EncryptingType::Value type = EncryptingType::DEFAULT, int chunkSize = 0) const {

  if (!impl->finalized) {
    impl->finalize(type);
  }
  BIO* out = BIO_new(BIO_s_mem());
  int flags = PKCS7_STREAM;
  if (type == EncryptingType::TEXT) {
    flags = PKCS7_TEXT | PKCS7_STREAM;
  }
  auto r = SMIME_write_PKCS7(out, impl->pkcs7, impl->bio, flags);

  if (chunkSize == 0) {
    chunkSize = 1024;
  }
  unsigned char* buff = new unsigned char[chunkSize + 1];
  while (r) {
    int ret = BIO_read(out, buff, chunkSize);
    if (ret > 0) {
      buff[ret] = 0;
      std::string str = (char*)buff;
      onData(str);
    } else {
      break;
    }
  }
  delete[] buff;
  BIO_free(out);

  onEnd();
}

void EnvelopedData::encryptSMime(const std::vector<unsigned char> data, EncryptingType::Value type) {
  impl->encrypt(data, type);
}

void EnvelopedData::updateSMime(const std::vector<unsigned char> data) {
  if (impl->finalized) {
    return;
  }
  impl->update(data);
}

void EnvelopedData::finalizeEncryptSMime(EncryptingType::Value type) {
  impl->finalize(type);
}

void EnvelopedData::finalizeEncryptSMime(const std::vector<unsigned char> data, EncryptingType::Value type) {
  impl->finalize(data, type);
}

const std::string EnvelopedData::toSMime() const {
  std::string retval;
  EncryptingType::Value type = EncryptingType::DEFAULT;
  toSMime([&retval](std::string s) {
        retval += s;
      }, [](){}, type);

  return retval;
}

const std::string EnvelopedData::toSMime(EncryptingType::Value type) const {
  std::string retval;

  toSMime([&retval](std::string s) {
        retval += s;
      }, [](){}, type);

  return retval;
}

EnvelopedData* EnvelopedData::fromSMime(const std::string smime) {
  auto p = new EnvelopedData();

  p->impl->fromSMime(smime);

  if (!p->impl->success) {
    return nullptr;
  }
  return p;
}

EnvelopedData* EnvelopedData::fromSMimeInit(const std::string smime) {
  auto p = new EnvelopedData();
  p->impl->fromSMimeInit(smime);
  return p;
}

void EnvelopedData::fromSMimeUpdate(const std::string smime) {
  impl->fromSMimeUpdate(smime);
}

void EnvelopedData::fromSMimeFinalize() {
  impl->fromSMimeFinalize();
}

EnvelopedData* EnvelopedData::fromSMimeFile(const std::string path) {
  auto p = new EnvelopedData();

  p->impl->fromSMimeFile(path);

  if (!p->impl->success) {
    return nullptr;
  }
  return p;
}

std::vector<std::string> EnvelopedData::getRecipientSerialNumbers(){
  return impl->getRecipientSerialNumbers();
}



} // namespace Erpiko
