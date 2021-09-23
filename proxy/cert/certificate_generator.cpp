#include "certificate_generator.hpp"
#include <boost/asio/ssl.hpp>

namespace proxy {
namespace cert {

typedef std::unique_ptr<X509, decltype(&X509_free)> X509_ptr;
typedef std::unique_ptr<BIO, decltype(&BIO_free)> BIO_MEM_ptr;
typedef std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> EVP_PKEY_ptr;
typedef std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)>
    X509_ex_ptr;
typedef std::unique_ptr<ASN1_IA5STRING, decltype(&ASN1_IA5STRING_free)>
    ASN1_IA5STRING_ptr;
typedef std::unique_ptr<GENERAL_NAME, decltype(&GENERAL_NAME_free)>
    GENERAL_NAME_ptr;
typedef std::unique_ptr<GENERAL_NAMES, decltype(&GENERAL_NAMES_free)>
    GENERAL_NAMES_ptr;

certificate_generator::certificate_generator(rsa_maker &rsa_maker)
    : rsa_maker_(rsa_maker) {}

int add_extension(X509V3_CTX &ctx, X509 *subject, int nid, const char *value) {
  X509_ex_ptr ex(X509V3_EXT_nconf_nid(nullptr, &ctx, nid, value),
                 X509_EXTENSION_free);
  if (!ex) {
    return 0;
  }
  if (!X509_add_ext(subject, ex.get(), -1)) {
    return 0;
  }
  return 1;
}

boost::tuple<std::string, std::string>
certificate_generator::generate_root_certificate() {
  EVP_PKEY_ptr private_key(EVP_PKEY_new(), EVP_PKEY_free);
  if (!private_key) {
    return boost::make_tuple("", "");
  }
  X509_ptr x509(X509_new(), X509_free);
  if (!x509) {
    return boost::make_tuple("", "");
  }

  RSA_ptr rsa = generate_rsa();
  if (!rsa) {
    return boost::make_tuple("", "");
  }
  if (!EVP_PKEY_assign_RSA(private_key.get(),
                           rsa.get())) { // Transfers ownership of rsa
    return boost::make_tuple("", "");
  }
  (void)rsa.release();

  if (!X509_set_version(x509.get(), 2)) {
    return boost::make_tuple("", "");
  }

  if (X509_gmtime_adj(X509_get_notBefore(x509.get()), 0) == nullptr) {
    return boost::make_tuple("", "");
  }
  if (X509_gmtime_adj(X509_get_notAfter(x509.get()),
                      (long)60 * 60 * 24 * 365 * 20) == nullptr) {
    return boost::make_tuple("", "");
  }
  if (!X509_set_pubkey(x509.get(), private_key.get())) {
    return boost::make_tuple("", "");
  }
  X509_NAME *name = X509_get_subject_name(x509.get());
  if (name == nullptr) {
    return boost::make_tuple("", "");
  }

  if (!X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                  (const unsigned char *)"Proxy CA", -1, -1,
                                  0)) {
    return boost::make_tuple("", "");
  }
  if (!X509_set_issuer_name(x509.get(), name)) {
    return boost::make_tuple("", "");
  }

  X509V3_CTX ctx;
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, x509.get(), x509.get(), nullptr, nullptr, 0);
  if (!add_extension(ctx, x509.get(), NID_basic_constraints, "CA:TRUE") ||
      !add_extension(ctx, x509.get(), NID_key_usage, "keyCertSign") ||
      !add_extension(ctx, x509.get(), NID_subject_key_identifier, "hash") ||
      !add_extension(ctx, x509.get(), NID_authority_key_identifier,
                     "keyid:always")) {
    return boost::make_tuple("", "");
  }

  if (!X509_sign(x509.get(), private_key.get(), EVP_sha256())) {
    return boost::make_tuple("", "");
  }

  BIO_MEM_ptr bio(BIO_new(BIO_s_mem()), BIO_free);
  if (!bio) {
    return boost::make_tuple("", "");
  }
  if (!PEM_write_bio_X509(bio.get(), x509.get())) {
    return boost::make_tuple("", "");
  }
  BUF_MEM *mem = nullptr;
  if (!BIO_get_mem_ptr(bio.get(), &mem) || !mem || !mem->data || !mem->length) {
    return boost::make_tuple("", "");
  }
  std::string pem(mem->data, mem->length);

  BIO_MEM_ptr bio_private(BIO_new(BIO_s_mem()), BIO_free);
  if (!bio_private) {
    return boost::make_tuple("", "");
  }
  if (!PEM_write_bio_PrivateKey(bio_private.get(), private_key.get(), nullptr,
                                nullptr, 0, 0, nullptr)) {
    return boost::make_tuple("", "");
  }
  BUF_MEM *mem_private_key = nullptr;
  if (!BIO_get_mem_ptr(bio_private.get(), &mem_private_key) ||
      !mem_private_key || !mem_private_key->data || !mem_private_key->length) {
    return boost::make_tuple("", "");
  }
  std::string private_key_str(mem_private_key->data, mem_private_key->length);

  return boost::make_tuple(private_key_str, pem);
}

boost::tuple<std::string, std::string>
certificate_generator::generate_certificate(std::string host,
                                            std::string root_private_key,
                                            std::string root_cert,
                                            rsa_maker::reschedule reschedule) {
  BIO_MEM_ptr bio_root(BIO_new(BIO_s_mem()), BIO_free);
  if (!bio_root) {
    return boost::make_tuple("", "");
  }
  if (BIO_write(bio_root.get(), root_private_key.data(),
                root_private_key.length()) < 0) {
    return boost::make_tuple("", "");
  }

  EVP_PKEY *tmp = nullptr;
  if (PEM_read_bio_PrivateKey(bio_root.get(), &tmp, 0, 0) == nullptr) {
    return boost::make_tuple("", "");
  }
  EVP_PKEY_ptr evp_root_private_key(tmp, EVP_PKEY_free);

  BIO_MEM_ptr bio_root_cert(BIO_new(BIO_s_mem()), BIO_free);
  if (!bio_root_cert) {
    return boost::make_tuple("", "");
  }
  if (BIO_write(bio_root_cert.get(), root_cert.data(), root_cert.length()) <
      0) {
    return boost::make_tuple("", "");
  }

  X509 *issuer_tmp = nullptr;
  if (PEM_read_bio_X509(bio_root_cert.get(), &issuer_tmp, nullptr, nullptr) ==
      nullptr) {
    return boost::make_tuple("", "");
  }
  X509_ptr issuer(issuer_tmp, X509_free);

  EVP_PKEY_ptr private_key(EVP_PKEY_new(), EVP_PKEY_free);
  if (!private_key) {
    return boost::make_tuple("", "");
  }
  X509_ptr x509(X509_new(), X509_free);
  if (!x509.get()) {
    return boost::make_tuple("", "");
  }

  RSA_ptr rsa = rsa_maker_.get(std::forward<rsa_maker::reschedule>(reschedule));
  if (!rsa) {
    return boost::make_tuple("", "");
  }
  if (!EVP_PKEY_assign_RSA(private_key.get(),
                           rsa.get())) { // Transfers ownership of rsa
    return boost::make_tuple("", "");
  }
  (void)rsa.release();

  if (!X509_set_version(x509.get(), 2)) {
    return boost::make_tuple("", "");
  }

  if (X509_gmtime_adj(X509_get_notBefore(x509.get()), 0) == nullptr) {
    return boost::make_tuple("", "");
  }
  if (X509_gmtime_adj(X509_get_notAfter(x509.get()),
                      (long)60 * 60 * 24 * 365) == nullptr) {
    return boost::make_tuple("", "");
  }
  if (!X509_set_pubkey(x509.get(), private_key.get())) {
    return boost::make_tuple("", "");
  }

  X509_NAME *iss_name = X509_get_issuer_name(x509.get());
  if (iss_name == nullptr) {
    return boost::make_tuple("", "");
  }

  if (!X509_NAME_add_entry_by_txt(iss_name, "CN", MBSTRING_ASC,
                                  (const unsigned char *)"Proxy CA", -1, -1,
                                  0)) {
    return boost::make_tuple("", "");
  }

  if (!X509_set_issuer_name(x509.get(), iss_name)) {
    return boost::make_tuple("", "");
  }

  ASN1_IA5STRING_ptr ia5(ASN1_IA5STRING_new(), ASN1_IA5STRING_free);
  if (!ia5) {
    return boost::make_tuple("", "");
  }
  if (!ASN1_STRING_set(ia5.get(), host.data(), host.length())) {
    return boost::make_tuple("", "");
  }

  GENERAL_NAME_ptr gen_dns(GENERAL_NAME_new(), GENERAL_NAME_free);
  if (!gen_dns) {
    return boost::make_tuple("", "");
  }
  GENERAL_NAME_set0_value(gen_dns.get(), GEN_DNS, ia5.release());

  GENERAL_NAMES_ptr gens(sk_GENERAL_NAME_new_null(), GENERAL_NAMES_free);
  if (!gens) {
    return boost::make_tuple("", "");
  }
  sk_GENERAL_NAME_push(gens.get(), gen_dns.release());
  if (X509_add1_ext_i2d(x509.get(), NID_subject_alt_name, gens.get(), 1,
                        X509V3_ADD_DEFAULT) !=
      1) { // This returns -1 on error sometimes
    return boost::make_tuple("", "");
  }

  X509V3_CTX ctx;
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, issuer.get(), x509.get(), nullptr, nullptr, 0);
  if (!add_extension(ctx, x509.get(), NID_basic_constraints, "CA:FALSE") ||
      !add_extension(ctx, x509.get(), NID_subject_key_identifier, "hash") ||
      !add_extension(ctx, x509.get(), NID_authority_key_identifier,
                     "keyid:always")) {
    return boost::make_tuple("", "");
  }

  if (!X509_set_pubkey(x509.get(), private_key.get())) {
    return boost::make_tuple("", "");
  }

  if (!X509_sign(x509.get(), evp_root_private_key.get(), EVP_sha256())) {
    return boost::make_tuple("", "");
  }

  BIO_MEM_ptr bio(BIO_new(BIO_s_mem()), BIO_free);
  if (!bio) {
    return boost::make_tuple("", "");
  }
  if (!PEM_write_bio_X509(bio.get(), x509.get())) {
    return boost::make_tuple("", "");
  }
  BUF_MEM *mem = nullptr;
  if (!BIO_get_mem_ptr(bio.get(), &mem) || !mem || !mem->data || !mem->length) {
    return boost::make_tuple("", "");
  }
  std::string pem(mem->data, mem->length);

  BIO_MEM_ptr bio_private(BIO_new(BIO_s_mem()), BIO_free);
  if (!bio_private) {
    return boost::make_tuple("", "");
  }
  if (!PEM_write_bio_PrivateKey(bio_private.get(), private_key.get(), nullptr,
                                nullptr, 0, 0, nullptr)) {
    return boost::make_tuple("", "");
  }
  BUF_MEM *mem_private_key = nullptr;
  if (!BIO_get_mem_ptr(bio_private.get(), &mem_private_key) ||
      !mem_private_key || !mem_private_key->data || !mem_private_key->length) {
    return boost::make_tuple("", "");
  }
  std::string private_key_str(mem_private_key->data, mem_private_key->length);

  return boost::make_tuple(private_key_str, pem);
}

} // namespace cert
} // namespace proxy