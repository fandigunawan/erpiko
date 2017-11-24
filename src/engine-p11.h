#ifndef _ENGINE_P11_H
#define _ENGINE_P11_H

#include <map>
#include <string>
#include <iostream>
#include "erpiko/token.h"
#ifdef WIN32
#include <Windows.h>
#endif
using namespace std;

namespace Erpiko {

class EngineP11 {
  bool initialized = false;
#ifdef WIN32
  HMODULE lib;
#else
  void* lib = nullptr;
#endif
  unsigned long session = 0;
  string keyLabel;
  unsigned int keyId;

  private:
    EngineP11() { }

  public:
    static EngineP11& getInstance() {
      static EngineP11 me;

      return me;
    }

    EngineP11(EngineP11 const&) = delete;
    void operator=(EngineP11 const&) = delete;
    void init();
    bool load(const std::string path);
    void finalize();
    bool waitForCardStatus(int &slot);
    bool login(const unsigned long slot, const string& pin);
    bool logout();
    unsigned long getSession() {
      return session;
    }

    void setKeyLabel(const string& label) {
      keyLabel = label;
    }

    void setKeyId(const unsigned int id) {
      keyId = id;
    }

    const string& getKeyLabel() const {
      return keyLabel;
    }

    unsigned int getKeyId() const {
      return keyId;
    }

    TokenOpResult::Value putData(const std::string& applicationName, std::string& label, std::vector<unsigned char> data);
    std::vector<unsigned char> getData(const std::string& applicationName, std::string& label);
  };
} // namespace Erpiko
#endif // _ENGINE_P11_H
