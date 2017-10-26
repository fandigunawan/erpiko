#include "engine-p11.h"
#include "erpiko/utils.h"
#include "pkcs11/cryptoki.h"
#include <iostream>
#include <string>
#include <openssl/engine.h>
#include <openssl/rsa.h>
#include <dlfcn.h>

ENGINE *erpikoEngine = nullptr;
CK_FUNCTION_LIST_PTR F;

using namespace std;
using namespace Erpiko;

int rsaKeygen(RSA *rsa, int bits, BIGNUM *exp, BN_GENCB *cb) {
  (void) cb;

  char* eStr = BN_bn2hex(exp);
  if (!eStr) return 0;
  auto v = Utils::fromHexString(eStr);
  CK_BYTE publicExponent[v.size()];
  for (unsigned int i = 0; i < v.size(); i ++) {
    publicExponent[i] = v.at(i);
  }
  free(eStr);

  EngineP11& p11 = EngineP11::getInstance();

  CK_OBJECT_HANDLE publicKey, privateKey;
  CK_MECHANISM mechanism = {
    CKM_RSA_PKCS_KEY_PAIR_GEN, NULL_PTR, 0
  };
  CK_ULONG modulusBits = bits;
  CK_BYTE* subject = reinterpret_cast<unsigned char*>(const_cast<char*>(p11.getKeyLabel().c_str()));
  CK_BYTE id[] = { (unsigned char)p11.getKeyId() };
  CK_BBOOL trueValue = CK_TRUE;
  CK_ATTRIBUTE publicKeyTemplate[] = {
    {CKA_ID, id, sizeof(id)},
    {CKA_LABEL, subject, p11.getKeyLabel().size()},
    {CKA_TOKEN, &trueValue, sizeof(trueValue)},
    {CKA_ENCRYPT, &trueValue, sizeof(trueValue)},
    {CKA_VERIFY, &trueValue, sizeof(trueValue)},
    {CKA_WRAP, &trueValue, sizeof(trueValue)},
    {CKA_MODULUS_BITS, &modulusBits, sizeof(modulusBits)},
    {CKA_PUBLIC_EXPONENT, publicExponent, sizeof(publicExponent)}
  };
  CK_ATTRIBUTE privateKeyTemplate[] = {
    {CKA_ID, id, sizeof(id)},
    {CKA_LABEL, subject, p11.getKeyLabel().size()},
    {CKA_TOKEN, &trueValue, sizeof(trueValue)},
    {CKA_PRIVATE, &trueValue, sizeof(trueValue)},
    {CKA_SENSITIVE, &trueValue, sizeof(trueValue)},
    {CKA_DECRYPT, &trueValue, sizeof(trueValue)},
    {CKA_SIGN, &trueValue, sizeof(trueValue)},
    {CKA_UNWRAP, &trueValue, sizeof(trueValue)}
  };

  CK_RV rv = CKR_OK;
  rv = F->C_GenerateKeyPair(p11.getSession(),
      &mechanism,
      publicKeyTemplate, 8,
      privateKeyTemplate, 8,
      &publicKey,
      &privateKey);

  unsigned char e[1024] = { 0 };
  unsigned char n[1024] = { 0 };
  CK_ATTRIBUTE pubValueT[] = {
    {CKA_PUBLIC_EXPONENT, e, sizeof(e)},
    {CKA_MODULUS, n, sizeof(n)}
  };

  rv = F->C_GetAttributeValue(p11.getSession(), publicKey, pubValueT, 2);
  std::vector<unsigned char> vec(e, e + pubValueT[0].ulValueLen);
  if (rv == CKR_OK)
  if ((rsa->e = BN_bin2bn(vec.data(), vec.size(), nullptr)) != nullptr)
  if ((rsa->n = BN_bin2bn(n, pubValueT[1].ulValueLen, nullptr)) != nullptr)
  {
    return 1;
  }
  return 0;
}

int rsaPubEncrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding) {
  (void) rsa;
  (void) padding;

  EngineP11& p11 = EngineP11::getInstance();
  CK_RSA_PKCS_OAEP_PARAMS oaepParams = {CKM_SHA_1, CKG_MGF1_SHA1, 1, nullptr, 0 };
  CK_MECHANISM mechanism = {
    CKM_RSA_PKCS_OAEP, &oaepParams, sizeof(oaepParams)
  };

  CK_BYTE id[] = { (unsigned char) p11.getKeyId() };
  CK_BYTE* subject = reinterpret_cast<unsigned char*>(const_cast<char*>(p11.getKeyLabel().c_str()));
  CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
  CK_KEY_TYPE publicKeyType = CKK_RSA;
  CK_ATTRIBUTE t[] = {
    { CKA_CLASS, &keyClass, sizeof(keyClass) },
    { CKA_KEY_TYPE,  &publicKeyType, sizeof(publicKeyType) },
    { CKA_ID, id, sizeof(id) },
    { CKA_LABEL, subject, p11.getKeyLabel().size()}
  };
  CK_ULONG objectCount;
  CK_OBJECT_HANDLE key;

  CK_RV rv = CKR_OK;
  rv = F->C_FindObjectsInit(p11.getSession(), t, 4);
  if (rv != CKR_OK) {
    return 0;
  }

  rv = F->C_FindObjects(p11.getSession(), &key, 1, &objectCount);
  if (rv != CKR_OK) {
    return 0;
  }

  rv = F->C_FindObjectsFinal(p11.getSession());
  if (objectCount == 0) return 0;

  rv = F->C_EncryptInit(p11.getSession(), &mechanism, key);
  if (rv != CKR_OK) {
    return 0;
  }
  CK_ULONG outLength;
  rv = F->C_Encrypt(p11.getSession(), const_cast<unsigned char*>(from), flen, to, &outLength);
  if (rv != CKR_OK) {
    return 0;
  }

  return outLength;
}

int rsaPrivDecrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding) {
  (void) rsa;
  (void) padding;

  EngineP11& p11 = EngineP11::getInstance();
  CK_RSA_PKCS_OAEP_PARAMS oaepParams = {CKM_SHA_1, CKG_MGF1_SHA1, 1, nullptr, 0 };
  CK_MECHANISM mechanism = {
    CKM_RSA_PKCS_OAEP, &oaepParams, sizeof(oaepParams)
  };

  CK_BYTE id[] = { (unsigned char) p11.getKeyId() };
  CK_BYTE* subject = reinterpret_cast<unsigned char*>(const_cast<char*>(p11.getKeyLabel().c_str()));
  CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
  CK_KEY_TYPE publicKeyType = CKK_RSA;
  CK_ATTRIBUTE t[] = {
    { CKA_CLASS, &keyClass, sizeof(keyClass) },
    { CKA_KEY_TYPE,  &publicKeyType, sizeof(publicKeyType) },
    { CKA_ID, id, sizeof(id) },
    { CKA_LABEL, subject, p11.getKeyLabel().size()}
  };
  CK_ULONG objectCount;
  CK_OBJECT_HANDLE key;

  CK_RV rv = CKR_OK;
  rv = F->C_FindObjectsInit(p11.getSession(), t, 4);
  if (rv != CKR_OK) {
    return 0;
  }

  rv = F->C_FindObjects(p11.getSession(), &key, 1, &objectCount);
  if (rv != CKR_OK) {
    return 0;
  }

  rv = F->C_FindObjectsFinal(p11.getSession());
  if (objectCount == 0) return 0;

  rv = F->C_DecryptInit(p11.getSession(), &mechanism, key);
  if (rv != CKR_OK) {
    return 0;
  }
  CK_ULONG outLength = flen;
  rv = F->C_Decrypt(p11.getSession(), const_cast<unsigned char*>(from), flen, to, &outLength);

  return outLength;
}

int rsaSign(int type, const unsigned char *from, unsigned int flen, unsigned char *to, unsigned int *siglen, const RSA *rsa) {
  (void) rsa;
  (void) type;

  EngineP11& p11 = EngineP11::getInstance();
  CK_MECHANISM mechanism = {
     CKM_SHA256_RSA_PKCS, nullptr, 0
  };

  CK_BYTE id[] = { (unsigned char) p11.getKeyId() };
  CK_BYTE* subject = reinterpret_cast<unsigned char*>(const_cast<char*>(p11.getKeyLabel().c_str()));
  CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
  CK_KEY_TYPE pKeyType = CKK_RSA;
  CK_ATTRIBUTE t[] = {
    { CKA_CLASS, &keyClass, sizeof(keyClass) },
    { CKA_KEY_TYPE,  &pKeyType, sizeof(pKeyType) },
    { CKA_ID, id, sizeof(id) },
    { CKA_LABEL, subject, p11.getKeyLabel().size()}
  };
  CK_ULONG objectCount;
  CK_OBJECT_HANDLE key;

  CK_RV rv = CKR_OK;
  rv = F->C_FindObjectsInit(p11.getSession(), t, 4);
  if (rv != CKR_OK) {
    return 0;
  }

  rv = F->C_FindObjects(p11.getSession(), &key, 1, &objectCount);
  if (rv != CKR_OK) {
    return 0;
  }

  rv = F->C_FindObjectsFinal(p11.getSession());
  if (objectCount == 0) return 0;

  rv = F->C_SignInit(p11.getSession(), &mechanism, key);
  if (rv != CKR_OK) {
    return 0;
  }
  CK_ULONG outLength = 128; // FIXME
  rv = F->C_Sign(p11.getSession(), const_cast<unsigned char*>(from), flen, to, &outLength);
  if (rv != CKR_OK) {
    return 0;
  }

  *siglen = (unsigned int) outLength;
  return 1;
}

int rsaVerify(int type, const unsigned char *from, unsigned int flen, const unsigned char *sig, unsigned int siglen, const RSA *rsa) {
  (void) rsa;
  (void) type;

  EngineP11& p11 = EngineP11::getInstance();
  CK_MECHANISM mechanism = {
     CKM_SHA256_RSA_PKCS, nullptr, 0
  };

  CK_BYTE id[] = { (unsigned char) p11.getKeyId() };
  CK_BYTE* subject = reinterpret_cast<unsigned char*>(const_cast<char*>(p11.getKeyLabel().c_str()));
  CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
  CK_KEY_TYPE publicKeyType = CKK_RSA;
  CK_ATTRIBUTE t[] = {
    { CKA_CLASS, &keyClass, sizeof(keyClass) },
    { CKA_KEY_TYPE,  &publicKeyType, sizeof(publicKeyType) },
    { CKA_ID, id, sizeof(id) },
    { CKA_LABEL, subject, p11.getKeyLabel().size()}
  };
  CK_ULONG objectCount;
  CK_OBJECT_HANDLE key;

  CK_RV rv = CKR_OK;
  rv = F->C_FindObjectsInit(p11.getSession(), t, 4);
  if (rv != CKR_OK) {
    return 0;
  }

  rv = F->C_FindObjects(p11.getSession(), &key, 1, &objectCount);
  if (rv != CKR_OK) {
    return 0;
  }

  rv = F->C_FindObjectsFinal(p11.getSession());
  if (objectCount == 0) return 0;

  rv = F->C_VerifyInit(p11.getSession(), &mechanism, key);
  if (rv != CKR_OK) {
    return 0;
  }
  rv = F->C_Verify(p11.getSession(), const_cast<unsigned char*>(from), flen, const_cast<unsigned char*>(sig), siglen);
  if (rv != CKR_OK) {
    return 0;
  }

  return 1;
}




const RSA_METHOD* rsaMethod() {
  static RSA_METHOD* m = nullptr;

  if (m == nullptr) {
    m = const_cast<RSA_METHOD*>(RSA_get_default_method());
    m->flags = RSA_FLAG_SIGN_VER;
    m->rsa_keygen = rsaKeygen;
    m->rsa_pub_enc = rsaPubEncrypt;
    m->rsa_priv_dec = rsaPrivDecrypt;
    m->rsa_sign = rsaSign;
    m->rsa_verify = rsaVerify;
  }
  return m;
}

static const int meths[] = {
  EVP_PKEY_RSA,
};

static int pkey_meths(ENGINE*e, EVP_PKEY_METHOD** meth, const int** nids, int nid) {
  (void) e;
  (void) meth;
  (void) nids;
  (void) nid;

  if (nid == EVP_PKEY_RSA) {
    *meth = const_cast<EVP_PKEY_METHOD*>(EVP_PKEY_meth_find(nid));
    return 1;
  } else if (nid != 0) {
    return 0;
  }
  if (nids != NULL) {
    *nids = meths;
    return 1;
  }
  return 0;
}


int e_init(ENGINE* e) {
  (void) e;

  return 1;
}

namespace Erpiko {

void
EngineP11::init() {
  if (initialized) return;
  if (erpikoEngine != nullptr) {
    ENGINE_free(erpikoEngine);
    erpikoEngine = nullptr;
  }
  erpikoEngine = ENGINE_new();
  if (
      ENGINE_set_id(erpikoEngine, "Erpiko-P11") &&
      ENGINE_set_name(erpikoEngine, "Erpiko-P11 Engine") &&
      ENGINE_set_init_function(erpikoEngine, e_init) &&
      ENGINE_set_RSA(erpikoEngine, rsaMethod()) &&
      ENGINE_set_pkey_meths(erpikoEngine, pkey_meths)
      )
  {
    if (ENGINE_init(erpikoEngine)) {
      initialized = true;
    }
  }
}

bool
EngineP11::load(const string path) {
  if (lib && F) return true;

  if (lib) {
    dlclose(lib);
    lib = nullptr;
  }

  lib = dlopen(path.c_str(), RTLD_LAZY);
  if (!lib) {
    return false;
  }
  auto getF = reinterpret_cast<CK_C_GetFunctionList> (reinterpret_cast<long long> (dlsym(lib, "C_GetFunctionList")));
  if (getF != nullptr) {
    CK_RV rv = getF(&F);
    if (rv == CKR_OK) {
      return F->C_Initialize(nullptr) == CKR_OK;
    }
  }
  return false;
}

void
EngineP11::finalize() {
  if (F != nullptr) {
    F->C_Finalize(nullptr);
    F = nullptr;
    dlclose(lib);
    lib = nullptr;
  }
}

bool
EngineP11::logout() {
  if (F->C_Logout(session) == CKR_OK &&
      F->C_CloseSession(session) == CKR_OK) {
    session = 0;
    return true;
  }
  return false;
}

bool
EngineP11::login(const unsigned long slot, const string& pin) {
  if (!F && !F->C_OpenSession) return false;
  if (!F && !F->C_Login) return false;

  CK_RV rv = F->C_OpenSession(slot, CKF_SERIAL_SESSION | CKF_RW_SESSION,
                          nullptr, nullptr, &session);
  if (rv != CKR_OK) return false;
  rv = F->C_Login(session, CKU_USER, reinterpret_cast<unsigned char*>(const_cast<char*>(pin.c_str())), pin.size());
  if (rv != CKR_OK) return false;
  return true;
}

} // namespace Erpiko
