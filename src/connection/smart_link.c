#include <errno.h>
#include <fcntl.h>
#include <linux/serial.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include <jansson.h>

#include "json/json.h"

#include "connection/neu_smart_link.h"

#define DEVICE_CONF_PATH "/etc/config/device.conf"
#define SERIAL_CONF_PATH "/etc/config/serial.conf"
#define SMART_LINK_PATH "/dev/smartlink"

#define TYPE 'S'
#define UART_RS232 _IOWR(TYPE, 1, int)
#define UART_RS485 _IOWR(TYPE, 0, int)

enum SERIAL_MODE { SM_RS232, SM_RS422, SM_RS485 };

static int get_port(const char *path);
static int get_port_property(int port, int *mode, int *baudrate);
static int read_file(const char *path, char **buf);
static int set_serial_mode(const char *dev_path, int baudrate, int mode);

int neu_conn_smart_link_auto_set(const char *dev_path)
{
    int mode     = 0;
    int baudrate = 0;
    int port     = get_port(dev_path);
    if (port <= 0) {
        return -1;
    }
    printf("get port %d\n", port);

    int ret = get_port_property(port, &mode, &baudrate);
    if (ret != 0) {
        return -1;
    }

    printf("get port property %d %d\n", mode, baudrate);

    ret = set_serial_mode(dev_path, baudrate, mode);

    return ret;
}

static int read_file(const char *path, char **buf)
{
    FILE *fp = NULL;

    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    rewind(fp);

    *buf = calloc(1, size + 1);
    if (*buf == NULL) {
        fclose(fp);
        return -1;
    }

    int ret = fread(*buf, 1, size, fp);
    if (ret != size) {
        fclose(fp);
        free(*buf);
        return -1;
    }
    fclose(fp);

    return ret;
}

static int get_port(const char *dev_path)
{
    json_t *     root = NULL;
    json_error_t error;
    char *       buf = NULL;
    int          ret = read_file(DEVICE_CONF_PATH, &buf);
    if (ret <= 0) {
        return -1;
    }

    root = json_loads(buf, JSON_DECODE_ANY, &error);
    if (root == NULL) {
        free(buf);
        return -1;
    }

    neu_json_elem_t e_tty = {
        .name = "tty_name",
        .t    = NEU_JSON_OBJECT,
    };

    ret = neu_json_decode_value(root, &e_tty);
    if (ret != 0 || json_is_array((json_t *) e_tty.v.val_object) == false) {
        free(buf);
        json_decref(root);
        return -1;
    }

    int     index = 0;
    json_t *e_dev = NULL;
    json_array_foreach(e_tty.v.val_object, index, e_dev)
    {
        neu_json_elem_t e_dd = {
            .name = "device",
            .t    = NEU_JSON_STR,
        };

        ret = neu_json_decode_value(e_dev, &e_dd);
        if (ret != 0) {
            free(buf);
            json_decref(root);
            return -1;
        }

        if (strcmp(e_dd.v.val_str, dev_path) == 0) {
            neu_json_elem_t e_port = {
                .name = "port",
                .t    = NEU_JSON_INT,
            };

            ret = neu_json_decode_value(e_dev, &e_port);
            if (ret != 0) {
                free(buf);
                json_decref(root);
                return -1;
            }

            free(buf);
            json_decref(root);
            free(e_dd.v.val_str);
            return e_port.v.val_int;
        }
        free(e_dd.v.val_str);
    }

    free(buf);
    json_decref(root);
    return -1;
}

static int get_port_property(int port, int *mode, int *baudrate)
{
    json_t *     root = NULL;
    json_error_t error;
    char *       buf = NULL;
    int          ret = read_file(SERIAL_CONF_PATH, &buf);
    if (ret <= 0) {
        return -1;
    }

    root = json_loads(buf, JSON_DECODE_ANY, &error);
    if (root == NULL) {
        free(buf);
        return -1;
    }

    json_t *e_port = json_array_get(root, port - 1);
    if (e_port == NULL) {
        json_decref(root);
        free(buf);
        return -1;
    }

    neu_json_elem_t elems[] = {
        {
            .name = "mode",
            .t    = NEU_JSON_INT,
        },
        {
            .name = "baudrate",
            .t    = NEU_JSON_INT,
        },
    };

    ret = neu_json_decode_by_json(e_port, NEU_JSON_ELEM_SIZE(elems), elems);
    if (ret != 0) {
        free(buf);
        json_decref(root);
        return -1;
    }

    *mode     = elems[0].v.val_int;
    *baudrate = elems[1].v.val_int;

    json_decref(root);
    free(buf);
    return 0;
}

static int set_serial_mode(const char *dev_path, int baudrate, int mode)
{
    static pthread_mutex_t mode_switch_mtx = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mode_switch_mtx);

    int serial_fd, switch_fd;
    int tty_num, para;
    int ret = -1;

    struct serial_rs485 conf;
    conf.flags &= !SER_RS485_ENABLED;

    switch (baudrate) {
    case 38400:
        conf.delay_rts_after_send = 1;
        break;
    case 57600:
        conf.delay_rts_after_send = 4;
        break;
    case 115200:
        conf.delay_rts_after_send = 6;
        break;
    default:
        conf.delay_rts_after_send = 0;
        break;
    }

    if ((serial_fd = open(dev_path, O_RDWR | O_NOCTTY)) < 0) {
        fprintf(stderr, "open %s failed, fd: %d, strerr: %s\n", dev_path,
                serial_fd, strerror(errno));
        goto out;
    }

    if (mode != SM_RS232) {
        if (ioctl(serial_fd, TIOCMGET, &para) != 0) {
            fprintf(stderr, "ioctl failed, strerr: %s\n", strerror(errno));
            goto out2;
        }
        if (mode == SM_RS422) {
            para |= TIOCM_DTR;
            conf.flags |= SER_RS485_ENABLED | SER_RS485_RTS_ON_SEND;
        } else if (mode == SM_RS485) {
            para &= ~TIOCM_DTR;
            conf.flags |= SER_RS485_ENABLED | SER_RS485_RTS_ON_SEND;
        }
        if (ioctl(serial_fd, TIOCMSET, &para) != 0) {
            fprintf(stderr, "ioctl failed, strerr: %s\n", strerror(errno));
            goto out2;
        }
    }
    ioctl(serial_fd, TIOCSRS485, &conf);

    if ((switch_fd = open(SMART_LINK_PATH, O_RDWR)) < 0) {
        fprintf(stderr, "open %s failed, ret: %d, strerr: %s\n",
                SMART_LINK_PATH, switch_fd, strerror(errno));
        goto out2;
    }

    sscanf(dev_path, "%*[a-zA-Z/]%d", &tty_num);
    printf("tty_num: %d\n", tty_num);
    if (mode == SM_RS232) {
        ret = ioctl(switch_fd, UART_RS232, tty_num);
    } else {
        ret = ioctl(switch_fd, UART_RS485, tty_num);
    }
    if (ret < 0) {
        fprintf(stderr, "ioctl failed, ret: %d, strerr: %s\n", ret,
                strerror(errno));
    }

    close(switch_fd);
    ret = serial_fd;
    pthread_mutex_unlock(&mode_switch_mtx);
    return ret;
out2:
    close(serial_fd);
    serial_fd = -1;
out:
    pthread_mutex_unlock(&mode_switch_mtx);
    return ret;
}
