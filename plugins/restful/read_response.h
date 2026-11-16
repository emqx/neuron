#ifndef _NEU_REST_READ_RESPONSE_H_
#define _NEU_REST_READ_RESPONSE_H_

#include <stdbool.h>

#include "utils/utarray.h"

typedef struct {
    void *       response;
    UT_array *   tags;
    unsigned int offset;
    bool         paginate;
} neu_http_read_response_t;

typedef struct {
    void *    response;
    UT_array *tags;
} neu_http_get_tags_response_t;

int neu_http_encode_read_response(void *json_object, void *param);
int neu_http_encode_get_tags_response(void *json_object, void *param);

#endif
