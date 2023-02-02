/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#define NEURON_LOG_LABEL "license"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "errcodes.h"
#include "utils/asprintf.h"
#include "utils/log.h"

#include "license.h"

const char root_cert_pub_key[] = "-----BEGIN PUBLIC KEY-----\n\
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyJgH+BvEJIStYvyw1keQ\n\
/ixVPJ4GGjlP7lTKnZL3mqZyPXQUEaLnRmcQ3/ra8xYQPcfMReynqmrYmT45/eD2\n\
tK7rdXTzYfOWoU0cXNQMaQS7be1bLF4QrAEbJhEsgkjX9rP30mrzZCjRRnkQtWmi\n\
4DNBU4qOl6Ee9aAS5aY+L7DW646J47Tyc7gAA4sdZw04KGBXnSyXzyBvPaf+QByO\n\
rOXUxBcxehHN/Ox41/duYFXSR40U6lyp49NYJ6yEUVWSp4oxsrkcgqyegNKXdW1D\n\
8oqJXzxalbh/RB8YYlX+Ae377gEBlLefPFdSEYDRN/ajs3UIeqde6i20lVyDPIjE\n\
cQIDAQAB\n\
-----END PUBLIC KEY-----";

static const uint32_t mon_yday[2][12] = {
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 },
};

static int isleap(int year)
{
    return (year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0);
}

static uint64_t mktimestamp(struct tm dt)
{
    uint64_t ret;
    int      i = 0;

    ret = ((uint64_t) dt.tm_year - 1970) * 365 * 24 * 3600;
    ret +=
        (mon_yday[isleap(dt.tm_year)][dt.tm_mon] + dt.tm_mday - 1) * 24 * 3600;
    ret += dt.tm_hour * 3600 + dt.tm_min * 60 + dt.tm_sec;

    for (i = 1970; i < dt.tm_year; i++) {
        if (isleap(i)) {
            ret += 24 * 3600;
        }
    }
    if (ret > 4107715199) { // 2100-02-29 23:59:59
        ret += 24 * 3600;
    }
    return (ret);
}

static uint64_t get_asn1_ts(ASN1_TIME *time, char **ts_str)
{
    struct tm   t;
    const char *str = (const char *) time->data;

    memset(&t, 0, sizeof(t));

    if (time->type == V_ASN1_UTCTIME) { /* two digit year */
        sscanf(str, "%2d%2d%2d%2d%2d%2dZ", &t.tm_year, &t.tm_mon, &t.tm_mday,
               &t.tm_hour, &t.tm_min, &t.tm_sec);
        t.tm_year += 2000;
    } else if (time->type == V_ASN1_GENERALIZEDTIME) { /* four digit year */
        sscanf(str, "%4d%2d%2d%2d%2d%2dZ", &t.tm_year, &t.tm_mon, &t.tm_mday,
               &t.tm_hour, &t.tm_min, &t.tm_sec);
    }

    neu_asprintf(ts_str, "%4d-%02d-%02d %02d:%02d:%02d", t.tm_year, t.tm_mon,
                 t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

    t.tm_mon -= 1;

    /* Note: we did not adjust the time based on time zone information */
    return mktimestamp(t);
}

static int scan_bitvec(uint8_t vec[], size_t size, const char *hex)
{
    // 0xFFFFFFFFFFFFF
    // skip 0x
    char *tmp   = strdup(hex);
    char *start = tmp;
    int   index = 0;

    tmp += 2;
    if (strlen(tmp) % 2 == 1) {
        tmp -= 1;
        tmp[0] = '0';
    }

    for (int i = strlen(tmp) - 2; i >= 0; i -= 2) {
        uint8_t v = 0;

        if (sscanf(tmp + i, "%02hhX", &v) != 1) {
            free(start);
            return -1;
        }
        if ((size_t) index > size) {
            free(start);
            return -1;
        }
        vec[index] = v;
        index += 1;
    }

    free(start);
    return 0;
}

void print_bitvec(char *s, size_t size, uint8_t vec[], size_t n)
{
    size_t i = 0;
    for (size_t j = n; j-- > 0 && i < size;) {
        if (0 != j && 0 == vec[j]) {
            continue;
        }
        uint8_t h = (vec[j] & 0xF0) >> 4;
        uint8_t l = vec[j] & 0x0F;
        s[i++]    = (h < 10) ? ('0' + h) : ('A' + h - 10);
        if (i < size) {
            s[i++] = (l < 10) ? ('0' + l) : ('A' + l - 10);
        }
    }

    if (i < size) {
        s[i] = '\0';
    } else if (size > 0) {
        s[size - 1] = '\0';
    }
}

static const char *const g_license_type_str_map_[] = {
    [LICENSE_TYPE_TRIAL]  = "trial",
    [LICENSE_TYPE_RETAIL] = "retail",
    [LICENSE_TYPE_SME]    = "small-enterprise",
};

static const char *const g_plugin_bit_str_map[] = {
    [PLUGIN_BIT_MODBUS_PLUS_TCP] = "modbus-plus-tcp",
    [PLUGIN_BIT_MODBUS_RTU]      = "modbus-rtu",
    [PLUGIN_BIT_OPCUA]           = "opcua",
    [PLUGIN_BIT_S7COMM]          = "s7comm",
    [PLUGIN_BIT_FINS]            = "fins",
    [PLUGIN_BIT_QNA3E]           = "QnA3E",
    [PLUGIN_BIT_IEC60870_5_104]  = "IEC60870-5-104",
    [PLUGIN_BIT_KNX]             = "KNXnet/IP",
    [PLUGIN_BIT_NONA11]          = "NONA11",
    [PLUGIN_BIT_DLT645_2007]     = "DLT645-2007",
    [PLUGIN_BIT_BACNET]          = "BACnet/IP",
    [PLUGIN_BIT_MAX]             = NULL,
};

static license_type_e scan_licese_type(const char *s)
{
    license_type_e t = LICENSE_TYPE_MAX;
    for (int i = 0; i < LICENSE_TYPE_MAX; ++i) {
        if (0 == strcmp(s, g_license_type_str_map_[i])) {
            t = (license_type_e) i;
            break;
        }
    }

    return t;
}

static int extract_license(const char *license_fname, const char *pub_key,
                           license_t *license)
{
    BIO *            certbio = NULL;
    BIO *            keybio  = NULL;
    X509 *           cert    = NULL;
    X509_NAME *      subj    = NULL;
    X509_NAME *      issuer  = NULL;
    X509_NAME_ENTRY *entry   = NULL;
    EVP_PKEY *       pkey    = NULL;
    const STACK_OF(X509_EXTENSION) * ext_list;
    ASN1_OBJECT *        obj;
    ASN1_STRING *        str;
    ASN1_TIME *          not_before, *not_after;
    X509_EXTENSION *     ext;
    char                 buf[100];
    const unsigned char *value;
    int                  i;
    int                  rv = 0;

    OpenSSL_add_all_algorithms();

    struct stat statbuf;
    if (-1 == stat(license_fname, &statbuf)) {
        rv = NEU_ERR_LICENSE_NOT_FOUND;
        goto final;
    }

    /* load emqx certificate */
    certbio = BIO_new(BIO_s_file());
    if (0 == BIO_read_filename(certbio, license_fname)) {
        rv = NEU_ERR_LICENSE_INVALID;
        goto final;
    }
    if (!(cert = PEM_read_bio_X509(certbio, NULL, NULL, NULL))) {
        nlog_warn("loading certificate into memory");
        rv = NEU_ERR_LICENSE_INVALID;
        goto final;
    }

    /* load public key */
    keybio = BIO_new(BIO_s_mem());
    BIO_puts(keybio, pub_key);
    if (!(pkey = PEM_read_bio_PUBKEY(keybio, NULL, NULL, NULL))) {
        nlog_warn("loading pubkey into memory");
        rv = NEU_ERR_EINTERNAL;
        goto final;
    }

    /* Verify the certificates */
    if (X509_verify(cert, pkey) == 0) {
        nlog_warn("certification verification failure");
        rv = NEU_ERR_LICENSE_INVALID;
        goto final;
    }

    not_before      = X509_get_notBefore(cert);
    not_after       = X509_get_notAfter(cert);
    license->since_ = get_asn1_ts(not_before, &license->since_str_);
    license->until_ = get_asn1_ts(not_after, &license->until_str_);
    nlog_info("certificate expire date: %s", not_after->data);
    if (NULL == license->since_str_ || NULL == license->until_str_) {
        nlog_warn("cannot strdup");
        rv = NEU_ERR_EINTERNAL;
        goto final;
    }

    subj = X509_get_subject_name(cert);
    for (i = 0; i < X509_NAME_entry_count(subj); i++) {
        entry = X509_NAME_get_entry(subj, i);
        str   = X509_NAME_ENTRY_get_data(entry);
        value = ASN1_STRING_get0_data(str);
        nlog_info("certificate subject: %s", value);
    }

    issuer = X509_get_issuer_name(cert);
    for (i = 0; i < X509_NAME_entry_count(issuer); i++) {
        entry = X509_NAME_get_entry(issuer, i);
        str   = X509_NAME_ENTRY_get_data(entry);
        value = ASN1_STRING_get0_data(str);
        nlog_info("certificate issuer: %s", value);
    }

    /* extract the certificate's extensions */

    ext_list = X509_get0_extensions(cert);
    if (sk_X509_EXTENSION_num(ext_list) <= 0) {
        rv = NEU_ERR_LICENSE_INVALID;
        goto final;
    }

    unsigned mark = 0;
    for (i = 0; i < sk_X509_EXTENSION_num(ext_list); i++) {
        ext = sk_X509_EXTENSION_value(ext_list, i);
        obj = X509_EXTENSION_get_object(ext);
        str = X509_EXTENSION_get_data(ext);
        OBJ_obj2txt(buf, sizeof buf, obj, 1);
        value = ASN1_STRING_get0_data(str);
        nlog_info("certificate OID: %s => %s", buf, &value[2]);

        const char *val = (char *) &value[2];

        if (0 == strcmp(buf, LICENSE_KEY_TYPE)) {
            license->type_ = scan_licese_type(val);
            if (LICENSE_TYPE_MAX == license->type_) {
                nlog_error("invalid license type:`%s`", val);
                rv = NEU_ERR_LICENSE_INVALID;
            }
            mark |= 0x01;
        } else if (0 == strcmp(buf, LICENSE_KEY_MAX_NODES)) {
            license->max_nodes_ = strtoul(val, NULL, 0);
            mark |= 0x02;
        } else if (0 == strcmp(buf, LICENSE_KEY_MAX_NODE_TAGS)) {
            license->max_node_tags_ = strtoul(val, NULL, 0);
            mark |= 0x04;
        } else if (0 == strcmp(buf, LICENSE_KEY_PLUGIN_FLAG)) {
            if (0 !=
                scan_bitvec(license->plugin_flag_,
                            sizeof(license->plugin_flag_), val)) {
                nlog_error("fail to scan license plugin flag:`%s`", val);
                rv = NEU_ERR_LICENSE_INVALID;
            }
            mark |= 0x08;
        } else if (0 == strcmp(buf, LICENSE_KEY_TOKEN)) {
            license->token_ = strdup(val);
            mark |= 0x10;
        }
    }

    if (0x0F == (mark & 0x0F)) {
        nlog_info("certification verification success");
    } else {
        rv = NEU_ERR_LICENSE_INVALID;
        nlog_error("missing license key fields, mark:%u", mark);
    }

final:
    if (pkey)
        EVP_PKEY_free(pkey);
    if (cert)
        X509_free(cert);
    if (certbio)
        BIO_free_all(certbio);
    if (keybio)
        BIO_free_all(keybio);
    return rv;
}

const char *license_type_str(license_type_e t)
{
    return g_license_type_str_map_[t];
}

const char *plugin_bit_str(plugin_bit_e b)
{
    return g_plugin_bit_str_map[b];
}

void license_print(license_t *license)
{
    char flag[sizeof(license->plugin_flag_) * 2 + 1] = {};
    print_bitvec(flag, sizeof(flag), license->plugin_flag_,
                 sizeof(license->plugin_flag_));

    nlog_info("license `%s` for %s, from %s to %s, max_nodes:%" PRIu32
              ", max_node_tags:%" PRIu32 ", flag:0x%s",
              license->fname_, license_type_str(license->type_),
              license->since_str_, license->until_str_, license->max_nodes_,
              license->max_node_tags_, flag);
}

void license_init(license_t *license)
{
    // no associate license file
    // no plugins enabled
    // expired license period
    memset(license, 0, sizeof(*license));
    license->since_ = 1;
}

void license_fini(license_t *license)
{
    free(license->since_str_);
    free(license->until_str_);
    free(license->fname_);
    free(license->token_);
}

license_t *license_new()
{
    license_t *license = malloc(sizeof(*license));
    if (NULL == license) {
        return NULL;
    }

    license_init(license);

    return license;
}

void license_free(license_t *license)
{
    if (NULL == license) {
        return;
    }

    license_fini(license);
    free(license);
}

int license_read(license_t *license, const char *fname)
{
    if (NULL == license || NULL == fname) {
        return -1;
    }

    license_t lic;
    license_init(&lic);

    lic.fname_ = strdup(fname);
    if (NULL == lic.fname_) {
        return -1;
    }

    int rv = extract_license(fname, root_cert_pub_key, &lic);
    if (0 != rv) {
        license_fini(&lic);
        return rv;
    }

    license_print(&lic);

    free(license->since_str_);
    free(license->until_str_);
    free(license->fname_);
    free(license->token_);
    *license = lic;

    return rv;
}
