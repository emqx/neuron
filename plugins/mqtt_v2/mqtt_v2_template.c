/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
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

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <jansson.h>

#include "msg.h"
#include "json/json.h"

#include "mqtt_v2_template.h"

#define TMPL_MAX_DEPTH 32
#define TMPL_TIME_FORMAT_MAX_LEN 128

typedef enum {
    VAR_APP,
    VAR_DEVICE,
    VAR_GROUP,
    VAR_SUB_TOPIC,
    VAR_TIMESTAMP,
    VAR_TIME_RFC3339,
    VAR_TIME_UTC,
    VAR_TIME_LOCAL,
    VAR_TIME_UNIX,
    VAR_TIME_FORMAT,
    VAR_PROPERTIES,
    VAR_PROPERTY,
    VAR_TAGS,
    VAR_TAGS_KV,
    VAR_TAGS_ITEM,
    VAR_TAG,
} var_kind_e;

typedef enum {
    TAG_ATTR_NONE,
    TAG_ATTR_NAME,
    TAG_ATTR_VALUE,
    TAG_ATTR_TIMESTAMP,
    TAG_ATTR_SOURCE_TIMESTAMP,
    TAG_ATTR_SERVER_TIMESTAMP,
    TAG_ATTR_QUALITY,
    TAG_ATTR_DESCRIPTION,
    TAG_ATTR_UNIT,
    TAG_ATTR_ERROR,
    TAG_ATTR_TYPE,
} tag_attr_e;

// which clock a TIME_* function formats. GROUP is the message timestamp.
typedef enum {
    TIME_SRC_GROUP = 0,
    TIME_SRC_TAG,
    TIME_SRC_NAMED_TAG,
} time_src_e;

typedef struct {
    var_kind_e kind;
    tag_attr_e attr;
    time_src_e time_src;
    char *     arg;      // tag name, property key or strftime format
    char *     time_ref; // tag name for TIME_*(tags[name].timestamp)
} var_t;

typedef struct {
    bool  is_var;
    char *text;
    var_t var;
} seg_t;

typedef struct {
    size_t n_seg;
    seg_t *segs;
} str_tmpl_t;

typedef enum {
    NODE_LITERAL,
    NODE_VAR,
    NODE_STR,
    NODE_OBJECT,
    NODE_ARRAY,
} node_kind_e;

typedef struct node node_t;

typedef struct {
    str_tmpl_t key;
    node_t *   value;
} field_t;

struct node {
    node_kind_e kind;
    json_t *    literal;
    var_t       var;
    str_tmpl_t  str;
    size_t      n_children;
    field_t *   fields;
    node_t **   items;
};

struct mqtt_v2_tmpl {
    mqtt_v2_tmpl_kind_e kind;
    // the tag template uses ${tag.error}, so tags that fail to be read are
    // reported. A data template never sets this.
    bool    reports_error;
    node_t *root;
};

typedef struct {
    mqtt_v2_tmpl_kind_e kind;
    char *              err;
    size_t              err_size;
} compiler_t;

typedef struct {
    const mqtt_v2_render_ctx_t *ctx;
    neu_resp_tag_value_meta_t * tag;
    neu_json_read_resp_tag_t    jtag;
} scope_t;

typedef struct {
    char * data;
    size_t len;
    size_t cap;
} strbuf_t;

static const struct {
    const char *name;
    tag_attr_e  attr;
} tag_attrs[] = {
    { "name", TAG_ATTR_NAME },
    { "value", TAG_ATTR_VALUE },
    { "timestamp", TAG_ATTR_TIMESTAMP },
    { "sourceTimestamp", TAG_ATTR_SOURCE_TIMESTAMP },
    { "serverTimestamp", TAG_ATTR_SERVER_TIMESTAMP },
    { "quality", TAG_ATTR_QUALITY },
    { "description", TAG_ATTR_DESCRIPTION },
    { "unit", TAG_ATTR_UNIT },
    { "error", TAG_ATTR_ERROR },
    { "type", TAG_ATTR_TYPE },
};

static const struct {
    const char *name;
    var_kind_e  kind;
} plain_vars[] = {
    { "app", VAR_APP },
    { "device", VAR_DEVICE },
    { "group", VAR_GROUP },
    { "sub_topic", VAR_SUB_TOPIC },
    { "timestamp", VAR_TIMESTAMP },
    { "TIME_RFC3339", VAR_TIME_RFC3339 },
    { "TIME_UTC", VAR_TIME_UTC },
    { "TIME_LOCAL", VAR_TIME_LOCAL },
    { "TIME_UNIX", VAR_TIME_UNIX },
    { "properties", VAR_PROPERTIES },
    { "tags", VAR_TAGS },
    { "tags_kv", VAR_TAGS_KV },
};

// drivers name tag metadata differently
static const char *quality_metas[]   = { "quality", "q", NULL };
static const char *timestamp_metas[] = { "timestamp", "sourcetimestamp", "time",
                                         "t",         "servertimestamp", NULL };
static const char *source_timestamp_metas[] = { "sourceTimestamp",
                                                "sourcetimestamp",
                                                "source_timestamp", NULL };
static const char *server_timestamp_metas[] = { "serverTimestamp",
                                                "servertimestamp",
                                                "server_timestamp", NULL };

static json_t *render_node(const node_t *node, scope_t *s);

static int strbuf_append(strbuf_t *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (cap < b->len + n + 1) {
            cap *= 2;
        }
        char *data = realloc(b->data, cap);
        if (NULL == data) {
            return -1;
        }
        b->data = data;
        b->cap  = cap;
    }

    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static char *strbuf_take(const strbuf_t *b)
{
    if (NULL == b->data) {
        return strdup("");
    }
    return b->data;
}

static void compile_error(compiler_t *c, const char *fmt, const char *arg)
{
    if (NULL != c->err && 0 < c->err_size) {
        snprintf(c->err, c->err_size, fmt, arg);
    }
}

static void var_fini(var_t *var)
{
    free(var->arg);
    free(var->time_ref);
    var->arg      = NULL;
    var->time_ref = NULL;
}

static void str_fini(str_tmpl_t *str)
{
    for (size_t i = 0; i < str->n_seg; ++i) {
        free(str->segs[i].text);
        var_fini(&str->segs[i].var);
    }
    free(str->segs);
    str->segs  = NULL;
    str->n_seg = 0;
}

static void node_free(node_t *node)
{
    if (NULL == node) {
        return;
    }

    switch (node->kind) {
    case NODE_LITERAL:
        json_decref(node->literal);
        break;
    case NODE_VAR:
        var_fini(&node->var);
        break;
    case NODE_STR:
        str_fini(&node->str);
        break;
    case NODE_OBJECT:
        for (size_t i = 0; i < node->n_children; ++i) {
            str_fini(&node->fields[i].key);
            node_free(node->fields[i].value);
        }
        free(node->fields);
        break;
    case NODE_ARRAY:
        for (size_t i = 0; i < node->n_children; ++i) {
            node_free(node->items[i]);
        }
        free(node->items);
        break;
    }

    free(node);
}

static bool parse_tag_attr(const char *name, tag_attr_e *attr)
{
    for (size_t i = 0; i < sizeof(tag_attrs) / sizeof(tag_attrs[0]); ++i) {
        if (0 ==
            strncmp(name, tag_attrs[i].name, strlen(tag_attrs[i].name) + 1)) {
            *attr = tag_attrs[i].attr;
            return true;
        }
    }
    return false;
}

static bool timestamp_attr(tag_attr_e attr)
{
    return TAG_ATTR_TIMESTAMP == attr || TAG_ATTR_SOURCE_TIMESTAMP == attr ||
        TAG_ATTR_SERVER_TIMESTAMP == attr;
}

static void trim_spaces(char *text)
{
    char * start = text;
    size_t len   = 0;

    while (isspace((unsigned char) *start)) {
        ++start;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }
    len = strlen(text);
    while (0 < len && isspace((unsigned char) text[len - 1])) {
        text[--len] = '\0';
    }
}

static bool time_function(const char *name, var_kind_e *kind)
{
    static const struct {
        const char *name;
        var_kind_e  kind;
    } fns[] = {
        { "TIME_RFC3339", VAR_TIME_RFC3339 },
        { "TIME_UTC", VAR_TIME_UTC },
        { "TIME_LOCAL", VAR_TIME_LOCAL },
        { "TIME_UNIX", VAR_TIME_UNIX },
    };

    for (size_t i = 0; i < sizeof(fns) / sizeof(fns[0]); ++i) {
        if (0 == strncmp(name, fns[i].name, strlen(fns[i].name) + 1)) {
            *kind = fns[i].kind;
            return true;
        }
    }
    return false;
}

// expr is one of timestamp, tag.<clock> or tags[name].<clock>
static int parse_time_expr(compiler_t *c, const char *origin, char *expr,
                           var_t *var)
{
    trim_spaces(expr);
    if ('\0' == expr[0]) {
        compile_error(c, "empty time argument `${%s}`", origin);
        return -1;
    }
    if (0 == strncmp(expr, "timestamp", strlen("timestamp") + 1)) {
        var->time_src = TIME_SRC_GROUP;
        return 0;
    }
    if (0 == strncmp(expr, "tag.", strlen("tag."))) {
        if (!parse_tag_attr(expr + strlen("tag."), &var->attr) ||
            !timestamp_attr(var->attr)) {
            compile_error(c, "`${%s}` is not a timestamp", origin);
            return -1;
        }
        var->time_src = TIME_SRC_TAG;
        return 0;
    }
    if (0 == strncmp(expr, "tags[", strlen("tags["))) {
        const char *close = strrchr(expr, ']');

        if (NULL == close || close == expr + strlen("tags[") ||
            '.' != close[1] || !parse_tag_attr(close + 2, &var->attr) ||
            !timestamp_attr(var->attr)) {
            compile_error(c, "`${%s}` is not a timestamp", origin);
            return -1;
        }
        var->time_src = TIME_SRC_NAMED_TAG;
        var->time_ref = strndup(expr + strlen("tags["), close - (expr + 5));
        return NULL != var->time_ref ? 0 : -1;
    }

    compile_error(c, "unknown time argument `${%s}`", origin);
    return -1;
}

// TIME_LOCAL(tag.timestamp), TIME:%Y-%m-%d(timestamp), and the same for the
// other clocks. `name` is restored before returning.
static int parse_time_call(compiler_t *c, char *name, var_t *var)
{
    char * origin = strdup(name);
    char * open   = strchr(name, '(');
    size_t len    = strlen(name);
    bool   split  = false;
    int    rv     = -1;

    if (NULL == origin) {
        return -1;
    }
    if (NULL == open || ')' != name[len - 1] || NULL != strchr(open + 1, '(')) {
        compile_error(c, "invalid time function `${%s}`", origin);
        goto done;
    }

    *open         = '\0';
    name[len - 1] = '\0';
    split         = true;

    if (0 == strncmp(name, "TIME:", strlen("TIME:"))) {
        size_t flen = strlen(name + strlen("TIME:"));
        if (0 == flen || TMPL_TIME_FORMAT_MAX_LEN < flen) {
            compile_error(c, "invalid time format `${%s}`", origin);
            goto done;
        }
        var->kind = VAR_TIME_FORMAT;
        var->arg  = strdup(name + strlen("TIME:"));
        if (NULL == var->arg) {
            goto done;
        }
    } else if (!time_function(name, &var->kind)) {
        compile_error(c, "unknown variable `${%s}`", origin);
        goto done;
    }

    rv = parse_time_expr(c, origin, open + 1, var);
    if (0 != rv) {
        var_fini(var);
    }

done:
    if (split) {
        *open         = '(';
        name[len - 1] = ')';
    }
    free(origin);
    return rv;
}

// `name` is the text between `${` and `}`
static int parse_var(compiler_t *c, char *name, var_t *var)
{
    size_t len = strlen(name);

    memset(var, 0, sizeof(*var));

    if (0 == strncmp(name, "TIME", strlen("TIME")) &&
        NULL != strchr(name, '(')) {
        if (0 != parse_time_call(c, name, var)) {
            return -1;
        }
        goto check;
    }

    for (size_t i = 0; i < sizeof(plain_vars) / sizeof(plain_vars[0]); ++i) {
        if (0 ==
            strncmp(name, plain_vars[i].name, strlen(plain_vars[i].name) + 1)) {
            var->kind = plain_vars[i].kind;
            goto check;
        }
    }

    if (0 == strncmp(name, "TIME:", 5)) {
        if (5 == len || 5 + TMPL_TIME_FORMAT_MAX_LEN < len) {
            compile_error(c, "invalid time format `${%s}`", name);
            return -1;
        }
        var->kind = VAR_TIME_FORMAT;
        var->arg  = strdup(name + 5);
    } else if (0 == strncmp(name, "properties[", 11) && ']' == name[len - 1]) {
        if (12 == len) {
            compile_error(c, "empty property key in `${%s}`", name);
            return -1;
        }
        var->kind = VAR_PROPERTY;
        var->arg  = strndup(name + 11, len - 12);
    } else if (0 == strncmp(name, "tags[", 5)) {
        const char *close = strrchr(name, ']');
        if (NULL == close || close == name + 5) {
            compile_error(c, "invalid tag reference `${%s}`", name);
            return -1;
        }

        var->kind = VAR_TAGS_ITEM;
        var->attr = TAG_ATTR_NONE;
        if ('\0' != close[1] &&
            ('.' != close[1] || !parse_tag_attr(close + 2, &var->attr))) {
            compile_error(c, "unknown tag attribute in `${%s}`", name);
            return -1;
        }
        var->arg = strndup(name + 5, close - (name + 5));
    } else if (0 == strncmp(name, "tag.", 4)) {
        var->kind = VAR_TAG;
        if (!parse_tag_attr(name + 4, &var->attr)) {
            compile_error(c, "unknown tag attribute in `${%s}`", name);
            return -1;
        }
    } else {
        compile_error(c, "unknown variable `${%s}`", name);
        return -1;
    }

check:
    if (MQTT_V2_TMPL_DATA == c->kind &&
        (VAR_TAG == var->kind || TIME_SRC_TAG == var->time_src)) {
        compile_error(c, "`${%s}` can only be used in the tag template", name);
        var_fini(var);
        return -1;
    }

    if (MQTT_V2_TMPL_TAG == c->kind &&
        (VAR_TAGS == var->kind || VAR_TAGS_KV == var->kind ||
         VAR_TAGS_ITEM == var->kind || TIME_SRC_NAMED_TAG == var->time_src)) {
        compile_error(c, "`${%s}` cannot be used in the tag template", name);
        var_fini(var);
        return -1;
    }

    return 0;
}

static int str_add_seg(str_tmpl_t *str, const seg_t *seg)
{
    seg_t *segs = realloc(str->segs, (str->n_seg + 1) * sizeof(seg_t));
    if (NULL == segs) {
        return -1;
    }
    str->segs               = segs;
    str->segs[str->n_seg++] = *seg;
    return 0;
}

static int compile_str(compiler_t *c, const char *text, str_tmpl_t *str)
{
    const char *p = text;

    memset(str, 0, sizeof(*str));

    while ('\0' != *p) {
        seg_t       seg   = { 0 };
        const char *start = strstr(p, "${");

        if (NULL == start) {
            seg.text = strdup(p);
            if (0 != str_add_seg(str, &seg)) {
                free(seg.text);
                goto error;
            }
            break;
        }

        if (start > p) {
            seg.text = strndup(p, start - p);
            if (0 != str_add_seg(str, &seg)) {
                free(seg.text);
                goto error;
            }
        }

        const char *end = strchr(start + 2, '}');
        if (NULL == end) {
            compile_error(c, "unterminated variable in \"%s\"", text);
            goto error;
        }

        char *name  = strndup(start + 2, end - start - 2);
        char *first = name;
        while (isspace((unsigned char) *first)) {
            ++first;
        }
        char *last = first + strlen(first);
        while (last > first && isspace((unsigned char) last[-1])) {
            *--last = '\0';
        }

        memset(&seg, 0, sizeof(seg));
        seg.is_var = true;
        int rv     = parse_var(c, first, &seg.var);
        free(name);
        if (0 != rv) {
            goto error;
        }
        if (0 != str_add_seg(str, &seg)) {
            var_fini(&seg.var);
            goto error;
        }

        p = end + 1;
    }

    return 0;

error:
    str_fini(str);
    return -1;
}

static node_t *compile_node(compiler_t *c, json_t *json, int depth)
{
    node_t *node = NULL;

    if (TMPL_MAX_DEPTH < depth) {
        compile_error(c, "%s", "template nesting is too deep");
        return NULL;
    }

    node = calloc(1, sizeof(*node));
    if (NULL == node) {
        return NULL;
    }

    if (json_is_object(json)) {
        const char *key   = NULL;
        json_t *    value = NULL;

        node->kind   = NODE_OBJECT;
        node->fields = calloc(json_object_size(json) + 1, sizeof(field_t));
        if (NULL == node->fields) {
            goto error;
        }

        json_object_foreach(json, key, value)
        {
            field_t *field = &node->fields[node->n_children];
            if (0 != compile_str(c, key, &field->key)) {
                goto error;
            }
            ++node->n_children;
            field->value = compile_node(c, value, depth + 1);
            if (NULL == field->value) {
                goto error;
            }
        }
    } else if (json_is_array(json)) {
        size_t  index = 0;
        json_t *value = NULL;

        node->kind  = NODE_ARRAY;
        node->items = calloc(json_array_size(json) + 1, sizeof(node_t *));
        if (NULL == node->items) {
            goto error;
        }

        json_array_foreach(json, index, value)
        {
            node->items[index] = compile_node(c, value, depth + 1);
            if (NULL == node->items[index]) {
                goto error;
            }
            ++node->n_children;
        }
    } else if (json_is_string(json)) {
        str_tmpl_t str  = { 0 };
        bool       vars = false;

        if (0 != compile_str(c, json_string_value(json), &str)) {
            goto error;
        }

        for (size_t i = 0; i < str.n_seg; ++i) {
            vars = vars || str.segs[i].is_var;
        }

        if (1 == str.n_seg && str.segs[0].is_var) {
            node->kind = NODE_VAR;
            node->var  = str.segs[0].var;
            free(str.segs);
        } else if (vars) {
            node->kind = NODE_STR;
            node->str  = str;
        } else {
            node->kind    = NODE_LITERAL;
            node->literal = json_incref(json);
            str_fini(&str);
        }
    } else {
        node->kind    = NODE_LITERAL;
        node->literal = json_incref(json);
    }

    return node;

error:
    node_free(node);
    return NULL;
}

// variables are also accepted without quotes, e.g. {"value": ${tag.value}}
static char *quote_bare_vars(const char *text)
{
    size_t len    = strlen(text);
    size_t n_var  = 0;
    bool   in_str = false;
    char * out    = NULL;
    size_t j      = 0;

    for (const char *p = text; NULL != (p = strstr(p, "${")); p += 2) {
        ++n_var;
    }

    out = malloc(len + 2 * n_var + 1);
    if (NULL == out) {
        return NULL;
    }

    for (size_t i = 0; i < len; ++i) {
        char ch = text[i];

        if (in_str) {
            out[j++] = ch;
            if ('\\' == ch && i + 1 < len) {
                out[j++] = text[++i];
            } else if ('"' == ch) {
                in_str = false;
            }
            continue;
        }

        if ('"' == ch) {
            in_str = true;
        } else if ('$' == ch && '{' == text[i + 1]) {
            const char *end = strchr(text + i + 2, '}');
            if (NULL != end) {
                size_t n = end - (text + i) + 1;
                out[j++] = '"';
                memcpy(out + j, text + i, n);
                j += n;
                out[j++] = '"';
                i += n - 1;
                continue;
            }
        }
        out[j++] = ch;
    }

    out[j] = '\0';
    return out;
}

static bool var_is_tag_error(const var_t *var)
{
    return VAR_TAG == var->kind && TAG_ATTR_ERROR == var->attr;
}

static bool str_reports_error(const str_tmpl_t *str)
{
    for (size_t i = 0; i < str->n_seg; ++i) {
        if (str->segs[i].is_var && var_is_tag_error(&str->segs[i].var)) {
            return true;
        }
    }
    return false;
}

static bool node_reports_error(const node_t *node)
{
    if (NULL == node) {
        return false;
    }

    switch (node->kind) {
    case NODE_VAR:
        return var_is_tag_error(&node->var);
    case NODE_STR:
        return str_reports_error(&node->str);
    case NODE_OBJECT:
        for (size_t i = 0; i < node->n_children; ++i) {
            if (str_reports_error(&node->fields[i].key) ||
                node_reports_error(node->fields[i].value)) {
                return true;
            }
        }
        return false;
    case NODE_ARRAY:
        for (size_t i = 0; i < node->n_children; ++i) {
            if (node_reports_error(node->items[i])) {
                return true;
            }
        }
        return false;
    case NODE_LITERAL:
        break;
    }

    return false;
}

int mqtt_v2_tmpl_compile(const char *text, mqtt_v2_tmpl_kind_e kind,
                         mqtt_v2_tmpl_t **tmpl, char *err, size_t err_size)
{
    json_error_t jerr   = { 0 };
    compiler_t   c      = { .kind = kind, .err = err, .err_size = err_size };
    char *       quoted = quote_bare_vars(text);

    if (NULL == quoted) {
        compile_error(&c, "%s", "out of memory");
        return -1;
    }

    json_t *json = json_loads(quoted, JSON_DECODE_ANY, &jerr);
    free(quoted);
    if (NULL == json) {
        if (NULL != err && 0 < err_size) {
            snprintf(err, err_size, "invalid JSON at line %d column %d: %s",
                     jerr.line, jerr.column, jerr.text);
        }
        return -1;
    }

    node_t *root = compile_node(&c, json, 1);
    json_decref(json);
    if (NULL == root) {
        return -1;
    }

    *tmpl = calloc(1, sizeof(mqtt_v2_tmpl_t));
    if (NULL == *tmpl) {
        node_free(root);
        return -1;
    }
    (*tmpl)->kind = kind;
    (*tmpl)->root = root;
    (*tmpl)->reports_error =
        MQTT_V2_TMPL_TAG == kind && node_reports_error(root);
    return 0;
}

bool mqtt_v2_tmpl_reports_error(const mqtt_v2_tmpl_t *tmpl)
{
    return NULL != tmpl && tmpl->reports_error;
}

void mqtt_v2_tmpl_free(mqtt_v2_tmpl_t *tmpl)
{
    if (NULL != tmpl) {
        node_free(tmpl->root);
        free(tmpl);
    }
}

static bool tag_is_error(const neu_resp_tag_value_meta_t *tag)
{
    switch (tag->value.type) {
    case NEU_TYPE_ERROR:
        return true;
    case NEU_TYPE_FLOAT:
        return isnan(tag->value.value.f32);
    case NEU_TYPE_DOUBLE:
        return isnan(tag->value.value.d64);
    default:
        return false;
    }
}

size_t mqtt_v2_tags_count(UT_array *tags, bool upload_err)
{
    size_t n = 0;

    if (upload_err) {
        return utarray_len(tags);
    }

    utarray_foreach(tags, neu_resp_tag_value_meta_t *, tag)
    {
        if (!tag_is_error(tag)) {
            ++n;
        }
    }
    return n;
}

static void scope_enter(scope_t *s, const mqtt_v2_render_ctx_t *ctx,
                        neu_resp_tag_value_meta_t *tag)
{
    memset(s, 0, sizeof(*s));
    s->ctx = ctx;
    s->tag = tag;
    neu_tag_value_to_json(tag, &s->jtag);
}

static void scope_leave(scope_t *s)
{
    free(s->jtag.metas);
    if (NEU_JSON_OBJECT == s->jtag.t) {
        json_decref(s->jtag.value.val_object);
    }
}

// reuses the neuron encoder so values look the same as in other plugins,
// with tag precision and float formatting applied
static json_t *value_to_json(enum neu_json_type t, union neu_json_value v,
                             uint8_t precision, double bias)
{
    neu_json_elem_t elem = {
        .name      = "v",
        .t         = t,
        .v         = v,
        .precision = precision,
        .bias      = bias,
    };
    json_t *object = json_object();
    json_t *value  = NULL;

    if (NULL == object) {
        return NULL;
    }

    if (NEU_JSON_OBJECT == t) {
        // the encoder steals the reference
        json_incref(v.val_object);
    }

    neu_json_encode_field(object, &elem, 1);
    value = json_incref(json_object_get(object, "v"));
    json_decref(object);
    return value;
}

static json_t *meta_value(scope_t *s, const char **names)
{
    for (; NULL != *names; ++names) {
        for (int k = 0; k < s->jtag.n_meta; ++k) {
            neu_json_tag_meta_t *meta = &s->jtag.metas[k];
            if (NULL != meta->name && 0 == strcasecmp(meta->name, *names)) {
                return value_to_json(meta->t, meta->value, 0, 0);
            }
        }
    }
    return NULL;
}

// drivers report sample attributes, older ones metadata instead
static json_t *render_tag_quality(scope_t *s)
{
    const neu_tag_sample_attrs_t *attrs = &s->tag->value.attrs;

    if (attrs->has_quality) {
        return json_integer(attrs->quality);
    }
    return meta_value(s, quality_metas);
}

static json_t *render_one_timestamp(scope_t *s, bool present, int64_t value,
                                    const char **metas)
{
    if (present) {
        return json_integer(value);
    }
    return meta_value(s, metas);
}

static json_t *render_tag_timestamp(scope_t *s)
{
    const neu_tag_sample_attrs_t *attrs = &s->tag->value.attrs;

    if (attrs->has_timestamp) {
        return json_integer(attrs->timestamp);
    }
    if (attrs->has_source_timestamp) {
        return json_integer(attrs->source_timestamp);
    }
    if (attrs->has_server_timestamp) {
        return json_integer(attrs->server_timestamp);
    }
    return meta_value(s, timestamp_metas);
}

static json_t *render_tag_info(scope_t *s, tag_attr_e attr)
{
    const mqtt_v2_render_ctx_t *ctx  = s->ctx;
    mqtt_v2_tag_info_t          info = { 0 };

    if (NULL == ctx->tag_info ||
        !ctx->tag_info(ctx->tag_info_ctx, s->tag->tag, &info)) {
        return NULL;
    }

    const char *text =
        TAG_ATTR_DESCRIPTION == attr ? info.description : info.unit;
    if (NULL == text || '\0' == text[0]) {
        return NULL;
    }
    return json_string(text);
}

static json_t *render_tag_attr(scope_t *s, tag_attr_e attr)
{
    switch (attr) {
    case TAG_ATTR_NONE:
        if (NULL == s->ctx->tag_tmpl) {
            return NULL;
        }
        return render_node(s->ctx->tag_tmpl->root, s);
    case TAG_ATTR_NAME:
        return json_string(s->tag->tag);
    case TAG_ATTR_VALUE:
        if (0 != s->jtag.error) {
            return NULL;
        }
        return value_to_json(s->jtag.t, s->jtag.value, s->jtag.precision,
                             s->jtag.datatag.bias);
    case TAG_ATTR_ERROR:
        return 0 != s->jtag.error ? json_integer(s->jtag.error) : NULL;
    case TAG_ATTR_QUALITY:
        return render_tag_quality(s);
    case TAG_ATTR_TIMESTAMP:
        return render_tag_timestamp(s);
    case TAG_ATTR_SOURCE_TIMESTAMP:
        return render_one_timestamp(s, s->tag->value.attrs.has_source_timestamp,
                                    s->tag->value.attrs.source_timestamp,
                                    source_timestamp_metas);
    case TAG_ATTR_SERVER_TIMESTAMP:
        return render_one_timestamp(s, s->tag->value.attrs.has_server_timestamp,
                                    s->tag->value.attrs.server_timestamp,
                                    server_timestamp_metas);
    case TAG_ATTR_DESCRIPTION:
    case TAG_ATTR_UNIT:
        return render_tag_info(s, attr);
    case TAG_ATTR_TYPE: {
        // auto typed tags take the type of their samples
        neu_type_e type = NEU_TYPE_AUTO != s->tag->resolved_type
            ? s->tag->resolved_type
            : s->tag->datatag.type;
        const char *name = neu_type_string(type);
        if (0 == strncmp(name, "NEU_TYPE_", 9)) {
            name += 9;
        }
        return '\0' != name[0] ? json_string(name) : NULL;
    }
    }

    return NULL;
}

static json_t *render_tag(const mqtt_v2_render_ctx_t *ctx,
                          neu_resp_tag_value_meta_t *tag, tag_attr_e attr)
{
    scope_t s      = { 0 };
    json_t *result = NULL;

    scope_enter(&s, ctx, tag);
    if (0 == s.jtag.error || ctx->upload_err) {
        result = render_tag_attr(&s, attr);
    }
    scope_leave(&s);

    return result;
}

static json_t *render_tags(const mqtt_v2_render_ctx_t *ctx, bool kv)
{
    json_t *out = kv ? json_object() : json_array();

    if (NULL == out || NULL == ctx->tags) {
        return out;
    }

    utarray_foreach(ctx->tags, neu_resp_tag_value_meta_t *, tag)
    {
        json_t *value = render_tag(ctx, tag, TAG_ATTR_NONE);
        if (NULL == value) {
            continue;
        }

        if (kv) {
            json_object_set_new(out, tag->tag, value);
        } else {
            json_array_append_new(out, value);
        }
    }

    return out;
}

static json_t *render_tags_item(const mqtt_v2_render_ctx_t *ctx,
                                const var_t *               var)
{
    if (NULL == ctx->tags) {
        return NULL;
    }

    utarray_foreach(ctx->tags, neu_resp_tag_value_meta_t *, tag)
    {
        if (0 == strncmp(tag->tag, var->arg, NEU_TAG_NAME_LEN)) {
            return render_tag(ctx, tag, var->attr);
        }
    }

    return NULL;
}

static bool meta_timestamp(scope_t *s, const char **names, int64_t *out)
{
    for (; NULL != *names; ++names) {
        for (int k = 0; k < s->jtag.n_meta; ++k) {
            neu_json_tag_meta_t *meta = &s->jtag.metas[k];
            if (NULL != meta->name && 0 == strcasecmp(meta->name, *names) &&
                NEU_JSON_INT == meta->t) {
                *out = meta->value.val_int;
                return true;
            }
        }
    }
    return false;
}

static bool attr_timestamp(scope_t *s, tag_attr_e attr, int64_t *out)
{
    const neu_tag_sample_attrs_t *attrs = &s->tag->value.attrs;

    switch (attr) {
    case TAG_ATTR_TIMESTAMP:
        if (attrs->has_timestamp) {
            *out = attrs->timestamp;
            return true;
        }
        if (attrs->has_source_timestamp) {
            *out = attrs->source_timestamp;
            return true;
        }
        if (attrs->has_server_timestamp) {
            *out = attrs->server_timestamp;
            return true;
        }
        return meta_timestamp(s, timestamp_metas, out);
    case TAG_ATTR_SOURCE_TIMESTAMP:
        if (attrs->has_source_timestamp) {
            *out = attrs->source_timestamp;
            return true;
        }
        return meta_timestamp(s, source_timestamp_metas, out);
    case TAG_ATTR_SERVER_TIMESTAMP:
        if (attrs->has_server_timestamp) {
            *out = attrs->server_timestamp;
            return true;
        }
        return meta_timestamp(s, server_timestamp_metas, out);
    default:
        return false;
    }
}

static bool resolve_time(const var_t *var, scope_t *s, int64_t *out)
{
    if (TIME_SRC_GROUP == var->time_src) {
        *out = s->ctx->timestamp;
        return true;
    }
    if (TIME_SRC_TAG == var->time_src) {
        return NULL != s->tag && attr_timestamp(s, var->attr, out);
    }
    if (NULL == s->ctx->tags || NULL == var->time_ref) {
        return false;
    }

    utarray_foreach(s->ctx->tags, neu_resp_tag_value_meta_t *, tag)
    {
        scope_t one = { 0 };
        bool    ok  = false;

        if (0 != strncmp(tag->tag, var->time_ref, NEU_TAG_NAME_LEN)) {
            continue;
        }
        scope_enter(&one, s->ctx, tag);
        ok = attr_timestamp(&one, var->attr, out);
        scope_leave(&one);
        return ok;
    }
    return false;
}

static json_t *render_time(var_kind_e kind, const char *format,
                           int64_t timestamp)
{
    time_t    sec                               = (time_t)(timestamp / 1000);
    int       ms                                = (int) (timestamp % 1000);
    struct tm tm                                = { 0 };
    char      buf[TMPL_TIME_FORMAT_MAX_LEN * 2] = { 0 };
    size_t    n                                 = 0;

    switch (kind) {
    case VAR_TIME_UTC:
        gmtime_r(&sec, &tm);
        n = strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
        snprintf(buf + n, sizeof(buf) - n, ".%03dZ", ms);
        break;
    case VAR_TIME_RFC3339: {
        char zone[8] = { 0 };
        localtime_r(&sec, &tm);
        n = strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
        strftime(zone, sizeof(zone), "%z", &tm);
        snprintf(buf + n, sizeof(buf) - n, ".%03d%.3s:%.2s", ms, zone,
                 zone + 3);
        break;
    }
    case VAR_TIME_LOCAL:
        localtime_r(&sec, &tm);
        n = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        snprintf(buf + n, sizeof(buf) - n, ".%03d", ms);
        break;
    case VAR_TIME_UNIX:
        return json_integer(timestamp / 1000);
    default:
        localtime_r(&sec, &tm);
        if (0 == strftime(buf, sizeof(buf), format, &tm)) {
            buf[0] = '\0';
        }
        break;
    }

    return json_string(buf);
}

static json_t *string_or_null(const char *str)
{
    return NULL != str ? json_string(str) : NULL;
}

static json_t *render_var(const var_t *var, scope_t *s)
{
    const mqtt_v2_render_ctx_t *ctx = s->ctx;

    switch (var->kind) {
    case VAR_APP:
        return string_or_null(ctx->app);
    case VAR_DEVICE:
        return string_or_null(ctx->device);
    case VAR_GROUP:
        return string_or_null(ctx->group);
    case VAR_SUB_TOPIC:
        return string_or_null(ctx->sub_topic);
    case VAR_TIMESTAMP:
        return json_integer(ctx->timestamp);
    case VAR_TIME_RFC3339:
    case VAR_TIME_UTC:
    case VAR_TIME_LOCAL:
    case VAR_TIME_UNIX:
    case VAR_TIME_FORMAT: {
        int64_t timestamp = 0;
        if (!resolve_time(var, s, &timestamp)) {
            return NULL;
        }
        return render_time(var->kind, var->arg, timestamp);
    }
    case VAR_PROPERTIES:
        if (NULL == ctx->properties || 0 == json_object_size(ctx->properties)) {
            return NULL;
        }
        return json_deep_copy(ctx->properties);
    case VAR_PROPERTY: {
        json_t *value = json_object_get(ctx->properties, var->arg);
        return NULL != value ? json_deep_copy(value) : NULL;
    }
    case VAR_TAGS:
        return render_tags(ctx, false);
    case VAR_TAGS_KV:
        return render_tags(ctx, true);
    case VAR_TAGS_ITEM:
        return render_tags_item(ctx, var);
    case VAR_TAG:
        return NULL != s->tag ? render_tag_attr(s, var->attr) : NULL;
    }

    return NULL;
}

static char *json_to_text(json_t *value)
{
    if (json_is_string(value)) {
        return strdup(json_string_value(value));
    }
    return json_dumps(value,
                      JSON_ENCODE_ANY | JSON_COMPACT | JSON_REAL_PRECISION(16));
}

// returns NULL if `str` is a single variable without a value
static char *render_str(const str_tmpl_t *str, scope_t *s)
{
    strbuf_t buf = { 0 };

    if (1 == str->n_seg && str->segs[0].is_var) {
        json_t *value = render_var(&str->segs[0].var, s);
        char *  text  = NULL;
        if (NULL != value) {
            text = json_to_text(value);
            json_decref(value);
        }
        return text;
    }

    for (size_t i = 0; i < str->n_seg; ++i) {
        const seg_t *seg = &str->segs[i];

        if (!seg->is_var) {
            strbuf_append(&buf, seg->text, strlen(seg->text));
            continue;
        }

        json_t *value = render_var(&seg->var, s);
        if (NULL != value) {
            char *text = json_to_text(value);
            if (NULL != text) {
                strbuf_append(&buf, text, strlen(text));
                free(text);
            }
            json_decref(value);
        }
    }

    return strbuf_take(&buf);
}

static json_t *render_node(const node_t *node, scope_t *s)
{
    switch (node->kind) {
    case NODE_LITERAL:
        return json_deep_copy(node->literal);
    case NODE_VAR:
        return render_var(&node->var, s);
    case NODE_STR: {
        char *  text  = render_str(&node->str, s);
        json_t *value = NULL != text ? json_string(text) : NULL;
        free(text);
        return value;
    }
    case NODE_OBJECT: {
        json_t *object = json_object();
        if (NULL == object) {
            return NULL;
        }
        for (size_t i = 0; i < node->n_children; ++i) {
            char *key = render_str(&node->fields[i].key, s);
            if (NULL == key || '\0' == key[0]) {
                free(key);
                continue;
            }
            json_t *value = render_node(node->fields[i].value, s);
            if (NULL != value) {
                json_object_set_new(object, key, value);
            }
            free(key);
        }
        return object;
    }
    case NODE_ARRAY: {
        json_t *array = json_array();
        if (NULL == array) {
            return NULL;
        }
        for (size_t i = 0; i < node->n_children; ++i) {
            json_t *value = render_node(node->items[i], s);
            json_array_append_new(array, NULL != value ? value : json_null());
        }
        return array;
    }
    }

    return NULL;
}

char *mqtt_v2_tmpl_render(const mqtt_v2_tmpl_t *      tmpl,
                          const mqtt_v2_render_ctx_t *ctx)
{
    scope_t s    = { .ctx = ctx };
    json_t *root = render_node(tmpl->root, &s);
    char *  out  = NULL;

    if (NULL != root) {
        out = json_dumps(root, JSON_ENCODE_ANY | JSON_REAL_PRECISION(16));
        json_decref(root);
    }

    return out;
}

static void append_name(strbuf_t *buf, const char *name)
{
    if (NULL == name) {
        return;
    }

    for (const char *p = name; '\0' != *p; ++p) {
        char ch = ('+' == *p || '#' == *p) ? '_' : *p;
        strbuf_append(buf, &ch, 1);
    }
}

char *mqtt_v2_topic_render(const char *tmpl, const char *prefix,
                           const char *app, const char *device,
                           const char *group)
{
    static const char *vars[]  = { "${prefix}", "${app}", "${device}",
                                  "${group}" };
    const char *       names[] = { prefix, app, device, group };
    strbuf_t           buf     = { 0 };

    for (const char *p = tmpl; '\0' != *p;) {
        bool matched = false;

        for (size_t i = 0; i < sizeof(vars) / sizeof(vars[0]); ++i) {
            size_t n = strlen(vars[i]);
            if (0 == strncmp(p, vars[i], n)) {
                append_name(&buf, names[i]);
                p += n;
                matched = true;
                break;
            }
        }

        if (!matched) {
            strbuf_append(&buf, p, 1);
            ++p;
        }
    }

    return strbuf_take(&buf);
}
