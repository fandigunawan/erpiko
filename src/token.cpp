#include "erpiko/token.h"
#include "engine-p11.h"
#include <openssl/engine.h>

using namespace std;
namespace Erpiko {

class Token::Impl {
  public:

  EngineP11& engine;
  bool valid = false;

  Impl() : engine{ EngineP11::getInstance() } {
    engine.init();
  }

  ~Impl() {
    engine.finalize();
  }

  bool load(string path) {
    valid = engine.load(path);
    return valid;
  }

};

Token::Token() : impl{ std::make_unique<Impl>()} {
}

Token::~Token() = default;

bool
Token::load(const std::string path) {
  return impl->load(path);
}

bool
Token::isValid() {
  return impl->valid;
}

CardStatus::Value Token::waitForCardStatus(int &slot) const {
  bool result = impl->engine.waitForCardStatus(slot);
  if (result != true) {
    return CardStatus::NOT_PRESENT;
  }
  return CardStatus::PRESENT;
}

bool
Token::login(const unsigned long slot, const std::string& pin) const {
  return impl->engine.login(slot, pin);
}

bool
Token::logout() const {
  return impl->engine.logout();
}

void
Token::setKeyId(const unsigned int id, const std::string& label) {
  impl->engine.setKeyLabel(label);
  impl->engine.setKeyId(id);
}

TokenOpResult::Value
Token::putData(const std::string& applicationName, std::string& label, std::vector<unsigned char> data) {
  return impl->engine.putData(applicationName, label, data);
}

std::vector<unsigned char>
Token::getData(const std::string& applicationName, std::string& label) {
  return impl->engine.getData(applicationName, label);
}

} // namespace Erpiko