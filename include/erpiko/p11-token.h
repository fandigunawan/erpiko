#ifndef _P11_TOKEN_H_
#define _P11_TOKEN_H_
#include "token.h"
namespace Erpiko {

class P11Token : Token {
  public:
    P11Token();
    virtual ~P11Token();
    virtual bool load(const std::string path);
    virtual bool isValid();
    virtual CardStatus::Value waitForCardStatus(int &slot) const;
    virtual bool login(const unsigned long slot, const std::string& pin) const;
    virtual bool logout() const;
    virtual void setKeyId(const unsigned int id, const std::string& label);
    virtual TokenOpResult::Value putData(const std::string& applicationName, std::string& label, std::vector<unsigned char> data);
    virtual std::vector<unsigned char> getData(const std::string& applicationName, std::string& label);
    virtual bool removeData(const std::string& applicationName, const std::string& label);
    virtual std::vector<Certificate*> getCertificates(bool);
    virtual TokenOpResult::Value putCertificate(const Certificate& cert);
    virtual bool removeCertificate(const Certificate& cert);
    virtual TokenOpResult::Value putPrivateKey(const RsaKey& data, const std::string& labelStr);
    virtual RsaKey* getPrivateKey(const RsaPublicKey& publicKey);
    virtual bool removePrivateKey(const std::string& labelStr);
    virtual void* engine() const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace Erpiko
#endif //  _P11_TOKEN_H_
