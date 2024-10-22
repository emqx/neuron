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

#ifndef _NEU_UTILS_CID_H_
#define _NEU_UTILS_CID_H_

#include "define.h"
#include "msg.h"

typedef enum {
    CO      = 0,
    ST      = 1, // boolean
    MX      = 2, // float
    UNKNOWN = 100,
} cid_fc_e;

typedef struct {
    char     name[NEU_CID_DO_NAME_LEN];
    char     lnclass[NEU_CID_LNCLASS_LEN];
    cid_fc_e fc;
} cid_do_t;

typedef struct {
    char      name[NEU_CID_DATASET_NAME_LEN];
    cid_do_t *dos;
    int       n_dos;
} cid_dataset_t;

typedef struct {
    char name[NEU_CID_REPORT_NAME_LEN];
    char id[NEU_CID_REPORT_ID_LEN];
    char dataset[NEU_CID_DATASET_NAME_LEN];
    bool buffered;
    int  intg_pd;
} cid_report_t;

typedef struct {
    char do_name[NEU_CID_DO_NAME_LEN];
    char sdi_name[NEU_CID_SDI_NAME_LEN];
} cid_ctrl_t;

typedef struct {
    char lnclass[NEU_CID_LNCLASS_LEN];
    char lntype[NEU_CID_LNTYPE_LEN];

    cid_dataset_t *datasets;
    int            n_datasets;

    cid_report_t *reports;
    int           n_reports;

    cid_ctrl_t *ctrls;
    int         n_ctrls;
} cid_ln_t;

typedef struct {
    char      inst[NEU_CID_INST_LEN];
    cid_ln_t *lns;
    int       n_lns;
} cid_ldevice_t;

typedef struct {
    char           name[NEU_CID_IED_NAME_LEN];
    cid_ldevice_t *ldevices;
    int            n_ldevices;
} cid_ied_t;

typedef enum {
    SPC = 0,
    DPC = 1,
} cid_tm_cdc_e;

typedef struct {
    char     name[NEU_CID_DA_NAME_LEN];
    cid_fc_e fc;
} cid_tm_da_t;

typedef struct {
    char         id[NEU_CID_DO_ID_LEN];
    cid_tm_cdc_e cdc;
    cid_tm_da_t *das;
    int          n_das;
} cid_tm_do_t;

typedef struct {
    char name[NEU_CID_LNO_NAME_LEN];
    char type[NEU_CID_LNO_TYPE_LEN];
} cid_tm_lno_do_t;

typedef struct {
    char             id[NEU_CID_DO_ID_LEN];
    cid_tm_lno_do_t *dos;
    int              n_dos;
} cid_tm_lno_t;

typedef struct {
    cid_tm_do_t *dos;
    int          n_dos;

    cid_tm_lno_t *lnos;
    int           n_lnos;
} cid_template_t;

typedef struct {
    cid_ied_t ied;
    cid_template_t template;
} cid_t;

typedef struct {
    bool control;
    char ied_name[NEU_CID_IED_NAME_LEN];
    char ldevice_inst[NEU_CID_LDEVICE_LEN];
    char ln_class[NEU_CID_LNCLASS_LEN];
    bool buffered;
    char report_name[NEU_CID_REPORT_NAME_LEN];
    char report_id[NEU_CID_REPORT_ID_LEN];
    char dataset_name[NEU_CID_DATASET_NAME_LEN];
} cid_dataset_info_t;

int  neu_cid_parse(const char *path, cid_t *cid);
void neu_cid_free(cid_t *cid);

void neu_cid_to_msg(char *driver, cid_t *cid, neu_req_add_gtag_t *cmd);

char *              neu_cid_info_to_string(cid_dataset_info_t *info);
cid_dataset_info_t *neu_cid_info_from_string(const char *str);

#endif