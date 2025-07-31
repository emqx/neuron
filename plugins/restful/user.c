#include <assert.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/rand.h>
#if !defined(OPENSSL_NO_DES) && !defined(OPENSSL_NO_DEPRECATED_3_0)
#include <openssl/des.h>
#endif
#include <openssl/sha.h>

#include "errcodes.h"
#include "user.h"
#include "utils/log.h"

static const unsigned char cov_2char[64] = {
    0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
    0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71,
    0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A
};

static const char ascii_dollar[] = { 0x24, 0x00 };

/**
 * @brief 使用SHA算法对密码进行加密
 *
 * 基于Ulrich Drepper描述的SHA密码算法实现，该算法用于密码存储
 *
 * @param passwd 需要加密的密码明文
 * @param magic 加密算法标识，目前支持'5'(SHA256)和'6'(SHA512)
 * @param salt 加密盐值，用于增强安全性
 * @return char* 返回加密后的密码字符串，失败时返回NULL
 */
static char *shacrypt(const char *passwd, const char *magic, const char *salt)
{
    /* Prefix for optional rounds specification.  */
    static const char rounds_prefix[] = "rounds=";
    /* Maximum salt string length.  */
#define SALT_LEN_MAX 16
    /* Default number of rounds if not explicitly specified.  */
#define ROUNDS_DEFAULT 5000
    /* Minimum number of rounds.  */
#define ROUNDS_MIN 1000
    /* Maximum number of rounds.  */
#define ROUNDS_MAX 999999999

    /* "$6$rounds=<N>$......salt......$...shahash(up to 86 chars)...\0" */
    size_t        out_len = 3 + 17 + 17 + 86 + 1;
    char *        out_buf = NULL;
    unsigned char buf[SHA512_DIGEST_LENGTH];
    unsigned char temp_buf[SHA512_DIGEST_LENGTH];
    size_t        buf_size = 0;
    char          ascii_magic[2];
    char          ascii_salt[17]; /* Max 16 chars plus '\0' */
    size_t        n;
    EVP_MD_CTX *  md = NULL, *md2 = NULL;
    const EVP_MD *sha = NULL;
    size_t        passwd_len, salt_len, magic_len;
    unsigned int  rounds        = ROUNDS_DEFAULT; /* Default */
    char          rounds_custom = 0;
    char *        p_bytes       = NULL;
    char *        s_bytes       = NULL;
    char *        cp            = NULL;

    passwd_len = strlen(passwd);
    magic_len  = strlen(magic);

    /* assert it's "5" or "6" */
    if (magic_len != 1)
        return NULL;

    switch (magic[0]) {
    case '5':
        sha      = EVP_sha256();
        buf_size = 32;
        break;
    case '6':
        sha      = EVP_sha512();
        buf_size = 64;
        break;
    default:
        return NULL;
    }

    if (strncmp(salt, rounds_prefix, sizeof(rounds_prefix) - 1) == 0) {
        const char *      num = salt + sizeof(rounds_prefix) - 1;
        char *            endp;
        unsigned long int srounds = strtoul(num, &endp, 10);
        if (*endp == '$') {
            salt = endp + 1;
            if (srounds > ROUNDS_MAX) {
                rounds = ROUNDS_MAX;
            } else if (srounds < ROUNDS_MIN) {
                rounds = ROUNDS_MIN;
            } else {
                rounds = (unsigned int) srounds;
            }
            rounds_custom = 1;
        } else {
            return NULL;
        }
    }

    if ((out_buf = OPENSSL_zalloc(out_len)) == NULL) {
        return NULL;
    }

    OPENSSL_strlcpy(ascii_magic, magic, sizeof(ascii_magic));

    /* The salt gets truncated to 16 chars */
    OPENSSL_strlcpy(ascii_salt, salt, sizeof(ascii_salt));
    salt_len = strlen(ascii_salt);

    out_buf[0] = 0;
    OPENSSL_strlcat(out_buf, ascii_dollar, out_len);
    OPENSSL_strlcat(out_buf, ascii_magic, out_len);
    OPENSSL_strlcat(out_buf, ascii_dollar, out_len);
    if (rounds_custom) {
        char tmp_buf[80]; /* "rounds=999999999" */
        sprintf(tmp_buf, "rounds=%u", rounds);
        OPENSSL_strlcat(out_buf, tmp_buf, out_len);
        OPENSSL_strlcat(out_buf, ascii_dollar, out_len);
    }
    OPENSSL_strlcat(out_buf, ascii_salt, out_len);

    /* assert "$5$rounds=999999999$......salt......" */
    if (strlen(out_buf) > 3 + 17 * rounds_custom + salt_len) {
        goto err;
    }

    md = EVP_MD_CTX_new();
    if (md == NULL || !EVP_DigestInit_ex(md, sha, NULL) ||
        !EVP_DigestUpdate(md, passwd, passwd_len) ||
        !EVP_DigestUpdate(md, ascii_salt, salt_len))
        goto err;

    md2 = EVP_MD_CTX_new();
    if (md2 == NULL || !EVP_DigestInit_ex(md2, sha, NULL) ||
        !EVP_DigestUpdate(md2, passwd, passwd_len) ||
        !EVP_DigestUpdate(md2, ascii_salt, salt_len) ||
        !EVP_DigestUpdate(md2, passwd, passwd_len) ||
        !EVP_DigestFinal_ex(md2, buf, NULL))
        goto err;

    for (n = passwd_len; n > buf_size; n -= buf_size) {
        if (!EVP_DigestUpdate(md, buf, buf_size))
            goto err;
    }
    if (!EVP_DigestUpdate(md, buf, n)) {
        goto err;
    }

    n = passwd_len;
    while (n) {
        if (!EVP_DigestUpdate(md,
                              (n & 1) ? buf : (const unsigned char *) passwd,
                              (n & 1) ? buf_size : passwd_len)) {
            goto err;
        }
        n >>= 1;
    }
    if (!EVP_DigestFinal_ex(md, buf, NULL)) {
        goto err;
    }

    /* P sequence */
    if (!EVP_DigestInit_ex(md2, sha, NULL)) {
        goto err;
    }

    for (n = passwd_len; n > 0; n--) {
        if (!EVP_DigestUpdate(md2, passwd, passwd_len)) {
            goto err;
        }
    }

    if (!EVP_DigestFinal_ex(md2, temp_buf, NULL)) {
        goto err;
    }

    if ((p_bytes = OPENSSL_zalloc(passwd_len)) == NULL) {
        goto err;
    }
    for (cp = p_bytes, n = passwd_len; n > buf_size;
         n -= buf_size, cp += buf_size) {
        memcpy(cp, temp_buf, buf_size);
    }
    memcpy(cp, temp_buf, n);

    /* S sequence */
    if (!EVP_DigestInit_ex(md2, sha, NULL)) {
        goto err;
    }

    for (n = 16 + buf[0]; n > 0; n--) {
        if (!EVP_DigestUpdate(md2, ascii_salt, salt_len)) {
            goto err;
        }
    }

    if (!EVP_DigestFinal_ex(md2, temp_buf, NULL)) {
        goto err;
    }

    if ((s_bytes = OPENSSL_zalloc(salt_len)) == NULL) {
        goto err;
    }
    for (cp = s_bytes, n = salt_len; n > buf_size;
         n -= buf_size, cp += buf_size) {
        memcpy(cp, temp_buf, buf_size);
    }
    memcpy(cp, temp_buf, n);

    for (n = 0; n < rounds; n++) {
        if (!EVP_DigestInit_ex(md2, sha, NULL)) {
            goto err;
        }
        if (!EVP_DigestUpdate(md2,
                              (n & 1) ? (const unsigned char *) p_bytes : buf,
                              (n & 1) ? passwd_len : buf_size)) {
            goto err;
        }
        if (n % 3) {
            if (!EVP_DigestUpdate(md2, s_bytes, salt_len)) {
                goto err;
            }
        }
        if (n % 7) {
            if (!EVP_DigestUpdate(md2, p_bytes, passwd_len)) {
                goto err;
            }
        }
        if (!EVP_DigestUpdate(md2,
                              (n & 1) ? buf : (const unsigned char *) p_bytes,
                              (n & 1) ? buf_size : passwd_len)) {
            goto err;
        }
        if (!EVP_DigestFinal_ex(md2, buf, NULL)) {
            goto err;
        }
    }
    EVP_MD_CTX_free(md2);
    EVP_MD_CTX_free(md);
    md2 = NULL;
    md  = NULL;
    OPENSSL_free(p_bytes);
    OPENSSL_free(s_bytes);
    p_bytes = NULL;
    s_bytes = NULL;

    cp    = out_buf + strlen(out_buf);
    *cp++ = ascii_dollar[0];

#define b64_from_24bit(B2, B1, B0, N)                       \
    do {                                                    \
        unsigned int w = ((B2) << 16) | ((B1) << 8) | (B0); \
        int          i = (N);                               \
        while (i-- > 0) {                                   \
            *cp++ = cov_2char[w & 0x3f];                    \
            w >>= 6;                                        \
        }                                                   \
    } while (0)

    switch (magic[0]) {
    case '5':
        b64_from_24bit(buf[0], buf[10], buf[20], 4);
        b64_from_24bit(buf[21], buf[1], buf[11], 4);
        b64_from_24bit(buf[12], buf[22], buf[2], 4);
        b64_from_24bit(buf[3], buf[13], buf[23], 4);
        b64_from_24bit(buf[24], buf[4], buf[14], 4);
        b64_from_24bit(buf[15], buf[25], buf[5], 4);
        b64_from_24bit(buf[6], buf[16], buf[26], 4);
        b64_from_24bit(buf[27], buf[7], buf[17], 4);
        b64_from_24bit(buf[18], buf[28], buf[8], 4);
        b64_from_24bit(buf[9], buf[19], buf[29], 4);
        b64_from_24bit(0, buf[31], buf[30], 3);
        break;
    case '6':
        b64_from_24bit(buf[0], buf[21], buf[42], 4);
        b64_from_24bit(buf[22], buf[43], buf[1], 4);
        b64_from_24bit(buf[44], buf[2], buf[23], 4);
        b64_from_24bit(buf[3], buf[24], buf[45], 4);
        b64_from_24bit(buf[25], buf[46], buf[4], 4);
        b64_from_24bit(buf[47], buf[5], buf[26], 4);
        b64_from_24bit(buf[6], buf[27], buf[48], 4);
        b64_from_24bit(buf[28], buf[49], buf[7], 4);
        b64_from_24bit(buf[50], buf[8], buf[29], 4);
        b64_from_24bit(buf[9], buf[30], buf[51], 4);
        b64_from_24bit(buf[31], buf[52], buf[10], 4);
        b64_from_24bit(buf[53], buf[11], buf[32], 4);
        b64_from_24bit(buf[12], buf[33], buf[54], 4);
        b64_from_24bit(buf[34], buf[55], buf[13], 4);
        b64_from_24bit(buf[56], buf[14], buf[35], 4);
        b64_from_24bit(buf[15], buf[36], buf[57], 4);
        b64_from_24bit(buf[37], buf[58], buf[16], 4);
        b64_from_24bit(buf[59], buf[17], buf[38], 4);
        b64_from_24bit(buf[18], buf[39], buf[60], 4);
        b64_from_24bit(buf[40], buf[61], buf[19], 4);
        b64_from_24bit(buf[62], buf[20], buf[41], 4);
        b64_from_24bit(0, 0, buf[63], 2);
        break;
    default:
        goto err;
    }
    *cp = '\0';

    return out_buf;

err:
    EVP_MD_CTX_free(md2);
    EVP_MD_CTX_free(md);
    OPENSSL_free(p_bytes);
    OPENSSL_free(s_bytes);
    OPENSSL_free(out_buf);
    return NULL;
}

/**
 * @brief 对密码进行哈希处理
 *
 * 使用SHA算法对密码进行哈希处理，如果未提供盐值则生成随机盐值
 *
 * @param passwd 需要哈希的密码明文
 * @param salt 可选的盐值，如果为NULL则生成新的随机盐值
 * @return char* 返回哈希后的密码字符串，失败时返回NULL
 */
// NOTE: if provided, require salt length of 16
static char *hash_password(const char *passwd, const char *salt)
{
    char *       hash    = NULL;
    char *       salt_p  = NULL;
    const size_t saltlen = 16;

    // first make sure we have a salt
    if (NULL == salt) {
        salt = salt_p = malloc(saltlen + 1);
        if (RAND_bytes((unsigned char *) salt_p, saltlen) <= 0) {
            goto end;
        }

        size_t i;
        for (i = 0; i < saltlen; i++) {
            salt_p[i] = cov_2char[salt_p[i] & 0x3f]; /* 6 bits */
        }
        salt_p[i] = 0;
    }

    assert(salt != NULL);

    hash = shacrypt(passwd, "5", salt);

end:
    free(salt_p);
    return hash;
}

/**
 * @brief 创建新用户
 *
 * 创建一个新用户并对密码进行哈希处理
 *
 * @param name 用户名
 * @param password 用户密码明文
 * @return neu_user_t* 返回创建的用户对象，失败时返回NULL
 */
neu_user_t *neu_user_new(const char *name, const char *password)
{
    neu_user_t *user = malloc(sizeof(*user));
    if (NULL == user) {
        return NULL;
    }

    user->name = strdup(name);
    user->hash = hash_password(password, NULL);

    if (NULL == user->name || NULL == user->hash) {
        free(user->name);
        free(user->hash);
        free(user);
        return NULL;
    }

    return user;
}

/**
 * @brief 释放用户对象
 *
 * 释放用户对象及其关联的内存资源
 *
 * @param user 需要释放的用户对象
 */
void neu_user_free(neu_user_t *user)
{
    if (NULL != user) {
        free(user->name);
        free(user->hash);
        free(user);
    }
}

/**
 * @brief 检查用户密码是否正确
 *
 * 验证提供的密码是否与用户存储的哈希值匹配
 *
 * @param user 用户对象
 * @param password 需要验证的密码明文
 * @return bool 密码正确返回true，错误返回false
 */
bool neu_user_check_password(neu_user_t *user, const char *password)
{
    char *salt = user->hash + 3;
    char *hash = hash_password(password, salt);

    bool pass = NULL != hash && 0 == strcmp(user->hash, hash);

    free(hash);
    return pass;
}

/**
 * @brief 从持久化存储加载用户信息
 *
 * 根据用户名从持久化存储中加载用户信息
 *
 * @param name 要加载的用户名
 * @return neu_user_t* 返回加载的用户对象，失败时返回NULL
 */
neu_user_t *neu_load_user(const char *name)
{
    neu_user_t *user = malloc(sizeof(*user));
    if (NULL == user) {
        return NULL;
    }

    neu_persist_user_info_t *info = NULL;
    if (0 != neu_persister_load_user(name, &info)) {
        free(user);
        return NULL;
    }

    user->name = info->name;
    user->hash = info->hash;

    free(info);
    return user;
}

/**
 * @brief 保存用户信息到持久化存储
 *
 * 将用户信息保存到持久化存储中
 *
 * @param user 要保存的用户对象
 * @return int 成功返回0，失败返回错误码
 */
int neu_save_user(neu_user_t *user)
{
    neu_persist_user_info_t info = {
        .name = user->name,
        .hash = user->hash,
    };

    return neu_persister_update_user(&info);
}

/**
 * @brief 更新用户密码
 *
 * 使用新密码更新用户的密码哈希
 *
 * @param user 用户对象
 * @param new_password 新密码明文
 * @return int 成功返回0，失败返回错误码
 */
int neu_user_update_password(neu_user_t *user, const char *new_password)
{
    char *salt = user->hash + 3;
    char *hash = hash_password(new_password, salt);
    if (NULL == hash) {
        nlog_error("hash_password fail");
        return NEU_ERR_EINTERNAL;
    }

    if (0 == strcmp(user->hash, hash)) {
        nlog_error("new password the same as old one");
        free(hash);
        return NEU_ERR_DUPLICATE_PASSWORD;
    }

    free(user->hash);
    user->hash = hash;
    return 0;
}
