#include "certificate_generator.hpp"
#include <ostream>

namespace proxy {
namespace cert {

// TODO: This should be implemented better, smart pointers, error checking, etc.

using X509_ptr = std::unique_ptr<X509, decltype(&X509_free)>;
using BIO_MEM_ptr = std::unique_ptr<BIO, decltype(&BIO_free)>;
using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using RSA_ptr = std::unique_ptr<RSA, decltype(&RSA_free)>;
using X509_EXTENSION_ptr =
    std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)>;
using BIGNUM_ptr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;

#define SERIAL_RAND_BITS 64
int rand_serial(BIGNUM *b, ASN1_INTEGER *ai) {
  BIGNUM *btmp;
  int ret = 0;
  if (b)
    btmp = b;
  else
    btmp = BN_new();
  if (!btmp)
    return 0;
  if (!BN_pseudo_rand(btmp, SERIAL_RAND_BITS, 0, 0))
    goto error;
  if (ai && !BN_to_ASN1_INTEGER(btmp, ai))
    goto error;
  ret = 1;
error:
  if (!b)
    BN_free(btmp);
  return ret;
}

void cs_cert_set_subject_alt_name(X509 *x509_cert, std::string host) {
  GENERAL_NAMES *gens = sk_GENERAL_NAME_new_null();

  GENERAL_NAME *gen_dns = GENERAL_NAME_new();
  ASN1_IA5STRING *ia5 = ASN1_IA5STRING_new();
  ASN1_STRING_set(ia5, host.data(), host.length());
  GENERAL_NAME_set0_value(gen_dns, GEN_DNS, ia5);
  sk_GENERAL_NAME_push(gens, gen_dns);

  X509_add1_ext_i2d(x509_cert, NID_subject_alt_name, gens, 0,
                    X509V3_ADD_DEFAULT);

  sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
}

void getOpenSSLError() {
  BIO *bio = BIO_new(BIO_s_mem());
  ERR_print_errors(bio);
  char *buf;
  size_t len = BIO_get_mem_data(bio, &buf);
  std::cout << std::string(buf, len) << std::endl;
  BIO_free(bio);
}

int cert_add_ext(X509 *issuer, X509 *subject, int nid, const char *value) {
  X509_EXTENSION *ex;
  X509V3_CTX ctx;
  /* No configuration database */
  X509V3_set_ctx_nodb(&ctx);
  /* Set issuer and subject certificates in the context */
  X509V3_set_ctx(&ctx, issuer, subject, NULL, NULL, 0);

  ex = X509V3_EXT_nconf_nid(NULL, &ctx, nid, value);
  if (!ex) {
    ERR_print_errors_fp(stdout);
    return 0;
  }
  X509_add_ext(subject, ex, -1);
  X509_EXTENSION_free(ex);
  return 1;
}

boost::tuple<std::string, std::string> generate_root_certificate() {
  int rc = 0;
  unsigned long err = 0;

  EVP_PKEY_ptr private_key(EVP_PKEY_new(), EVP_PKEY_free);
  X509_ptr x509(X509_new(), X509_free);
  RSA *rsa = RSA_new();
  BIGNUM_ptr bn(BN_new(), BN_free);
  BN_set_word(bn.get(), RSA_F4);
  RSA_generate_key_ex(rsa, 2048, bn.get(), NULL);
  EVP_PKEY_assign_RSA(private_key.get(), rsa);

  X509_set_version(x509.get(), 2);

  ASN1_INTEGER *sno = ASN1_INTEGER_new();
  rand_serial(NULL, sno);
  X509_set_serialNumber(x509.get(), sno);
  ASN1_INTEGER_free(sno);

  X509_gmtime_adj(X509_get_notBefore(x509.get()), 0);
  X509_gmtime_adj(X509_get_notAfter(x509.get()), (long)60 * 60 * 24 * 365 * 2);
  X509_set_pubkey(x509.get(), private_key.get());
  X509_NAME *name = X509_get_subject_name(x509.get());

  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                             (const unsigned char *)"Proxy CA", -1, -1, 0);

  /* Its self signed so set the issuer name to be the same as the
   * subject.
   */
  X509_set_issuer_name(x509.get(), name);

  /* Add various extensions: standard extensions */
  cert_add_ext(x509.get(), x509.get(), NID_basic_constraints, "CA:TRUE");
  cert_add_ext(x509.get(), x509.get(), NID_key_usage, "keyCertSign");

  cert_add_ext(x509.get(), x509.get(), NID_subject_key_identifier, "hash");
  cert_add_ext(x509.get(), x509.get(), NID_authority_key_identifier,
               "keyid:always");

  X509_sign(x509.get(), private_key.get(), EVP_sha256());

  BIO_MEM_ptr bio(BIO_new(BIO_s_mem()), BIO_free);
  rc = PEM_write_bio_X509(bio.get(), x509.get());
  err = ERR_get_error();

  if (rc != 1) {
    std::cerr << "PEM_write_bio_X509 failed, error " << err << ", ";
    std::cerr << std::hex << "0x" << err;
    getOpenSSLError();
    exit(1);
  }

  BUF_MEM *mem = NULL;
  BIO_get_mem_ptr(bio.get(), &mem);
  err = ERR_get_error();

  if (!mem || !mem->data || !mem->length) {
    std::cerr << "BIO_get_mem_ptr failed, error " << err << ", ";
    std::cerr << std::hex << "0x" << err;
    getOpenSSLError();
    exit(2);
  }

  std::string pem(mem->data, mem->length);

  BIO_MEM_ptr bio_private(BIO_new(BIO_s_mem()), BIO_free);
  rc = PEM_write_bio_PrivateKey(bio_private.get(), private_key.get(), NULL,
                                NULL, 0, 0, NULL);

  err = ERR_get_error();

  if (rc != 1) {
    std::cerr << "PEM_write_bio_PrivateKey failed, error " << err << ", ";
    std::cerr << std::hex << "0x" << err;
    getOpenSSLError();
    exit(1);
  }

  BUF_MEM *mem_private_key = NULL;
  BIO_get_mem_ptr(bio_private.get(), &mem_private_key);

  err = ERR_get_error();

  if (!mem_private_key || !mem_private_key->data || !mem_private_key->length) {
    std::cerr << "BIO_get_mem_ptr failed, error " << err << ", ";
    std::cerr << std::hex << "0x" << err;
    getOpenSSLError();
    exit(2);
  }

  std::string private_key_str(mem_private_key->data, mem_private_key->length);

  return boost::make_tuple(private_key_str, pem);
}

boost::tuple<std::string, std::string>
generate_certificate(std::string host, std::string root_private_key,
                     std::string root_cert) {
  int rc = 0;
  unsigned long err = 0;

  BIO_MEM_ptr bio_root(BIO_new(BIO_s_mem()), BIO_free);
  BIO_write(bio_root.get(), root_private_key.data(), root_private_key.length());
  EVP_PKEY *tmp = NULL;
  PEM_read_bio_PrivateKey(bio_root.get(), &tmp, 0, 0);

  EVP_PKEY_ptr evp_root_private_key(tmp, EVP_PKEY_free);

  BIO_MEM_ptr bio_root_cert(BIO_new(BIO_s_mem()), BIO_free);
  BIO_write(bio_root_cert.get(), root_cert.data(), root_cert.length());
  X509 *issuer_tmp = NULL;
  PEM_read_bio_X509(bio_root_cert.get(), &issuer_tmp, NULL, NULL);

  X509_ptr issuer(issuer_tmp, X509_free);

  EVP_PKEY_ptr private_key(EVP_PKEY_new(), EVP_PKEY_free);
  X509_ptr x509(X509_new(), X509_free);
  // RSA_ptr rsa(RSA_new(), RSA_free);
  RSA *rsa = RSA_new();
  BIGNUM_ptr bn(BN_new(), BN_free);
  BN_set_word(bn.get(), RSA_F4);
  RSA_generate_key_ex(rsa, 2048, bn.get(), NULL);
  EVP_PKEY_assign_RSA(private_key.get(), rsa);

  X509_set_version(x509.get(), 2);

  ASN1_INTEGER *sno = ASN1_INTEGER_new();
  rand_serial(NULL, sno);
  X509_set_serialNumber(x509.get(), sno);
  ASN1_INTEGER_free(sno);

  // ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), 2);
  X509_gmtime_adj(X509_get_notBefore(x509.get()), 0);
  X509_gmtime_adj(X509_get_notAfter(x509.get()), (long)60 * 60 * 24 * 365 * 2);
  X509_set_pubkey(x509.get(), private_key.get());
  X509_NAME *name = X509_get_subject_name(x509.get());

  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                             (const unsigned char *)host.data(), host.length(),
                             -1, 0);

  /* Its self signed so set the issuer name to be the same as the
   * subject.
   */

  X509_NAME *iss_name = X509_get_issuer_name(x509.get());

  X509_NAME_add_entry_by_txt(iss_name, "CN", MBSTRING_ASC,
                             (const unsigned char *)"Proxy CA", -1, -1, 0);

  /* Its self signed so set the issuer name to be the same as the
   * subject.
   */
  X509_set_issuer_name(x509.get(), iss_name);

  cs_cert_set_subject_alt_name(x509.get(), host);

  cert_add_ext(issuer.get(), x509.get(), NID_basic_constraints, "CA:FALSE");
  cert_add_ext(issuer.get(), x509.get(), NID_subject_key_identifier, "hash");
  cert_add_ext(issuer.get(), x509.get(), NID_authority_key_identifier,
               "keyid:always");

  X509_set_pubkey(x509.get(), private_key.get());

  X509_sign(x509.get(), evp_root_private_key.get(), EVP_sha256());

  BIO_MEM_ptr bio(BIO_new(BIO_s_mem()), BIO_free);
  rc = PEM_write_bio_X509(bio.get(), x509.get());
  err = ERR_get_error();

  if (rc != 1) {
    std::cerr << "PEM_write_bio_X509 failed, error " << err << ", ";
    std::cerr << std::hex << "0x" << err;
    getOpenSSLError();
    exit(1);
  }

  BUF_MEM *mem = NULL;
  BIO_get_mem_ptr(bio.get(), &mem);
  err = ERR_get_error();

  if (!mem || !mem->data || !mem->length) {
    std::cerr << "BIO_get_mem_ptr failed, error " << err << ", ";
    std::cerr << std::hex << "0x" << err;
    getOpenSSLError();
    exit(2);
  }

  std::string pem(mem->data, mem->length);

  BIO_MEM_ptr bio_private(BIO_new(BIO_s_mem()), BIO_free);
  rc = PEM_write_bio_PrivateKey(bio_private.get(), private_key.get(), NULL,
                                NULL, 0, 0, NULL);

  err = ERR_get_error();

  if (rc != 1) {
    std::cerr << "PEM_write_bio_PrivateKey failed, error " << err << ", ";
    std::cerr << std::hex << "0x" << err;
    getOpenSSLError();
    exit(1);
  }

  BUF_MEM *mem_private_key = NULL;
  BIO_get_mem_ptr(bio_private.get(), &mem_private_key);

  err = ERR_get_error();

  if (!mem_private_key || !mem_private_key->data || !mem_private_key->length) {
    std::cerr << "BIO_get_mem_ptr failed, error " << err << ", ";
    std::cerr << std::hex << "0x" << err;
    getOpenSSLError();
    exit(2);
  }

  std::string private_key_str(mem_private_key->data, mem_private_key->length);

  return boost::make_tuple(private_key_str, pem);
}

} // namespace cert
} // namespace proxy