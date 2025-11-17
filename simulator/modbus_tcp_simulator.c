#include <arpa/inet.h>
#include <jansson.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "neuron.h"
#include "connection/neu_connection.h"
#include "event/event.h"
#include "modbus/modbus.h"
#include "modbus_tcp_simulator.h"
#include "persist/persist.h"
#include "utils/log.h"
#include "utils/time.h"

#define SIM_DEFAULT_IP "0.0.0.0"
#define SIM_DEFAULT_PORT 1502
#define SIM_MAX_REGS 1000
#define SIM_MAX_BITS 1000

extern zlog_category_t *neuron;

typedef struct {
    bool            running;
    char            ip[64];
    uint16_t        port;
    neu_events_t   *events;
    neu_event_io_t *listen_io;
    neu_conn_t     *conn;
    pthread_t       gen_thread;
    atomic_bool     gen_stop;
    pthread_mutex_t lock;

    uint8_t  coil_bits[SIM_MAX_BITS];
    uint8_t  input_bits[SIM_MAX_BITS];
    uint16_t hold_regs[SIM_MAX_REGS];
    uint16_t input_regs[SIM_MAX_REGS];

    bool               readonly_mask[SIM_MAX_REGS];
    neu_sim_tag_type_e tag_type[SIM_MAX_REGS];
    char              *tag_name[SIM_MAX_REGS];
    char              *tag_addr[SIM_MAX_REGS];
    int                tag_count;
    int                last_error_code;
} sim_ctx_t;

static sim_ctx_t g_sim = { 0 };

struct client_event {
    neu_event_io_t *client;
    int             fd;
    void           *user;
};
static struct client_event g_clients[32] = { 0 };

static void connected(void *data, int fd)
{
    (void) data;
    (void) fd;
}
static void disconnected(void *data, int fd)
{
    (void) data;
    (void) fd;
}

static void sim_reset_regs_unlocked()
{
    memset(g_sim.hold_regs, 0, sizeof(g_sim.hold_regs));
    memset(g_sim.input_regs, 0, sizeof(g_sim.input_regs));
    memset(g_sim.coil_bits, 0, sizeof(g_sim.coil_bits));
    memset(g_sim.input_bits, 0, sizeof(g_sim.input_bits));
    memset(g_sim.readonly_mask, 0, sizeof(g_sim.readonly_mask));
    memset(g_sim.tag_type, 0, sizeof(g_sim.tag_type));
    for (uint16_t i = 0; i < SIM_MAX_REGS; ++i) {
        if (g_sim.tag_name[i]) {
            free(g_sim.tag_name[i]);
            g_sim.tag_name[i] = NULL;
        }
        if (g_sim.tag_addr[i]) {
            free(g_sim.tag_addr[i]);
            g_sim.tag_addr[i] = NULL;
        }
    }
    g_sim.tag_count = 0;
}

static void sim_mark_tag_unlocked(uint16_t addr, neu_sim_tag_type_e t)
{
    if (addr >= SIM_MAX_REGS) {
        return;
    }
    if (t == NEU_SIM_TAG_SINE) {
        if (addr + 1 < SIM_MAX_REGS) {
            g_sim.readonly_mask[addr]     = true;
            g_sim.readonly_mask[addr + 1] = true;
            g_sim.tag_type[addr]          = t;
            g_sim.tag_count++;
        }
    } else if (t == NEU_SIM_TAG_SAW || t == NEU_SIM_TAG_SQUARE ||
               t == NEU_SIM_TAG_RANDOM) {
        g_sim.readonly_mask[addr] = true;
        g_sim.tag_type[addr]      = t;
        g_sim.tag_count++;
    }
}

static inline int64_t now_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int prng_int(int seed, int min, int max)
{
    uint32_t x = (uint32_t) seed * 2654435761u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    int span = max - min + 1;
    return min + (int) (x % (uint32_t) (span > 0 ? span : 1));
}

static int pdu_read_holding(const uint8_t *pdu, int pdu_len,
                            struct modbus_code *rc, struct modbus_header *rh,
                            uint8_t *res, int res_mlen, int *res_len)
{
    if (pdu_len < (int) sizeof(struct modbus_address))
        return -1;
    struct modbus_address *addr  = (struct modbus_address *) pdu;
    uint16_t               start = ntohs(addr->start_address);
    uint16_t               nreg  = ntohs(addr->n_reg);
    if (nreg == 0 || (start + nreg) > SIM_MAX_REGS) {
        rc->function = MODBUS_READ_HOLD_REG_ERR;
        if (*res_len + 1 > res_mlen)
            return -1;
        res[*res_len] = 0x02;
        *res_len += 1;
        rh->len = htons(sizeof(struct modbus_code) + 1);
        return 0;
    }
    if (*res_len + 1 + 2 * nreg > res_mlen)
        return -1;
    uint8_t *res_data = res + *res_len;
    res_data[0]       = (uint8_t) (nreg * 2);
    pthread_mutex_lock(&g_sim.lock);
    for (uint16_t i = 0; i < nreg; ++i) {
        uint16_t v  = g_sim.hold_regs[start + i];
        uint16_t be = htons(v);
        memcpy(&res_data[1 + i * 2], &be, 2);
    }
    pthread_mutex_unlock(&g_sim.lock);
    *res_len += 1 + 2 * nreg;
    rh->len = htons(sizeof(struct modbus_code) + 1 + 2 * nreg);
    return 0;
}

static int pdu_read_input_regs(const uint8_t *pdu, int pdu_len,
                               struct modbus_code *rc, struct modbus_header *rh,
                               uint8_t *res, int res_mlen, int *res_len)
{
    if (pdu_len < (int) sizeof(struct modbus_address))
        return -1;
    struct modbus_address *addr  = (struct modbus_address *) pdu;
    uint16_t               start = ntohs(addr->start_address);
    uint16_t               nreg  = ntohs(addr->n_reg);
    if (nreg == 0 || (start + nreg) > SIM_MAX_REGS) {
        rc->function = MODBUS_READ_INPUT_REG_ERR;
        if (*res_len + 1 > res_mlen)
            return -1;
        res[*res_len] = 0x02;
        *res_len += 1;
        rh->len = htons(sizeof(struct modbus_code) + 1);
        return 0;
    }
    if (*res_len + 1 + 2 * nreg > res_mlen)
        return -1;
    uint8_t *res_data = res + *res_len;
    res_data[0]       = (uint8_t) (nreg * 2);
    pthread_mutex_lock(&g_sim.lock);
    for (uint16_t i = 0; i < nreg; ++i) {
        uint16_t v              = g_sim.input_regs[start + i];
        res_data[1 + i * 2]     = (uint8_t) (v >> 8);
        res_data[1 + i * 2 + 1] = (uint8_t) (v & 0xFF);
    }
    pthread_mutex_unlock(&g_sim.lock);
    *res_len += 1 + 2 * nreg;
    rh->len = htons(sizeof(struct modbus_code) + 1 + 2 * nreg);
    return 0;
}

static int pdu_read_coils(const uint8_t *pdu, int pdu_len,
                          struct modbus_code *rc, struct modbus_header *rh,
                          uint8_t *res, int res_mlen, int *res_len)
{
    if (pdu_len < (int) sizeof(struct modbus_address))
        return -1;
    struct modbus_address *addr  = (struct modbus_address *) pdu;
    uint16_t               start = ntohs(addr->start_address);
    uint16_t               nbits = ntohs(addr->n_reg);
    if (nbits == 0 || (start + nbits) > SIM_MAX_BITS) {
        rc->function = MODBUS_READ_COIL_ERR;
        if (*res_len + 1 > res_mlen)
            return -1;
        res[*res_len] = 0x02;
        *res_len += 1;
        rh->len = htons(sizeof(struct modbus_code) + 1);
        return 0;
    }
    uint16_t nbytes = (nbits + 7) / 8;
    if (*res_len + 1 + nbytes > res_mlen)
        return -1;
    uint8_t *res_data = res + *res_len;
    res_data[0]       = (uint8_t) nbytes;
    memset(&res_data[1], 0, nbytes);
    pthread_mutex_lock(&g_sim.lock);
    for (uint16_t i = 0; i < nbits; ++i) {
        if (g_sim.coil_bits[start + i]) {
            res_data[1 + (i / 8)] |= (uint8_t) (1u << (i % 8));
        }
    }
    pthread_mutex_unlock(&g_sim.lock);
    *res_len += 1 + nbytes;
    rh->len = htons(sizeof(struct modbus_code) + 1 + nbytes);
    return 0;
}

static int pdu_read_inputs(const uint8_t *pdu, int pdu_len,
                           struct modbus_code *rc, struct modbus_header *rh,
                           uint8_t *res, int res_mlen, int *res_len)
{
    if (pdu_len < (int) sizeof(struct modbus_address))
        return -1;
    struct modbus_address *addr  = (struct modbus_address *) pdu;
    uint16_t               start = ntohs(addr->start_address);
    uint16_t               nbits = ntohs(addr->n_reg);
    if (nbits == 0 || (start + nbits) > SIM_MAX_BITS) {
        rc->function = MODBUS_READ_INPUT_ERR;
        if (*res_len + 1 > res_mlen)
            return -1;
        res[*res_len] = 0x02;
        *res_len += 1;
        rh->len = htons(sizeof(struct modbus_code) + 1);
        return 0;
    }
    uint16_t nbytes = (nbits + 7) / 8;
    if (*res_len + 1 + nbytes > res_mlen)
        return -1;
    uint8_t *res_data = res + *res_len;
    res_data[0]       = (uint8_t) nbytes;
    memset(&res_data[1], 0, nbytes);
    pthread_mutex_lock(&g_sim.lock);
    for (uint16_t i = 0; i < nbits; ++i) {
        if (g_sim.input_bits[start + i]) {
            res_data[1 + (i / 8)] |= (uint8_t) (1u << (i % 8));
        }
    }
    pthread_mutex_unlock(&g_sim.lock);
    *res_len += 1 + nbytes;
    rh->len = htons(sizeof(struct modbus_code) + 1 + nbytes);
    return 0;
}

static int pdu_write_single_holding(const uint8_t *pdu, int pdu_len,
                                    struct modbus_code   *rc,
                                    struct modbus_header *rh, uint8_t *res,
                                    int res_mlen, int *res_len)
{
    if (pdu_len < (int) sizeof(struct modbus_address))
        return -1;
    struct modbus_address *addr  = (struct modbus_address *) pdu;
    uint16_t               start = ntohs(addr->start_address);
    if (start >= SIM_MAX_REGS) {
        rc->function = MODBUS_WRITE_S_HOLD_REG_ERR;
        if (*res_len + 1 > res_mlen)
            return -1;
        res[*res_len] = 0x02;
        *res_len += 1;
        rh->len = htons(sizeof(struct modbus_code) + 1);
        return 0;
    }
    pthread_mutex_lock(&g_sim.lock);
    bool is_readonly = g_sim.readonly_mask[start];
    if (!is_readonly) {
        const uint8_t *vbytes  = pdu + offsetof(struct modbus_address, n_reg);
        uint16_t       v       = (uint16_t) ((vbytes[0] << 8) | vbytes[1]);
        g_sim.hold_regs[start] = v;
    }
    pthread_mutex_unlock(&g_sim.lock);
    if (is_readonly) {
        rc->function = MODBUS_WRITE_S_HOLD_REG_ERR;
        if (*res_len + 1 > res_mlen)
            return -1;
        res[*res_len] = 0x03;
        *res_len += 1;
        rh->len = htons(sizeof(struct modbus_code) + 1);
        return 0;
    }
    if (*res_len + (int) sizeof(struct modbus_address) > res_mlen)
        return -1;
    memcpy(res + *res_len, pdu, sizeof(struct modbus_address));
    *res_len += sizeof(struct modbus_address);
    rh->len = htons(sizeof(struct modbus_code) + sizeof(struct modbus_address));
    return 0;
}

static int pdu_write_multiple_holding(const uint8_t *pdu, int pdu_len,
                                      struct modbus_code   *rc,
                                      struct modbus_header *rh, uint8_t *res,
                                      int res_mlen, int *res_len)
{
    if (pdu_len <
        (int) (sizeof(struct modbus_address) + sizeof(struct modbus_data)))
        return -1;
    struct modbus_address *addr = (struct modbus_address *) pdu;
    struct modbus_data    *data =
        (struct modbus_data *) (pdu + sizeof(struct modbus_address));
    uint16_t start = ntohs(addr->start_address);
    uint16_t nreg  = ntohs(addr->n_reg);
    if (nreg == 0 || (start + nreg) > SIM_MAX_REGS) {
        rc->function = MODBUS_WRITE_M_HOLD_REG_ERR;
        if (*res_len + 1 > res_mlen)
            return -1;
        res[*res_len] = 0x02;
        *res_len += 1;
        rh->len = htons(sizeof(struct modbus_code) + 1);
        return 0;
    }
    bool deny = false;
    pthread_mutex_lock(&g_sim.lock);
    for (uint16_t i = 0; i < nreg; ++i) {
        if (g_sim.readonly_mask[start + i]) {
            deny = true;
            break;
        }
    }
    if (!deny) {
        for (uint16_t i = 0; i < nreg; ++i) {
            const uint8_t *bp          = &data->byte[i * 2];
            uint16_t       v           = (uint16_t) ((bp[0] << 8) | bp[1]);
            g_sim.hold_regs[start + i] = v;
        }
    }
    pthread_mutex_unlock(&g_sim.lock);
    if (deny) {
        rc->function = MODBUS_WRITE_M_HOLD_REG_ERR;
        if (*res_len + 1 > res_mlen)
            return -1;
        res[*res_len] = 0x03;
        *res_len += 1;
        rh->len = htons(sizeof(struct modbus_code) + 1);
        return 0;
    }
    if (*res_len + (int) sizeof(struct modbus_address) > res_mlen)
        return -1;
    memcpy(res + *res_len, pdu, sizeof(struct modbus_address));
    *res_len += sizeof(struct modbus_address);
    rh->len = htons(sizeof(struct modbus_code) + sizeof(struct modbus_address));
    return 0;
}

static int pdu_write_single_coil(const uint8_t *pdu, int pdu_len,
                                 struct modbus_code   *rc,
                                 struct modbus_header *rh, uint8_t *res,
                                 int res_mlen, int *res_len)
{
    if (pdu_len < (int) sizeof(struct modbus_address))
        return -1;
    struct modbus_address *addr  = (struct modbus_address *) pdu;
    uint16_t               start = ntohs(addr->start_address);
    if (start >= SIM_MAX_BITS) {
        rc->function = MODBUS_WRITE_S_COIL_ERR;
        if (*res_len + 1 > res_mlen)
            return -1;
        res[*res_len] = 0x02;
        *res_len += 1;
        rh->len = htons(sizeof(struct modbus_code) + 1);
        return 0;
    }
    const uint8_t *vbytes = (const uint8_t *) &addr->n_reg;
    uint16_t       val    = ((uint16_t) vbytes[0] << 8) | vbytes[1];
    pthread_mutex_lock(&g_sim.lock);
    g_sim.coil_bits[start] = (val == 0xFF00) ? 1 : 0;
    pthread_mutex_unlock(&g_sim.lock);
    if (*res_len + (int) sizeof(struct modbus_address) > res_mlen)
        return -1;
    memcpy(res + *res_len, pdu, sizeof(struct modbus_address));
    *res_len += sizeof(struct modbus_address);
    rh->len = htons(sizeof(struct modbus_code) + sizeof(struct modbus_address));
    return 0;
}

static int pdu_write_multiple_coils(const uint8_t *pdu, int pdu_len,
                                    struct modbus_code   *rc,
                                    struct modbus_header *rh, uint8_t *res,
                                    int res_mlen, int *res_len)
{
    if (pdu_len <
        (int) (sizeof(struct modbus_address) + sizeof(struct modbus_data)))
        return -1;
    struct modbus_address *addr = (struct modbus_address *) pdu;
    struct modbus_data    *data =
        (struct modbus_data *) (pdu + sizeof(struct modbus_address));
    uint16_t start = ntohs(addr->start_address);
    uint16_t nbits = ntohs(addr->n_reg);
    if (nbits == 0 || (start + nbits) > SIM_MAX_BITS) {
        rc->function = MODBUS_WRITE_M_COIL_ERR;
        if (*res_len + 1 > res_mlen)
            return -1;
        res[*res_len] = 0x02;
        *res_len += 1;
        rh->len = htons(sizeof(struct modbus_code) + 1);
        return 0;
    }
    pthread_mutex_lock(&g_sim.lock);
    for (uint16_t i = 0; i < nbits; ++i) {
        uint8_t b                  = (data->byte[i / 8] >> (i % 8)) & 0x1;
        g_sim.coil_bits[start + i] = b ? 1 : 0;
    }
    pthread_mutex_unlock(&g_sim.lock);
    if (*res_len + (int) sizeof(struct modbus_address) > res_mlen)
        return -1;
    memcpy(res + *res_len, pdu, sizeof(struct modbus_address));
    *res_len += sizeof(struct modbus_address);
    rh->len = htons(sizeof(struct modbus_code) + sizeof(struct modbus_address));
    return 0;
}

static inline void sim_update_sine_hi_lo(uint16_t addr, int64_t tms)
{
    double T     = 60000.0;
    double theta = 2.0 * M_PI * fmod((double) tms, T) / T;
    double val   = -100.0 + (100.0 - (-100.0)) * (sin(theta) + 1.0) / 2.0;
    float  fval  = (float) val;
    union {
        float    f;
        uint32_t u;
    } fu;
    fu.f        = fval;
    uint16_t lo = (uint16_t) (fu.u & 0xFFFFu);
    uint16_t hi = (uint16_t) ((fu.u >> 16) & 0xFFFFu);
    if (addr + 1 < SIM_MAX_REGS) {
        g_sim.hold_regs[addr]     = hi;
        g_sim.hold_regs[addr + 1] = lo;
    }
}

static inline void sim_update_saw(uint16_t addr, int64_t tms)
{
    double T  = 100000.0;
    double ph = fmod((double) tms, T) / T;
    int    v  = (int) floor(100.0 * ph + 0.5);
    if (v < 0)
        v = 0;
    if (v > 100)
        v = 100;
    g_sim.hold_regs[addr] = (uint16_t) v;
}

static inline void sim_update_square(uint16_t addr, int64_t tms)
{
    int64_t period_ms     = 10000;
    int64_t phase         = tms % period_ms;
    int     v             = (phase < (period_ms / 2)) ? -10 : 10;
    g_sim.hold_regs[addr] = (uint16_t) (int16_t) v;
}

static inline void sim_update_random(uint16_t addr, int64_t tms)
{
    int sec               = (int) (tms / 1000);
    int v                 = prng_int(sec ^ addr, 0, 100);
    g_sim.hold_regs[addr] = (uint16_t) v;
}

static void *generator_thread(void *arg)
{
    (void) arg;
    while (!atomic_load(&g_sim.gen_stop)) {
        int64_t tms = now_ms();
        pthread_mutex_lock(&g_sim.lock);
        for (uint16_t addr = 0; addr < SIM_MAX_REGS; ++addr) {
            neu_sim_tag_type_e tp = g_sim.tag_type[addr];
            if (tp == NEU_SIM_TAG_NONE) {
                continue;
            }
            if (tp == NEU_SIM_TAG_SINE)
                sim_update_sine_hi_lo(addr, tms);
            else if (tp == NEU_SIM_TAG_SAW)
                sim_update_saw(addr, tms);
            else if (tp == NEU_SIM_TAG_SQUARE)
                sim_update_square(addr, tms);
            else if (tp == NEU_SIM_TAG_RANDOM)
                sim_update_random(addr, tms);
        }
        pthread_mutex_unlock(&g_sim.lock);
        usleep(1000 * 1000);
    }
    return NULL;
}

static int add_client(int fd, neu_event_io_t *client, void *user)
{
    for (size_t i = 0; i < sizeof(g_clients) / sizeof(g_clients[0]); ++i) {
        if (g_clients[i].fd == 0) {
            g_clients[i].fd     = fd;
            g_clients[i].client = client;
            g_clients[i].user   = user;
            return 0;
        }
    }
    return -1;
}

static neu_event_io_t *del_client(int fd)
{
    for (size_t i = 0; i < sizeof(g_clients) / sizeof(g_clients[0]); ++i) {
        if (g_clients[i].fd == fd) {
            g_clients[i].fd = 0;
            return g_clients[i].client;
        }
    }
    return NULL;
}

struct cycle_buf {
    uint16_t len;
    uint8_t  buf[4096];
};

static int process_modbus_tcp(uint8_t *req, uint16_t req_len, uint8_t *res,
                              int res_mlen, int *res_len)
{
    if (req_len < sizeof(struct modbus_header)) {
        return 0;
    }
    struct modbus_header *h    = (struct modbus_header *) req;
    uint16_t              need = sizeof(struct modbus_header) + ntohs(h->len);
    if (req_len < need) {
        return 0;
    }
    if (h->protocol != 0) {
        return -1;
    }

    memcpy(res, req, sizeof(struct modbus_header));
    struct modbus_header *rh = (struct modbus_header *) res;
    struct modbus_code   *c =
        (struct modbus_code *) (req + sizeof(struct modbus_header));
    struct modbus_code *rc =
        (struct modbus_code *) (res + sizeof(struct modbus_header));
    rc->slave_id = c->slave_id;
    rc->function = c->function;
    *res_len     = sizeof(struct modbus_header) + sizeof(struct modbus_code);
    rh->len      = htons(sizeof(struct modbus_code));

    uint8_t *pdu =
        req + sizeof(struct modbus_header) + sizeof(struct modbus_code);
    int pdu_len =
        need - (sizeof(struct modbus_header) + sizeof(struct modbus_code));

    switch (c->function) {
    case MODBUS_READ_HOLD_REG:
        pdu_read_holding(pdu, pdu_len, rc, rh, res, res_mlen, res_len);
        return need;
    case MODBUS_READ_INPUT_REG:
        pdu_read_input_regs(pdu, pdu_len, rc, rh, res, res_mlen, res_len);
        return need;
    case MODBUS_READ_COIL:
        pdu_read_coils(pdu, pdu_len, rc, rh, res, res_mlen, res_len);
        return need;
    case MODBUS_READ_INPUT:
        pdu_read_inputs(pdu, pdu_len, rc, rh, res, res_mlen, res_len);
        return need;
    case MODBUS_WRITE_S_HOLD_REG:
        pdu_write_single_holding(pdu, pdu_len, rc, rh, res, res_mlen, res_len);
        return need;
    case MODBUS_WRITE_M_HOLD_REG:
        pdu_write_multiple_holding(pdu, pdu_len, rc, rh, res, res_mlen,
                                   res_len);
        return need;
    case MODBUS_WRITE_S_COIL:
        pdu_write_single_coil(pdu, pdu_len, rc, rh, res, res_mlen, res_len);
        return need;
    case MODBUS_WRITE_M_COIL:
        pdu_write_multiple_coils(pdu, pdu_len, rc, rh, res, res_mlen, res_len);
        return need;
    default:
        break;
    }

    rc->function = c->function | 0x80;
    if (*res_len + 1 > res_mlen)
        return -1;
    res[*res_len] = 0x01;
    *res_len += 1;
    rh->len = htons(sizeof(struct modbus_code) + 1);
    return need;
}

static int client_io_cb(enum neu_event_io_type type, int fd, void *usr)
{
    switch (type) {
    case NEU_EVENT_IO_READ: {
        struct cycle_buf *cb = (struct cycle_buf *) usr;
        ssize_t r = neu_conn_tcp_server_recv(g_sim.conn, fd, cb->buf + cb->len,
                                             sizeof(cb->buf) - cb->len);
        if (r > 0) {
            cb->len += (uint16_t) r;
        }
        uint8_t res[2048] = { 0 };
        int     res_len   = 0;
        int     used =
            process_modbus_tcp(cb->buf, cb->len, res, sizeof(res), &res_len);
        if (used == -1) {
            neu_event_io_t *io = del_client(fd);
            if (io)
                neu_event_del_io(g_sim.events, io);
            free(usr);
            neu_conn_tcp_server_close_client(g_sim.conn, fd);
            return 0;
        } else if (used == 0) {
            return 0;
        } else {
            memmove(cb->buf, cb->buf + used, cb->len - used);
            cb->len -= used;
            neu_conn_tcp_server_send(g_sim.conn, fd, res, res_len);
        }
        break;
    }
    case NEU_EVENT_IO_CLOSED:
    case NEU_EVENT_IO_HUP: {
        neu_event_io_t *io = del_client(fd);
        if (io)
            neu_event_del_io(g_sim.events, io);
        free(usr);
        neu_conn_tcp_server_close_client(g_sim.conn, fd);
        break;
    }
    }
    return 0;
}

static int listen_io_cb(enum neu_event_io_type type, int lfd, void *usr)
{
    (void) usr;
    (void) lfd;
    if (type == NEU_EVENT_IO_READ) {
        int cfd = neu_conn_tcp_server_accept(g_sim.conn);
        if (cfd > 0) {
            struct cycle_buf    *buf = calloc(1, sizeof(struct cycle_buf));
            neu_event_io_param_t cio = {
                .fd       = cfd,
                .usr_data = buf,
                .cb       = client_io_cb,
            };
            neu_event_io_t *client = neu_event_add_io(g_sim.events, cio);
            add_client(cfd, client, buf);
            nlog_info("modbus simulator accepted client fd=%d", cfd);
        }
    }
    return 0;
}

static void server_start_listen(void *data, int fd)
{
    (void) data;
    neu_event_io_param_t io = { 0 };
    io.fd                   = fd;
    io.usr_data             = NULL;
    io.cb                   = listen_io_cb;
    g_sim.listen_io         = neu_event_add_io(g_sim.events, io);
    nlog_info("modbus simulator start listening on %s:%u", g_sim.ip,
              g_sim.port);
}

static void server_stop_listen(void *data, int fd)
{
    (void) data;
    (void) fd;
    if (g_sim.listen_io) {
        neu_event_del_io(g_sim.events, g_sim.listen_io);
        g_sim.listen_io = NULL;
    }
}

static char *sim_build_tags_json_unlocked(void)
{
    json_t *arr = json_array();
    for (uint16_t idx = 0; idx < SIM_MAX_REGS; ++idx) {
        neu_sim_tag_type_e tp = g_sim.tag_type[idx];
        if (tp == NEU_SIM_TAG_NONE) {
            continue;
        }
        json_t     *obj  = json_object();
        const char *name = g_sim.tag_name[idx] ? g_sim.tag_name[idx] : "";
        const char *addr = g_sim.tag_addr[idx] ? g_sim.tag_addr[idx] : "";
        json_object_set_new(obj, "name", json_string(name));
        json_object_set_new(obj, "address", json_string(addr));
        json_object_set_new(obj, "index", json_integer(idx));
        json_object_set_new(obj, "type", json_integer((int) tp));
        json_array_append_new(arr, obj);
        if (tp == NEU_SIM_TAG_SINE && idx + 1 < SIM_MAX_REGS) {
            ++idx; // skip the second register of float pair
        }
    }
    char *s = json_dumps(arr, 0);
    json_decref(arr);
    return s;
}

static void sim_persist_save(void)
{
    sqlite3 *db = neu_persister_get_db();
    if (!db) {
        return;
    }
    char *tags = NULL;
    pthread_mutex_lock(&g_sim.lock);
    tags        = sim_build_tags_json_unlocked();
    int enabled = g_sim.running ? 1 : 0;
    pthread_mutex_unlock(&g_sim.lock);

    sqlite3_stmt *stmt = NULL;
    const char   *sql  = "INSERT OR REPLACE INTO modbus_tcp_simulator (id, "
                         "enabled, tags_json, updated_at) "
                         "VALUES (1, ?, ?, CURRENT_TIMESTAMP)";
    if (SQLITE_OK == sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) {
        sqlite3_bind_int(stmt, 1, enabled);
        if (tags) {
            sqlite3_bind_text(stmt, 2, tags, -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_text(stmt, 2, "[]", -1, SQLITE_STATIC);
        }
        (void) sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    if (tags) {
        free(tags);
    }
}

static void sim_persist_load_and_apply(void)
{
    sqlite3 *db = neu_persister_get_db();
    if (!db) {
        return;
    }
    sqlite3_stmt *stmt = NULL;
    const char   *sql =
        "SELECT enabled, tags_json FROM modbus_tcp_simulator WHERE id=1";
    if (SQLITE_OK != sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) {
        return;
    }

    int step = sqlite3_step(stmt);
    if (step == SQLITE_ROW) {
        int         enabled  = sqlite3_column_int(stmt, 0);
        const char *tags_str = (const char *) sqlite3_column_text(stmt, 1);

        if (tags_str && tags_str[0]) {
            json_error_t jerr;
            json_t      *arr = json_loads(tags_str, 0, &jerr);
            if (arr && json_is_array(arr)) {
                size_t              count   = json_array_size(arr);
                char              **names   = NULL;
                char              **addrs   = NULL;
                uint16_t           *indices = NULL;
                neu_sim_tag_type_e *types   = NULL;
                if (count > 0) {
                    names   = calloc(count, sizeof(char *));
                    addrs   = calloc(count, sizeof(char *));
                    indices = calloc(count, sizeof(uint16_t));
                    types   = calloc(count, sizeof(neu_sim_tag_type_e));
                }
                size_t m = 0;
                for (size_t i = 0; i < count; ++i) {
                    json_t *obj = json_array_get(arr, i);
                    if (!obj || !json_is_object(obj))
                        continue;
                    json_t *jname = json_object_get(obj, "name");
                    json_t *jaddr = json_object_get(obj, "address");
                    json_t *jidx  = json_object_get(obj, "index");
                    json_t *jtype = json_object_get(obj, "type");
                    if (!jidx || !json_is_integer(jidx) || !jtype)
                        continue;
                    uint16_t idx = (uint16_t) json_integer_value(jidx);
                    int      tpv = json_is_integer(jtype)
                             ? (int) json_integer_value(jtype)
                             : 0;
                    if (idx >= SIM_MAX_REGS)
                        continue;
                    indices[m] = idx;
                    types[m]   = (neu_sim_tag_type_e) tpv;
                    names[m]   = jname && json_is_string(jname)
                          ? strdup(json_string_value(jname))
                          : strdup("");
                    addrs[m]   = jaddr && json_is_string(jaddr)
                          ? strdup(json_string_value(jaddr))
                          : strdup("");
                    ++m;
                }
                if (m > 0) {
                    neu_modbus_simulator_config_tags((const char **) names,
                                                     (const char **) addrs,
                                                     indices, types, (int) m);
                }
                for (size_t i = 0; i < m; ++i) {
                    free(names[i]);
                    free(addrs[i]);
                }
                free(names);
                free(addrs);
                free(indices);
                free(types);
                json_decref(arr);
            }
        }

        if (enabled) {
            neu_modbus_simulator_start(g_sim.ip, g_sim.port);
        }
    }

    sqlite3_finalize(stmt);
}

// cppcheck-suppress unusedFunction
void neu_modbus_simulator_apply_persist(void)
{
    sim_persist_load_and_apply();
}

// cppcheck-suppress unusedFunction
int neu_modbus_simulator_init(const char *config_dir)
{
    memset(&g_sim, 0, sizeof(g_sim));
    pthread_mutex_init(&g_sim.lock, NULL);
    memset(g_sim.ip, 0, sizeof(g_sim.ip));
    strncpy(g_sim.ip, SIM_DEFAULT_IP, sizeof(g_sim.ip) - 1);
    g_sim.port = SIM_DEFAULT_PORT;
    sim_reset_regs_unlocked();

    if (config_dir && *config_dir) {
        char path[256] = { 0 };
        snprintf(path, sizeof(path), "%s/%s", config_dir, "neuron.json");
        nlog_notice("modbus simulator read config: %s", path);
        FILE *fp = fopen(path, "rb");
        if (!fp) {
            char alt[256] = { 0 };
            snprintf(alt, sizeof(alt), "./config/neuron.json");
            nlog_warn("config not found at %s, trying %s", path, alt);
            fp = fopen(alt, "rb");
            if (!fp) {
                snprintf(alt, sizeof(alt), "./neuron.json");
                nlog_warn("config not found at ./config/neuron.json, trying %s",
                          alt);
                fp = fopen(alt, "rb");
            }
            if (fp) {
                nlog_notice("modbus simulator read config (fallback): %s", alt);
            }
        }
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long sz = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            char *buf = (char *) calloc(1, (size_t) sz + 1);
            if (buf && sz > 0) {
                fread(buf, 1, (size_t) sz, fp);
                json_error_t jerr;
                json_t      *root = json_loads(buf, 0, &jerr);
                if (root) {
                    json_t *sim = json_object_get(root, "modbus_simulator");
                    if (!sim) {
                        sim = json_object_get(root, "simulator");
                    }
                    if (sim && json_is_object(sim)) {
                        json_t *ip   = json_object_get(sim, "ip");
                        json_t *port = json_object_get(sim, "port");
                        if (ip && json_is_string(ip)) {
                            const char *cfg_ip = json_string_value(ip);
                            memset(g_sim.ip, 0, sizeof(g_sim.ip));
                            if (cfg_ip && cfg_ip[0] != '\0' &&
                                (is_ipv4(cfg_ip) || is_ipv6(cfg_ip))) {
                                strncpy(g_sim.ip, cfg_ip, sizeof(g_sim.ip) - 1);
                            } else {
                                strncpy(g_sim.ip, SIM_DEFAULT_IP,
                                        sizeof(g_sim.ip) - 1);
                            }
                        }
                        if (port && json_is_integer(port)) {
                            int p = (int) json_integer_value(port);
                            if (p > 0 && p < 65536)
                                g_sim.port = (uint16_t) p;
                        }
                        nlog_notice(
                            "modbus simulator config parsed (ip:%s port:%u)",
                            g_sim.ip, g_sim.port);
                    }
                    json_decref(root);
                }
                free(buf);
            }
            fclose(fp);
        }
    }
    return 0;
}

static void sim_do_stop(bool persist)
{
    if (!g_sim.running) {
        return;
    }
    atomic_store(&g_sim.gen_stop, true);
    if (g_sim.gen_thread) {
        pthread_join(g_sim.gen_thread, NULL);
        g_sim.gen_thread = 0;
    }
    if (g_sim.events && g_sim.listen_io) {
        neu_event_del_io(g_sim.events, g_sim.listen_io);
        g_sim.listen_io = NULL;
    }
    if (g_sim.conn) {
        neu_conn_destory(g_sim.conn);
        g_sim.conn = NULL;
    }
    if (g_sim.events) {
        neu_event_close(g_sim.events);
        g_sim.events = NULL;
    }
    g_sim.running = false;
    if (persist) {
        sim_persist_save();
    }
}

// cppcheck-suppress unusedFunction
void neu_modbus_simulator_fini(void)
{
    sim_do_stop(false);
    pthread_mutex_destroy(&g_sim.lock);
}

int neu_modbus_simulator_start(const char *ip, uint16_t port)
{
    if (g_sim.running) {
        return 0;
    }
    if (ip && *ip) {
        if ((void*)g_sim.ip != (void*)ip) {
             memset(g_sim.ip, 0, sizeof(g_sim.ip));
             strncpy(g_sim.ip, ip, sizeof(g_sim.ip) - 1);
        }
    } else {
        memset(g_sim.ip, 0, sizeof(g_sim.ip));
        strncpy(g_sim.ip, SIM_DEFAULT_IP, sizeof(g_sim.ip) - 1);
    }

    if (!(is_ipv4(g_sim.ip) || is_ipv6(g_sim.ip))) {
        memset(g_sim.ip, 0, sizeof(g_sim.ip));
        strncpy(g_sim.ip, SIM_DEFAULT_IP, sizeof(g_sim.ip) - 1);
    }
    if (port > 0) {
        g_sim.port = port;
    }
    g_sim.events           = neu_event_new("modbus_tcp_simulator");
    neu_conn_param_t param = {
        .log                            = neuron,
        .type                           = NEU_CONN_TCP_SERVER,
        .params.tcp_server.ip           = g_sim.ip,
        .params.tcp_server.port         = g_sim.port,
        .params.tcp_server.timeout      = 0,
        .params.tcp_server.max_link     = 16,
        .params.tcp_server.start_listen = server_start_listen,
        .params.tcp_server.stop_listen  = server_stop_listen,
    };
    g_sim.conn = neu_conn_new(&param, NULL, connected, disconnected);
    atomic_store(&g_sim.gen_stop, false);

    int lfd = neu_conn_fd(g_sim.conn);
    if (lfd <= 0) {
        nlog_error("modbus simulator listen %s:%u failed", g_sim.ip,
                   g_sim.port);
        g_sim.last_error_code = NEU_ERR_PORT_IN_USE;
        return -1;
    }
    if (pthread_create(&g_sim.gen_thread, NULL, generator_thread, NULL) != 0) {
        nlog_error("modbus simulator failed to start generator thread");
    }

    g_sim.last_error_code = 0;
    g_sim.running         = true;
    sim_persist_save();
    return 0;
}

// cppcheck-suppress unusedFunction
void neu_modbus_simulator_stop(void)
{
    sim_do_stop(true);
}

int neu_modbus_simulator_config_tags(const char **names, const char **addr_strs,
                                     const uint16_t           *addresses,
                                     const neu_sim_tag_type_e *types,
                                     int                       tag_count)
{
    pthread_mutex_lock(&g_sim.lock);
    sim_reset_regs_unlocked();
    if (addresses && types && tag_count > 0) {
        for (int i = 0; i < tag_count; ++i) {
            sim_mark_tag_unlocked(addresses[i], types[i]);
            if (names && names[i]) {
                g_sim.tag_name[addresses[i]] = strdup(names[i]);
            }
            if (addr_strs && addr_strs[i]) {
                g_sim.tag_addr[addresses[i]] = strdup(addr_strs[i]);
            }
        }
    }
    pthread_mutex_unlock(&g_sim.lock);
    sim_persist_save();
    return 0;
}

// cppcheck-suppress unusedFunction
void neu_modbus_simulator_get_status(neu_modbus_simulator_status_t *out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    out->running = g_sim.running;
    memset(out->ip, 0, sizeof(out->ip));
    strncpy(out->ip, g_sim.ip, sizeof(out->ip) - 1);
    out->port      = g_sim.port;
    out->tag_count = g_sim.tag_count;
    out->error     = g_sim.last_error_code;
}

// cppcheck-suppress unusedFunction
char *neu_modbus_simulator_export_drivers_json(void)
{
    json_t *root  = json_object();
    json_t *nodes = json_array();
    json_object_set_new(root, "nodes", nodes);

    json_t *node = json_object();
    json_array_append_new(nodes, node);
    json_object_set_new(node, "plugin", json_string("Modbus TCP"));
    json_object_set_new(node, "name", json_string("ModbusTCP_Simulator"));
    json_t     *params = json_object();
    const char *host = (strcmp(g_sim.ip, "0.0.0.0") == 0 || g_sim.ip[0] == '\0')
        ? "127.0.0.1"
        : g_sim.ip;
    json_object_set_new(params, "host", json_string(host));
    json_object_set_new(params, "port", json_integer(g_sim.port));
    json_object_set_new(params, "address_base", json_integer(1));
    json_object_set_new(params, "backup_port", json_integer(502));
    json_object_set_new(params, "check_header", json_integer(0));
    json_object_set_new(params, "connection_mode", json_integer(0));
    json_object_set_new(params, "degrade_cycle", json_integer(2));
    json_object_set_new(params, "degrade_time", json_integer(600));
    json_object_set_new(params, "device_degrade", json_integer(0));
    json_object_set_new(params, "endianess", json_integer(1));
    json_object_set_new(params, "interval", json_integer(20));
    json_object_set_new(params, "max_retries", json_integer(0));
    json_object_set_new(params, "retry_interval", json_integer(0));
    json_object_set_new(params, "timeout", json_integer(3000));
    json_object_set_new(params, "name", json_string("ModbusTCP_Simulator"));
    json_object_set_new(params, "plugin", json_string("Modbus TCP"));
    json_object_set_new(node, "params", params);

    json_t *groups = json_array();
    json_object_set_new(node, "groups", groups);
    json_t *group = json_object();
    json_array_append_new(groups, group);
    json_object_set_new(group, "group", json_string("group1"));
    json_object_set_new(group, "interval", json_integer(1000));
    json_t *tags = json_array();
    json_object_set_new(group, "tags", tags);

    pthread_mutex_lock(&g_sim.lock);
    for (uint16_t addr = 0; addr < SIM_MAX_REGS; ++addr) {
        neu_sim_tag_type_e tp = g_sim.tag_type[addr];
        if (tp == NEU_SIM_TAG_NONE)
            continue;
        const char *tname = NULL;
        int         nreg  = 1;
        int         type  = NEU_TYPE_INT16;
        if (tp == NEU_SIM_TAG_SINE) {
            tname = "sine";
            nreg  = 2;
            type  = NEU_TYPE_FLOAT;
        } else if (tp == NEU_SIM_TAG_SAW) {
            tname = "saw";
        } else if (tp == NEU_SIM_TAG_SQUARE) {
            tname = "square";
        } else if (tp == NEU_SIM_TAG_RANDOM) {
            tname = "random";
        }
        char        name[64] = { 0 };
        const char *use_name = g_sim.tag_name[addr];
        if (!use_name) {
            snprintf(name, sizeof(name), "sim_%s_%u", tname ? tname : "p",
                     addr);
            use_name = name;
        }
        char        addr_str[64] = { 0 };
        const char *use_addr     = g_sim.tag_addr[addr];
        if (!use_addr) {
            snprintf(addr_str, sizeof(addr_str), "1!4%u",
                     (unsigned) (addr + 1));
            use_addr = addr_str;
        }

        json_t *tag = json_object();
        json_object_set_new(tag, "name", json_string(use_name));
        json_object_set_new(tag, "address", json_string(use_addr));
        json_object_set_new(tag, "type", json_integer(type));
        json_object_set_new(tag, "attribute", json_integer(NEU_ATTRIBUTE_READ));
        json_object_set_new(tag, "precision", json_integer(0));
        json_object_set_new(tag, "decimal", json_real(0.0));
        json_object_set_new(tag, "bias", json_real(0.0));
        json_object_set_new(tag, "description", json_string(""));
        json_array_append_new(tags, tag);
        if (nreg == 2) {
            addr++;
        }
    }
    pthread_mutex_unlock(&g_sim.lock);

    char *out = json_dumps(root, JSON_REAL_PRECISION(6));
    json_decref(root);

    return out;
}

// cppcheck-suppress unusedFunction
char *neu_modbus_simulator_list_tags_json(void)
{
    json_t *root = json_object();
    json_t *arr  = json_array();
    json_object_set_new(root, "tags", arr);

    pthread_mutex_lock(&g_sim.lock);
    for (uint16_t addr = 0; addr < SIM_MAX_REGS; ++addr) {
        neu_sim_tag_type_e tp = g_sim.tag_type[addr];
        if (tp == NEU_SIM_TAG_NONE)
            continue;
        const char *name     = g_sim.tag_name[addr] ? g_sim.tag_name[addr] : "";
        const char *addr_str = g_sim.tag_addr[addr] ? g_sim.tag_addr[addr] : "";
        const char *tname    = "";
        switch (tp) {
        case NEU_SIM_TAG_SINE:
            tname = "sine";
            break;
        case NEU_SIM_TAG_SAW:
            tname = "saw";
            break;
        case NEU_SIM_TAG_SQUARE:
            tname = "square";
            break;
        case NEU_SIM_TAG_RANDOM:
            tname = "random";
            break;
        default:
            tname = "";
            break;
        }
        json_t *obj = json_object();
        json_object_set_new(obj, "type", json_string(tname));
        json_object_set_new(obj, "name", json_string(name));
        json_object_set_new(obj, "address", json_string(addr_str));
        json_array_append_new(arr, obj);
    }
    pthread_mutex_unlock(&g_sim.lock);

    char *out = json_dumps(root, 0);
    json_decref(root);
    return out;
}