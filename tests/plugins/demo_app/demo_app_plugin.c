/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "persist/persist.h"
#include "utils/base64.h"
#include <neuron.h>

#include "demo_app_plugin.h"

static const char *server_cert =
    "LS0tLS1CRUdJTiBDRVJUSUZJQ0FURS0tLS0tCk1JSUZuVENDQTRXZ0F3SUJBZ0lVZFYvKzJLSk"
    "xrM3NOQytabDRmNlBONmNKbzlvd0RRWUpLb1pJaHZjTkFRRUwKQlFBd1hURUxNQWtHQTFVRUJo"
    "TUNRMDR4RVRBUEJnTlZCQWdNQ0ZOb1lXNW5TR0ZwTVJFd0R3WURWUVFIREFoVAphR0Z1WjBoaG"
    "FURU1NQW9HQTFVRUNnd0RUMDVGTVF3d0NnWURWUVFMREFOUFRrVXhEREFLQmdOVkJBTU1BMDlP"
    "ClJUQWdGdzB5TlRBNU1UY3dNVE0wTlRKYUdBOHlNVEkxTURreE9EQXhNelExTWxvd1hURUxNQW"
    "tHQTFVRUJoTUMKUTA0eEVUQVBCZ05WQkFnTUNGTm9ZVzVuU0dGcE1SRXdEd1lEVlFRSERBaFRh"
    "R0Z1WjBoaGFURU1NQW9HQTFVRQpDZ3dEVDA1Rk1Rd3dDZ1lEVlFRTERBTlBUa1V4RERBS0JnTl"
    "ZCQU1NQTA5T1JUQ0NBaUl3RFFZSktvWklodmNOCkFRRUJCUUFEZ2dJUEFEQ0NBZ29DZ2dJQkFL"
    "Uy9NdmNTdTA1MDlndDlETTlaa0NFYU96MmVDRjBFWnNqeU5NRVQKQTlLdW56Zmx6ejEwYkxnZG"
    "pJNTd5VHp1VFRqZkI5N29rcXB0MDl4K3Rzb0Joc2hFdVB0MUNadUQveDdqcnM1Qwp2R1p1ellT"
    "bjk3RWd2QitXMEVmRSs5YWVVdi8xNkdybDk0bGNwaStFM2Y3KzJTR2dTcDNqQncxdytTaHhwaX"
    "NBCklHbzR2Uy8rbHpHNDJGUzhOb3VTbVk0d1ZYQU5EdGNzYnNTMElTTDAweUM1dk43WFlmNjVi"
    "OTcrUURxbEkwVGkKM0c2UzdjNmpnWlRUUVo3Z3NWOGZjRjZJZGpaNXY0SWJleG9pY3k5MFJWZV"
    "psZXNHYzJzZEFGODN0SXFiMktuZApwN3VoakJGYktIZEQ4S2NQdVRaZ3U2WEJiN0lUTHVqZ3Nx"
    "ZjJNNkpJZklUWG5yTVd3bFd0aW5TYXNaZUZFY01zCnppM2RqQ3ZPRlFaRXJXSGEySlQ3eHk0R3"
    "k5WGQrdm5IcjZKZXJYV0tLMFlHZU83QTBMRmVjZVRJcEt6RUtTQmYKYzhMNCtxUGljY3pOVUxk"
    "clFTelIwRzlWTU9yK3REalNuMEo4TFBsSzV0c09kZjZHR0sraThRV0M5SDJoS282bwpJRWNFeX"
    "BxaDdNRHZ0Z2FRdVRaVmQvb3kyQllkYmZuY0lHT2FGLzlMZFM2RUdTZyt1N0hIVVFkSzZGZjZK"
    "WDFFCk5BMCtwMm9hVEtlbkJYYWRHeFJySVlxUWpERW9BVzUzZjY4dzNkdDRKNnYzZ0pxOXFSdV"
    "JWK002cjNLajNLejEKSlBwRlRtdDFXc3NWSk82NzRKZUhEbXpqWmtzQnptN3Z6MnY3b1NYOEQr"
    "NkVjUEpIQS8zWDU5bzFRajRDR1p1OAppbHZ2QWdNQkFBR2pVekJSTUIwR0ExVWREZ1FXQkJTOX"
    "lEeUZwRjhiRUx4RzJXbjBnTzBlZ1NEQUNEQWZCZ05WCkhTTUVHREFXZ0JTOXlEeUZwRjhiRUx4"
    "RzJXbjBnTzBlZ1NEQUNEQVBCZ05WSFJNQkFmOEVCVEFEQVFIL01BMEcKQ1NxR1NJYjNEUUVCQ3"
    "dVQUE0SUNBUUNBL1FCSWlaYlZONHRZSG1HeVNCdnVaQzNoN2J6b1VuUnQyQTI0L2o3SwpGWklB"
    "WDRQbWJ1djJ1SUdKQ1c4RXM5YVMrbktLNms3ZEN3Vk1GM043bWtaa0E3cVN2N05MZ0dkVStxZ2"
    "JiU3MvClppVWxKTjlPUXBFYlFpUjRweGx5eTVsL3dodGJKZ1RXYTEzSEVhTm9mUzk2aW5udndM"
    "cXRSYitDbSszM0F0M3IKcVk4L1hKU0pONXVKdmh1OGsvOERpbVNKTnZFZEFNYXk4aS9PRmYvYT"
    "haQXJzUVNydi9MTUdhMUdXU1k2OEgzVgphWGcyN05QbTRHYnVCSWtPYkhTbVdtYms2NDZLRTgr"
    "Qk5BSHdCVXNiMUJHMU5xOXFsWVVlTk0xZ2Zia0swcXJIClJrNUhISDdQS0N6dU9GLzhJdnJQdX"
    "k3bFVOS0Y3ajE4Um52VzQ2L1czOExqRlptZ0VEK3l3T3lpSEVGTWF1MzIKZ09adUg1K0JKTjRT"
    "K09LK09lZ1NyRUJTWi9FblZBOHo1djE3emh0SVA1SmM3eExXb25uNG9LMllzVTRkeUVaZwpxem"
    "MvSVE5LzJYUWIrM2EvVVdnT1oxMEJGdmhScEZhWGlNaXp4NjVGRmYzTjJoRXRQMElLUTR1dmc2"
    "Vm5BWG1PCkZHRkx5ZnJqWVgrZkhZV0JZK1hzSzRya3NCbHJWcTB0MXlaR2UwWWFzd1JWaVdUMz"
    "lvT2VUNmdOeGtkK3Nuc3EKc2V0MVVoMmQwcnlIaHBnQ1grcnNUTTE1c2lNYmExTDB1NWw1dWk2"
    "NWVuYVptaUQ4d3hyTjNXbkptNHdCUTNwbAo1VzcvRnlHWmpreDVuS1VLeTlFUk5yL3hZOERtbV"
    "VaVkpNallxMXM3bkxKeEloazJpUnFRS0UwT1VUWlc5di9ZCmZnPT0KLS0tLS1FTkQgQ0VSVElG"
    "SUNBVEUtLS0tLQo=";

static const char *server_private_key =
    "LS0tLS1CRUdJTiBQUklWQVRFIEtFWS0tLS0tCk1JSUpSUUlCQURBTkJna3Foa2lHOXcwQkFRRU"
    "ZBQVNDQ1M4d2dna3JBZ0VBQW9JQ0FRQ2t2ekwzRXJ0T2RQWUwKZlF6UFdaQWhHanM5bmdoZEJH"
    "Ykk4alRCRXdQU3JwODM1Yzg5ZEd5NEhZeU9lOGs4N2swNDN3ZmU2SktxYmRQYwpmcmJLQVliSV"
    "JMajdkUW1iZy84ZTQ2N09RcnhtYnMyRXAvZXhJTHdmbHRCSHhQdldubEwvOWVocTVmZUpYS1l2"
    "CmhOMysvdGtob0VxZDR3Y05jUGtvY2FZckFDQnFPTDB2L3BjeHVOaFV2RGFMa3BtT01GVndEUT"
    "dYTEc3RXRDRWkKOU5NZ3ViemUxMkgrdVcvZS9rQTZwU05FNHR4dWt1M09vNEdVMDBHZTRMRmZI"
    "M0JlaUhZMmViK0NHM3NhSW5NdgpkRVZYbVpYckJuTnJIUUJmTjdTS205aXAzYWU3b1l3Uld5aD"
    "NRL0NuRDdrMllMdWx3Vyt5RXk3bzRMS245ak9pClNIeUUxNTZ6RnNKVnJZcDBtckdYaFJIRExN"
    "NHQzWXdyemhVR1JLMWgydGlVKzhjdUJzdlYzZnI1eDYraVhxMTEKaWl0R0JuanV3TkN4WG5Ia3"
    "lLU3N4Q2tnWDNQQytQcWo0bkhNelZDM2EwRXMwZEJ2VlREcS9yUTQwcDlDZkN6NQpTdWJiRG5Y"
    "K2hoaXZvdkVGZ3ZSOW9TcU9xQ0JIQk1xYW9lekE3N1lHa0xrMlZYZjZNdGdXSFczNTNDQmptaG"
    "YvClMzVXVoQmtvUHJ1eHgxRUhTdWhYK2lWOVJEUU5QcWRxR2t5bnB3VjJuUnNVYXlHS2tJd3hL"
    "QUZ1ZDMrdk1OM2IKZUNlcjk0Q2F2YWtia1Zmak9xOXlvOXlzOVNUNlJVNXJkVnJMRlNUdXUrQ1"
    "hodzVzNDJaTEFjNXU3ODlyKzZFbAovQS91aEhEeVJ3UDkxK2ZhTlVJK0FobWJ2SXBiN3dJREFR"
    "QUJBb0lDQVFDVnovaThqU0FmUEdUeUZZa3NsVGxuCm9MNzJUVnMxbXVjTkhKTzBiMkl1bC9UWm"
    "svVmM0UjVzWHhLT0R5cjBhWTkzZy9sWVR3M3dSV2MvUDB0aGJ2bnQKNi9CN1dmYWVvTzNDWFRG"
    "OUIxcmpteWJ3MXYxNjZWV3BOQys2MC9wZk5DSXQrS1NkY1Bva3cwZXNOcHJaWTA1NApNWmNvOU"
    "daWlpyNXNkTXl5bGE3a2FTeEJGTGJRMUZLRTI1S09ZWVdaczRJK0h1Rzc4Zi9ZaUNVSjhMQjlO"
    "ZW4yCmlzM21JU01HR1JhM1BheFEzQ3V2c1pUbkwvQ0tNWUdleEowRzcwUHo4d3ZzaGRJTXNvak"
    "RQMGlYZ3BmOERRSXoKY3U1S0hWbFZQUWJFZE93VnJDYjNqUFFlWkZMc0FiQTVleWJsdUJscnBm"
    "ZTRhNUZnMjlRYXdOdzdXc2RBMWdMTgpTczVvSHhqWDM0TFdvN3NTVkFUaWxDMWNVeDdBVVVmTk"
    "VicWNRd1EvcVo4R294Sk9Lenl0VlJlUVpGTkxYMnhEClRMUXlLNHJZWTlrWlZuS0NrL2VWbWdF"
    "OEUrKzBDSlNseHVSczFtejB1N1psbmRzT1h1anJSSkEyeTFmTTFyODEKemlXanhNVk9CL3RxMV"
    "FaaHJFRXlNa0JUcnQwNTlPMktYOE1qQy9FRFBTYlZlWEVIWHNtT0gybGxDVlI1eVpWbwpvTzNa"
    "NWpYOHZpYUl5Mi93bGNDaVVLTityTEJoWGhTem9ZM0J3YXpjL0RZZXMzMXFTZjIyQkNERko2N0"
    "pucGZYCklOWWJ2dXo1MSttMWdNbTk5N3ZQdE5BNjBSOURaeUIreFg0SVEvTnBJYkFQQzloNVVo"
    "QXFZY2s4bVlxTTM1Z3AKaHcvZ1U3NmJ6YnMzUlJ4QmttR2VvUUtDQVFFQTFKdVBEZFRSZitJOF"
    "p0Q0FHbndraHpmaTdBWG5IdEE4c2FSagoyelNQV2VaQzlTRjNCM2E2QlFIaC9wbStuTGdIUnFn"
    "bWVPWUY5L1AxWURoNWhacUJ1RnFSL3cvbG9CN2lEOTQ3Ckgyd21XS004MC8vRkFwOHoxbGlaSX"
    "ZNSW1UVTNMekMwbkZlVWlQVDA3UjI1T3ZQODVBQytydGNHNllOb3l6djAKRHpjb2pyY0FpMWc3"
    "OEEzZmsyTlpyTm8vZTM0a2NlZ2g0blFCQkUzNEd4N1BLWlpSYUVuUVh6anlsYlByWURIKwpWUF"
    "NBN1F3dUh3aHJEQWpJWWNhQUVIeVFJOGJOZEI5ak10QzBHVnpWcFJ5bHRhbmI2T1kwTUl0eXdr"
    "TzVHL2VlClBlVmdXQkdVbFljQ0VwOFF3SWxkTnp2eGxWOCs2b21XU21lYnJqbVdLVVlZMnFkTU"
    "xRS0NBUUVBeGw3NzVtQmQKa1BHNmhWTVdwRktHQ3RBaURJRGpkTXlTUHNUa3FLTTlwOTR6bHdw"
    "V2NyZTJBL3BaNGJSNnExY1ZZbUZSWTIvcQpPdHFNNGhmMTZkN1pmNTd0U09XU1ZDMnpGSVJaQ1"
    "ZnT0RGU2hFcUVuMTBFVlJoZDV1YTZjVzJTMFlhb3EyZUxpClJZSW96b0Y1SVBDU0FSZnBDOFdp"
    "Mm1ic0luVGhCclpySTVpbElNcDJaSTBTMTRIOXpnNXY5aEczNE40WmszUXUKcDF3cFpRYzJjRk"
    "VJQ1NHNUVoTkpQTjN2QVpkTkcyTmJXbll3THZINlpzN2Jxcml0UGhyQkMvVVhRU0ZuVU12WApi"
    "WFd0SnlxUWRjQ2JRY0h0TlA1RWpjT2RBSnZpMnJKRFA0ak9iVmlyM0YvNzVRdVhlbGlnSDJwMX"
    "UwQUJHSS8zCjBoTEorRUU1N3BJdUN3S0NBUUVBcWtxekthUDE3aFc2eGF0cGdFMUJRT0ZkckNr"
    "S04xOFp6anZiRThBa3RpQSsKTDNRUGovWGtCNUM3Z0orSzBxS0FpWEt6NWhCd1pXdU5kZldtT0"
    "lKS051eXNsZjBZc3NrcHp3WDBteldYL1ZVNgpxcC9xSURCK1o4aHRXRllMNUJPQTBSYVZBOGtP"
    "bXlpQmFjOXVCeTlZdHhOMXFEdHBPTzdkcURPc1IrZXBYNjVyCkRER3ZTSmNFSmx6a2ZoUU4xdz"
    "l4aDU4a1RPc1h5V2ZlTzNMUkFnRjUwU3VXY0Q3LzV6TEdiRFYrL25NZE5VR1YKUzV3RmlnMGEy"
    "MTRRUTZSU0c2WG4rSVFQZzZzeEprTjZpSmJLa0JpdW9QeUtmdnVpL3NFWjZySEttTTg1L1RDUA"
    "pyN2tFeXYrSGZvQklIcTZ2MFRJS0JmZ1FYaVpneHdzdklINForRUVhUFFLQ0FRRUF3NGFaK2RC"
    "Z1NWN0tmMkdQCmZyWk1vdG5WeGNkQTZ6NDhwQndFV2VoVS9HSjdMVlFtTTNxNnNKOFIrdk9ldW"
    "NYak1RNUZYNkxQRitXVldjRk8KUlkrL2lCaEpRRUh3QW9Mczhic2wyNXpzYytEeXh0OEwyeEt1"
    "ZW1EblBVdExtSlhoYWlhRmlEQW00V1pTL0lFSAp1SjBHL0licDBRMmo0R3YwTWw1TDh3YlpBY1"
    "FTZHdpNHU1R0YwZCt5SUdtQWwrV00vZHRORUZkREtQa2hZQkJFCjYxNGpjb1RIMjNZaENScWoy"
    "MGliMnZRSzlsWnFWY3R6Q2VTNUJrUnpsalJldFgrRnhVKzhleG9BcEVhS1BvR28KMURmZGtHb3"
    "FlY0pxSmVWNm1rQU0wWTFGMVpqdUJ6K1FYbTVKWmRFUUpLcTRZVjdZM3BGT3d6d2NnL2E3RnJT"
    "TAorb2doVXdLQ0FRRUFwQlRIeWVXY0M2cjlFbm9iOE5hVWVCYmlZMFd4cU8xMnB6OXpDem0rSl"
    "JqNFhic003TVkwCjZWWnJqRU00RGZGeWxsWXlmei9FVGpLcHRmeElGTTdicWN1VmtBU3ErYkx2"
    "cDR6bktWSy9Ka1pvUGl0SzA1ZkwKRGRIczNpUHBRbitnWXpNVnBka0Y3bE5rdUhoaklKand4Vk"
    "Z1bGZJQ2dKSE9yZmdWcXM2TFJadVJ0Q0dVZlZVegpRd3BSaXZpckJzOU8rMGt4OWkxaU5yamQ1"
    "K3JFRWIzTEtSS3VOT3lLVnhCMkljYVNZTWs5OHRtTWl5MEQ1U1dzCjMvWHpabzFZQkptLzRobW"
    "JjbWtVUTE4UUNoMXJVbXdaLzdRZXdwTDl0aUlXU2FteStPTnJUZ21KQWlweTJPTXcKL3RKd2Nh"
    "bkk4SXJzTmdCOVE0bzIvMHlBTlNsR2hEekQyQT09Ci0tLS0tRU5EIFBSSVZBVEUgS0VZLS0tLS"
    "0K";

static const char *subject = "CN=DemoApp, O=EMQ, C=CN";
static const char *issuer  = "CN=DemoApp, O=EMQ, C=CN";

#define DESCRIPTION "Demo northbound APP plugin"
#define DESCRIPTION_ZH "演示北向应用插件"

// Forward declarations
static neu_plugin_t *demo_app_plugin_open(void);
static int           demo_app_plugin_close(neu_plugin_t *plugin);
static int           demo_app_plugin_init(neu_plugin_t *plugin, bool load);
static int           demo_app_plugin_uninit(neu_plugin_t *plugin);
static int           demo_app_plugin_start(neu_plugin_t *plugin);
static int           demo_app_plugin_stop(neu_plugin_t *plugin);
static int demo_app_plugin_config(neu_plugin_t *plugin, const char *config);
static int demo_app_plugin_request(neu_plugin_t *      plugin,
                                   neu_reqresp_head_t *head, void *data);

// Plugin interface functions table
const neu_plugin_intf_funs_t demo_app_plugin_intf_funs = {
    .open    = demo_app_plugin_open,
    .close   = demo_app_plugin_close,
    .init    = demo_app_plugin_init,
    .uninit  = demo_app_plugin_uninit,
    .start   = demo_app_plugin_start,
    .stop    = demo_app_plugin_stop,
    .setting = demo_app_plugin_config,
    .request = demo_app_plugin_request,
};

// Plugin module definition
const neu_plugin_module_t neu_plugin_module = {
    .version         = NEURON_PLUGIN_VER_1_0,
    .schema          = "demo_app",
    .module_name     = "Demo APP",
    .module_descr    = DESCRIPTION,
    .module_descr_zh = DESCRIPTION_ZH,
    .intf_funs       = &demo_app_plugin_intf_funs,
    .kind            = NEU_PLUGIN_KIND_SYSTEM,
    .type            = NEU_NA_TYPE_APP,
    .display         = true,
    .single          = false,
};

// Helper functions for message handling
static int handle_cert_messages(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                                void *data);
static int handle_security_policy_messages(neu_plugin_t *      plugin,
                                           neu_reqresp_head_t *head,
                                           void *              data);
static int handle_auth_messages(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                                void *data);

static char *get_current_timestamp(void);

// Implementation

static neu_plugin_t *demo_app_plugin_open(void)
{
    neu_plugin_t *plugin = calloc(1, sizeof(neu_plugin_t));
    if (plugin == NULL) {
        return NULL;
    }

    neu_plugin_common_init(&plugin->common);

    return plugin;
}

static int demo_app_plugin_close(neu_plugin_t *plugin)
{
    if (plugin == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    free(plugin);
    return NEU_ERR_SUCCESS;
}

static int demo_app_plugin_init(neu_plugin_t *plugin, bool load)
{
    (void) load;
    if (plugin == NULL) {
        return NEU_ERR_EINTERNAL;
    }
    return NEU_ERR_SUCCESS;
}

static int demo_app_plugin_uninit(neu_plugin_t *plugin)
{
    if (plugin == NULL) {
        return NEU_ERR_EINTERNAL;
    }
    return NEU_ERR_SUCCESS;
}

static int demo_app_plugin_start(neu_plugin_t *plugin)
{
    if (plugin == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
    plog_notice(plugin, "demo_app plugin started and connected");
    return NEU_ERR_SUCCESS;
}

static int demo_app_plugin_stop(neu_plugin_t *plugin)
{
    if (plugin == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    plog_notice(plugin, "demo_app plugin stopped and disconnected");
    return NEU_ERR_SUCCESS;
}

static int demo_app_plugin_config(neu_plugin_t *plugin, const char *config)
{
    if (plugin == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    plog_notice(plugin, "demo_app plugin configured with: %s",
                config ? config : "NULL");
    return NEU_ERR_SUCCESS;
}

static int demo_app_plugin_request(neu_plugin_t *      plugin,
                                   neu_reqresp_head_t *head, void *data)
{
    if (plugin == NULL || head == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    int ret = NEU_ERR_SUCCESS;

    switch (head->type) {
    // Certificate related messages
    case NEU_REQ_SERVER_CERT_SELF_SIGN:
    case NEU_REQ_SERVER_CERT_UPLOAD:
    case NEU_REQ_SERVER_CERT_INFO:
    case NEU_REQ_SERVER_CERT_EXPORT:
    case NEU_REQ_CLIENT_CERT_UPLOAD:
    case NEU_REQ_CLIENT_CERT_DELETE:
    case NEU_REQ_CLIENT_CERT_TRUST:
    case NEU_REQ_CLIENT_CERT_INFO:
        ret = handle_cert_messages(plugin, head, data);
        break;

    // Security policy related messages
    case NEU_REQ_SERVER_SECURITY_POLICY:
    case NEU_REQ_SERVER_SECURITY_POLICY_STATUS:
        ret = handle_security_policy_messages(plugin, head, data);
        break;

    // Authentication related messages
    case NEU_REQ_SERVER_AUTH_SWITCH:
    case NEU_REQ_SERVER_AUTH_SWITCH_STATUS:
    case NEU_REQ_SERVER_AUTH_USER_ADD:
    case NEU_REQ_SERVER_AUTH_USER_DELETE:
    case NEU_REQ_SERVER_AUTH_USER_UPDATE_PWD:
    case NEU_REQ_SERVER_AUTH_USER_INFO:
        ret = handle_auth_messages(plugin, head, data);
        break;

    default:
        plog_warn(plugin, "Unhandled request type: %s",
                  neu_reqresp_type_string(head->type));
        ret = NEU_ERR_EINTERNAL;
        break;
    }

    return ret;
}

// Certificate messages handler
static int handle_cert_messages(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                                void *data)
{
    (void) data; // Suppress unused parameter warning
    int ret = NEU_ERR_SUCCESS;

    switch (head->type) {
    case NEU_REQ_SERVER_CERT_SELF_SIGN:
    case NEU_REQ_SERVER_CERT_UPLOAD: {
        plog_info(plugin, "Processing NEU_REQ_SERVER_CERT_UPLOAD");
        neu_req_server_cert_upload_t *req = data;

        neu_persist_server_cert_info_t cert_info = { 0 };

        cert_info.certificate_data = strdup(server_cert);
        cert_info.certificate_size = strlen(server_cert);
        cert_info.private_key_data = strdup(server_private_key);
        cert_info.private_key_size = strlen(server_private_key);

        cert_info.common_name   = strdup(subject);
        cert_info.subject       = strdup(subject);
        cert_info.issuer        = strdup(issuer);
        cert_info.valid_from    = get_current_timestamp();
        cert_info.valid_to      = get_current_timestamp();
        cert_info.serial_number = strdup("1");
        cert_info.fingerprint   = strdup("SHA256:demo_fingerprint");
        cert_info.created_at    = get_current_timestamp();
        cert_info.updated_at    = get_current_timestamp();

        cert_info.app_name = strdup(req->app_name);

        neu_persister_store_server_cert(&cert_info);

        neu_persister_update_server_cert(&cert_info);

        neu_persist_server_cert_info_fini(&cert_info);

        ret = NEU_ERR_SUCCESS;

        break;
    }
    case NEU_REQ_SERVER_CERT_INFO: {
        plog_info(plugin, "Mock handling NEU_REQ_SERVER_CERT_INFO");
        neu_req_server_cert_data_t *req = data;

        head->type = NEU_RESP_SERVER_CERT_INFO;

        neu_persist_server_cert_info_t *cert_info = NULL;
        neu_persister_load_server_cert(req->app_name, &cert_info);

        head->type = NEU_RESP_SERVER_CERT_INFO;

        neu_resp_server_cert_data_t resp = { 0 };

        strncpy(resp.app_name, req->app_name, sizeof(resp.app_name) - 1);

        if (cert_info->subject) {
            resp.subject = strdup(cert_info->subject);
        }

        if (cert_info->issuer) {
            resp.issuer = strdup(cert_info->issuer);
        }

        if (cert_info->valid_to) {
            resp.expire = strdup(cert_info->valid_to);
        }

        if (cert_info->fingerprint) {
            resp.fingerprint = strdup(cert_info->fingerprint);
        }

        plog_notice(plugin,
                    "returning certificate info for app: %s, subject: %s",
                    req->app_name, resp.subject ? resp.subject : "N/A");

        // Send response
        ret = neu_plugin_op(plugin, *head, &resp);

        // Clean up certificate info
        neu_persist_server_cert_info_fini(cert_info);
        free(cert_info);
        break;
    }

    case NEU_REQ_SERVER_CERT_EXPORT: {
        plog_info(plugin, "Mock handling NEU_REQ_SERVER_CERT_EXPORT");
        // Mock response with dummy cert export
        neu_req_server_cert_export_t *  req       = data;
        neu_persist_server_cert_info_t *cert_info = NULL;
        neu_persister_load_server_cert(req->app_name, &cert_info);

        // Create response message
        head->type = NEU_RESP_SERVER_CERT_EXPORT;

        neu_resp_server_cert_export_t resp = { 0 };

        // Set app_name
        strncpy(resp.app_name, req->app_name, sizeof(resp.app_name) - 1);

        // Encode certificate data to base64
        if (cert_info->certificate_data && cert_info->certificate_size > 0) {
            resp.cert_base64 = neu_encode64(cert_info->certificate_data,
                                            cert_info->certificate_size);
            if (!resp.cert_base64) {
                plog_error(plugin,
                           "failed to encode certificate data to base64");
                neu_persist_server_cert_info_fini(cert_info);
                free(cert_info);
                return NEU_ERR_EINTERNAL;
            }
        } else {
            plog_error(plugin, "no certificate data found for app: %s",
                       req->app_name);
            neu_persist_server_cert_info_fini(cert_info);
            free(cert_info);
            return NEU_ERR_EINTERNAL;
        }

        plog_notice(plugin,
                    "exporting certificate for app: %s, data size: %zu bytes",
                    req->app_name, cert_info->certificate_size);

        // Send response
        ret = neu_plugin_op(plugin, *head, &resp);

        // Clean up certificate info
        neu_persist_server_cert_info_fini(cert_info);
        free(cert_info);
        break;
    }

    case NEU_REQ_CLIENT_CERT_UPLOAD: {
        plog_info(plugin, "Mock handling NEU_REQ_CLIENT_CERT_UPLOAD");
        // Mock response: always success

        neu_req_client_cert_upload_t *req = data;

        neu_persist_client_cert_info_t cert_info = { 0 };

        cert_info.certificate_data = strdup(server_cert);
        cert_info.certificate_size = strlen(server_cert);
        cert_info.common_name      = strdup("Mock Client Cert");
        cert_info.subject          = strdup(subject);
        cert_info.issuer           = strdup(issuer);
        cert_info.valid_from       = get_current_timestamp();
        cert_info.valid_to         = get_current_timestamp();
        cert_info.serial_number    = strdup("1");
        cert_info.fingerprint      = strdup("SHA256:mock_fingerprint");
        cert_info.created_at       = get_current_timestamp();
        cert_info.app_name         = strdup(req->app_name);

        ret = neu_persister_store_client_cert(&cert_info);

        neu_persist_client_cert_info_fini(&cert_info);

        break;
    }

    case NEU_REQ_CLIENT_CERT_DELETE: {
        plog_info(plugin, "Mock handling NEU_REQ_CLIENT_CERT_DELETE");
        // Mock response: always success
        neu_req_client_cert_del_t *req = data;

        ret = neu_persister_delete_client_cert(req->fingerprint);

        break;
    }

    case NEU_REQ_CLIENT_CERT_TRUST: {
        plog_info(plugin, "Mock handling NEU_REQ_CLIENT_CERT_TRUST");
        // Mock response: always success

        neu_req_client_cert_trust_t *req = data;

        neu_persist_client_cert_info_t target_cert = { 0 };

        target_cert.trust_status = 1;
        target_cert.fingerprint  = req->fingerprint;

        // Update certificate in database
        ret = neu_persister_update_client_cert(&target_cert);

        break;
    }

    case NEU_REQ_CLIENT_CERT_INFO: {
        plog_info(plugin, "Mock handling NEU_REQ_CLIENT_CERT_INFO");
        // Mock response with dummy client cert info
        neu_req_client_cert_data_t *req = data;

        head->type = NEU_RESP_CLIENT_CERT_INFO;

        neu_resp_client_certs_data_t resp = { 0 };

        strncpy(resp.app_name, req->app_name, sizeof(resp.app_name) - 1);

        UT_icd cert_icd = { sizeof(neu_resp_client_cert_data_t), NULL, NULL,
                            NULL };
        utarray_new(resp.certs, &cert_icd);

        neu_resp_client_cert_data_t cert_data = { 0 };

        // Copy certificate information

        cert_data.subject = strdup(subject);

        cert_data.issuer = strdup(issuer);

        cert_data.expire = get_current_timestamp();

        cert_data.fingerprint = strdup("SHA256:mock_fingerprint");

        cert_data.ca           = 0;
        cert_data.trust_status = 1;

        // Add to response array
        utarray_push_back(resp.certs, &cert_data);

        ret = neu_plugin_op(plugin, *head, &resp);

        break;
    }

    default:
        plog_warn(plugin, "Unhandled certificate message type: %s",
                  neu_reqresp_type_string(head->type));
        ret = NEU_ERR_EINTERNAL;
        break;
    }

    return ret;
}

// Security policy messages handler
static int handle_security_policy_messages(neu_plugin_t *      plugin,
                                           neu_reqresp_head_t *head, void *data)
{
    int ret = NEU_ERR_SUCCESS;
    (void) data; // Suppress unused parameter warning

    switch (head->type) {
    case NEU_REQ_SERVER_SECURITY_POLICY: {
        plog_info(plugin, "Mock handling NEU_REQ_SERVER_SECURITY_POLICY");
        // Mock response: always success
        neu_req_server_security_policy_t * req         = data;
        neu_persist_security_policy_info_t policy_info = { 0 };
        policy_info.app_name                           = strdup(req->app_name);
        policy_info.policy_name = strdup(req->policy_name);
        policy_info.created_at  = get_current_timestamp();
        policy_info.updated_at  = get_current_timestamp();

        neu_persister_store_security_policy(&policy_info);

        neu_persist_security_policy_info_fini(&policy_info);

        ret = NEU_ERR_SUCCESS;

        break;
    }

    case NEU_REQ_SERVER_SECURITY_POLICY_STATUS: {
        plog_info(plugin,
                  "Mock handling NEU_REQ_SERVER_SECURITY_POLICY_STATUS");
        // Mock response with security policy status

        neu_req_server_security_policy_status_t *req = data;
        head->type = NEU_RESP_SERVER_SECURITY_POLICY_STATUS;

        neu_resp_server_security_policy_status_t resp = { 0 };

        // Set app_name
        strncpy(resp.app_name, req->app_name, sizeof(resp.app_name) - 1);

        // Try to load security policy from database first
        neu_persist_security_policy_info_t *policy_info = NULL;
        neu_persister_load_security_policy(req->app_name, &policy_info);

        if (policy_info && policy_info->policy_name) {
            resp.policy_name = strdup(policy_info->policy_name);
        } else {
            resp.policy_name = strdup("None");
        }

        ret = neu_plugin_op(plugin, *head, &resp);

        if (policy_info) {
            neu_persist_security_policy_info_fini(policy_info);
            free(policy_info);
        }

        break;
    }

    default:
        plog_warn(plugin, "Unhandled security policy message type: %s",
                  neu_reqresp_type_string(head->type));
        ret = NEU_ERR_EINTERNAL;
        break;
    }

    return ret;
}

// Authentication messages handler
static int handle_auth_messages(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                                void *data)
{
    int ret = NEU_ERR_SUCCESS;
    (void) data; // Suppress unused parameter warning

    switch (head->type) {
    case NEU_REQ_SERVER_AUTH_SWITCH: {
        plog_info(plugin, "Mock handling NEU_REQ_SERVER_AUTH_SWITCH");
        // Mock response: always success
        neu_req_server_auth_switch_t *req = data;

        neu_persist_auth_setting_info_t auth_info = { 0 };
        auth_info.app_name                        = strdup(req->app_name);
        auth_info.enabled                         = req->enable ? 1 : 0;

        auth_info.created_at = get_current_timestamp();
        auth_info.updated_at = get_current_timestamp();

        neu_persister_store_auth_setting(&auth_info);

        neu_persister_update_auth_setting(&auth_info);

        neu_persist_auth_setting_info_fini(&auth_info);

        ret = NEU_ERR_SUCCESS;

        break;
    }

    case NEU_REQ_SERVER_AUTH_SWITCH_STATUS: {
        plog_info(plugin, "Mock handling NEU_REQ_SERVER_AUTH_SWITCH_STATUS");
        // Mock response with auth switch status
        neu_req_server_auth_switch_status_t *req       = data;
        neu_persist_auth_setting_info_t *    auth_info = NULL;
        int db_ret = neu_persister_load_auth_setting(req->app_name, &auth_info);

        neu_resp_server_auth_switch_status_t resp_data = { 0 };
        strncpy(resp_data.app_name, req->app_name,
                sizeof(resp_data.app_name) - 1);

        if (db_ret == 0 && auth_info) {
            // Found auth setting in database
            resp_data.enable = (auth_info->enabled == 1) ? true : false;
            plog_info(plugin,
                      "Found auth setting in database for app: %s, enabled: %s",
                      req->app_name, resp_data.enable ? "true" : "false");

            // Free auth info
            neu_persist_auth_setting_info_fini(auth_info);
            free(auth_info);
        } else {
            // No auth setting found in database, use plugin state or default
            resp_data.enable = true;

            plog_info(
                plugin,
                "No auth setting found in database for app: %s, using plugin "
                "state: %s",
                req->app_name, resp_data.enable ? "true" : "false");
        }

        // Send response
        head->type = NEU_RESP_SERVER_AUTH_SWITCH_STATUS;

        ret = neu_plugin_op(plugin, *head, &resp_data);
        break;
    }

    case NEU_REQ_SERVER_AUTH_USER_ADD: {
        plog_info(plugin, "Mock handling NEU_REQ_SERVER_AUTH_USER_ADD");
        // Mock response: always success

        neu_req_server_auth_user_add_t *req       = data;
        neu_persist_auth_user_info_t    auth_user = { 0 };
        auth_user.app_name                        = strdup(req->app_name);
        auth_user.username                        = strdup(req->user);
        auth_user.password_hash                   = strdup(req->pwd);

        auth_user.created_at = get_current_timestamp();
        auth_user.updated_at = get_current_timestamp();
        // Store user in database
        neu_persister_store_auth_user(&auth_user);

        neu_persist_auth_user_info_t *auth_user0 = NULL;

        neu_persister_load_auth_user(req->app_name, req->user, &auth_user0);

        neu_persist_auth_user_info_fini(&auth_user);
        if (auth_user0) {
            neu_persist_auth_user_info_fini(auth_user0);
            free(auth_user0);
        }

        ret = NEU_ERR_SUCCESS;

        break;
    }

    case NEU_REQ_SERVER_AUTH_USER_DELETE: {
        plog_info(plugin, "Mock handling NEU_REQ_SERVER_AUTH_USER_DELETE");
        // Mock response: always success
        neu_req_server_auth_user_del_t *req = data;
        ret = neu_persister_delete_auth_user(req->app_name, req->user);
        break;
    }

    case NEU_REQ_SERVER_AUTH_USER_UPDATE_PWD: {
        plog_info(plugin, "Mock handling NEU_REQ_SERVER_AUTH_USER_UPDATE_PWD");
        // Mock response: always success
        neu_req_server_auth_user_update_t *req  = data;
        neu_persist_auth_user_info_t       user = { 0 };
        user.app_name                           = strdup(req->app_name);
        user.username                           = strdup(req->user);
        user.password_hash                      = strdup(req->new_pwd);
        user.updated_at                         = get_current_timestamp();

        ret = neu_persister_update_auth_user(&user);

        neu_persist_auth_user_info_fini(&user);

        break;
    }

    case NEU_REQ_SERVER_AUTH_USER_INFO: {
        plog_info(plugin, "Mock handling NEU_REQ_SERVER_AUTH_USER_INFO");
        // Mock response with user info
        neu_req_server_auth_users_info_t *req = data;

        UT_array *user_infos = NULL;
        neu_persister_load_auth_users_by_app(req->app_name, &user_infos);

        // Create response structure
        neu_resp_server_auth_users_info_t resp = { 0 };
        strcpy(resp.app_name, req->app_name);

        // Initialize user array
        UT_icd user_icd = { sizeof(neu_server_auth_user_t), NULL, NULL, NULL };
        utarray_new(resp.users, &user_icd);

        if (user_infos != NULL) {
            // Convert loaded user info to response format
            utarray_foreach(user_infos, neu_persist_auth_user_info_t *,
                            p_user_info)
            {
                if (p_user_info != NULL) {
                    neu_server_auth_user_t user = { 0 };

                    // Copy username
                    user.user = strdup(p_user_info->username);
                    // Mask password in response
                    user.pwd_mask = strdup("******");

                    utarray_push_back(resp.users, &user);
                }
            }

            // Free loaded user infos
            utarray_free(user_infos);
        }

        // Send successful response
        head->type = NEU_RESP_SERVER_AUTH_USER_INFO;

        ret = neu_plugin_op(plugin, *head, &resp);

        break;
    }

    default:
        plog_warn(plugin, "Unhandled authentication message type: %s",
                  neu_reqresp_type_string(head->type));
        ret = NEU_ERR_EINTERNAL;
        break;
    }

    return ret;
}

static char *get_current_timestamp(void)
{
    time_t     now;
    struct tm *tm_info;
    char *     timestamp = malloc(32);

    if (!timestamp) {
        return NULL;
    }

    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, 32, "%Y-%m-%d %H:%M:%S", tm_info);

    return timestamp;
}