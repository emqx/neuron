#include <stdlib.h>

#include <neuron.h>

#include "modbus_point.h"
#include "modbus_req.h"
#include "modbus_stack.h"

static neu_plugin_t *driver_open(neu_adapter_t *            adapter,
                                 const adapter_callbacks_t *callbacks);

static int driver_close(neu_plugin_t *plugin);
static int driver_init(neu_plugin_t *plugin);
static int driver_uninit(neu_plugin_t *plugin);
static int driver_start(neu_plugin_t *plugin);
static int driver_stop(neu_plugin_t *plugin);
static int driver_config(neu_plugin_t *plugin, neu_config_t *config);
static int driver_request(neu_plugin_t *plugin, neu_request_t *req);
static int driver_event_reply(neu_plugin_t *plugin, neu_event_reply_t *reply);

static int driver_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag);
static int driver_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group);
static int driver_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                        neu_value_u value);

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open        = driver_open,
    .close       = driver_close,
    .init        = driver_init,
    .uninit      = driver_uninit,
    .start       = driver_start,
    .stop        = driver_stop,
    .config      = driver_config,
    .request     = driver_request,
    .event_reply = driver_event_reply,

    .validate_tag = driver_validate_tag,

    .driver.validate_tag = driver_validate_tag,
    .driver.group_timer  = driver_group_timer,
    .driver.write_tag    = driver_write,
};

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "modbus-tcp",
    .module_descr = "modbus_tcp plugin",
    .intf_funs    = &plugin_intf_funs,
    .kind         = PLUGIN_KIND_SYSTEM,
    .type         = NEU_NA_TYPE_DRIVER,
};

static neu_plugin_t *driver_open(neu_adapter_t *            adapter,
                                 const adapter_callbacks_t *callbacks)
{
    neu_plugin_t *plugin = calloc(1, sizeof(neu_plugin_t));

    neu_plugin_common_init(&plugin->common);
    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;
    plugin->common.link_state        = NEU_PLUGIN_LINK_STATE_DISCONNECTED;

    return plugin;
}

static int driver_close(neu_plugin_t *plugin)
{
    free(plugin);

    return 0;
}

static int driver_init(neu_plugin_t *plugin)
{
    plugin->events = neu_event_new();
    plugin->stack  = modbus_stack_create((void *) plugin, modbus_send_msg,
                                        modbus_value_handle, modbus_write_resp);

    return 0;
}

static int driver_uninit(neu_plugin_t *plugin)
{
    if (plugin->stack) {
        modbus_stack_destroy(plugin->stack);
    }

    if (plugin->conn != NULL) {
        neu_conn_destory(plugin->conn);
    }

    neu_event_close(plugin->events);

    plog_info(plugin, "node: modbus uninit ok");

    return 0;
}

static int driver_start(neu_plugin_t *plugin)
{
    (void) plugin;

    return 0;
}

static int driver_stop(neu_plugin_t *plugin)
{
    (void) plugin;

    return 0;
}

static int driver_config(neu_plugin_t *plugin, neu_config_t *config)
{
    int              ret       = 0;
    char *           err_param = NULL;
    neu_json_elem_t  port      = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t  timeout   = { .name = "timeout", .t = NEU_JSON_INT };
    neu_json_elem_t  host      = { .name = "host", .t = NEU_JSON_STR };
    neu_conn_param_t param     = { 0 };

    ret = neu_parse_param(config->buf, &err_param, 3, &port, &host, &timeout);

    if (ret != 0) {
        plog_warn(plugin, "config: %s, decode error: %s", (char *) config->buf,
                  err_param);
        free(err_param);
        free(host.v.val_str);
        return -1;
    }

    param.type                      = NEU_CONN_TCP_CLIENT;
    param.params.tcp_client.ip      = host.v.val_str;
    param.params.tcp_client.port    = port.v.val_int;
    param.params.tcp_client.timeout = timeout.v.val_int;

    plog_info(plugin, "config: host: %s, port: %" PRId64 ", timeout: %" PRId64,
              host.v.val_str, port.v.val_int, timeout.v.val_int);

    plugin->common.link_state = NEU_PLUGIN_LINK_STATE_CONNECTING;
    if (plugin->conn != NULL) {
        plugin->conn = neu_conn_reconfig(plugin->conn, &param);
    } else {
        plugin->conn =
            neu_conn_new(&param, (void *) plugin, modbus_conn_connected,
                         modbus_conn_disconnected);
    }

    free(host.v.val_str);
    return 0;
}

static int driver_request(neu_plugin_t *plugin, neu_request_t *req)
{
    switch (req->req_type) {
    default:
        plog_warn(plugin, "unhandle msg type: %d", req->req_type);
        break;
    }

    return 0;
}

static int driver_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag)
{
    modbus_point_t point = { 0 };

    int ret = modbus_tag_to_point(tag, &point);
    if (ret == 0) {
        plog_debug(
            plugin,
            "validate tag success, name: %s, address: %s, type: %d, slave id: "
            "%d, start address: %d, n register: %d, area: %s",
            tag->name, tag->addr_str, tag->type,
            point.slave_id, point.start_address, point.n_register,
            modbus_area_to_str(point.area));
    }

    return ret;
}

static int driver_event_reply(neu_plugin_t *plugin, neu_event_reply_t *reply)
{
    (void) plugin;
    (void) reply;

    return 0;
}

static int driver_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group)
{
    return modbus_group_timer(plugin, group, 0xff);
}

static int driver_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                        neu_value_u value)
{
    return modbus_write(plugin, req, tag, value);
}