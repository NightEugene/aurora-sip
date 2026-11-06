#ifndef AURORA_TLS_IDENTITY_H
#define AURORA_TLS_IDENTITY_H
#include <limits.h>
#include <string.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

/* Identity only. PJSIP must still verify the chain, validity and purpose.
 * OpenSSL enforces SAN precedence, one-label wildcards and exact IP SANs. */
static int aurora_tls_identity_matches(const char *pem, size_t pem_size,
                                       const char *host, size_t host_size)
{
    BIO *input;
    X509 *cert;
    ASN1_OCTET_STRING *ip;
    char name[256];
    int matches;
    if (!pem || !pem_size || pem_size > INT_MAX || !host || !host_size ||
        host_size >= sizeof(name) || memchr(host, '\0', host_size))
        return 0;
    memcpy(name, host, host_size);
    name[host_size] = '\0';
    input = BIO_new_mem_buf(pem, (int)pem_size);
    if (!input) return 0;
    cert = PEM_read_bio_X509(input, NULL, NULL, NULL);
    BIO_free(input);
    if (!cert) return 0;
    ip = a2i_IPADDRESS(name);
    if (ip) {
        matches = X509_check_ip_asc(cert, name, 0);
        ASN1_OCTET_STRING_free(ip);
    } else {
        matches = X509_check_host(cert, name, host_size,
                                 X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS, NULL);
    }
    X509_free(cert);
    return matches == 1;
}
#endif
