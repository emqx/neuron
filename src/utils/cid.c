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

#include <jansson.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "define.h"
#include "utils/cid.h"
#include "utils/log.h"
#include "json/json.h"

static int parse_ied(xmlNode *xml_ied, cid_ied_t *ied,
                     cid_template_t *template);
static int parse_template(xmlNode *xml_template, cid_template_t *template);
static int parse_ldevice(xmlNode *xml_server, cid_ied_t *ied,
                         cid_template_t *template);
static int parse_lnode(xmlNode *xml_ldevice, cid_ldevice_t *ldev,
                       cid_template_t *template);
static int parse_dataset(xmlNode *xml_dataset, cid_ln_t *ln);
static int parse_report(xmlNode *xml_report, cid_ln_t *ln);
static int parse_doi(xmlNode *xml_doi, cid_ln_t *ln, cid_template_t *template,
                     int index);

int neu_cid_parse(const char *path, cid_t *cid)
{
    memset(cid, 0, sizeof(cid_t));
    xmlDoc *doc = xmlReadFile(path, NULL, 0);
    if (doc == NULL) {
        nlog_warn("Failed to read icd file %s", path);
        return -1;
    }
    xmlNode *root = xmlDocGetRootElement(doc);
    if (root == NULL || strcmp((const char *) root->name, "SCL") != 0) {
        nlog_warn("Failed to get root element(SCL)");
        xmlFreeDoc(doc);
        return -1;
    }

    xmlNode *find             = root->children;
    bool     ied_parsed       = false;
    bool     templates_parsed = false;
    while (find != NULL) {
        if (find->type == XML_ELEMENT_NODE &&
            strcmp((const char *) find->name, "DataTypeTemplates") == 0) {
            int ret = parse_template(find, &cid->template);
            if (ret == 0) {
                templates_parsed = true;
            }
        }

        find = find->next;
    }
    xmlNode *find_ied = root->children;
    while (find_ied != NULL) {
        if (find_ied->type == XML_ELEMENT_NODE &&
            strcmp((const char *) find_ied->name, "IED") == 0) {
            int ret = parse_ied(find_ied, &cid->ied, &cid->template);
            if (ret == 0) {
                ied_parsed = true;
            }
        }

        find_ied = find_ied->next;
    }

    if (ied_parsed && templates_parsed) {
        xmlFreeDoc(doc);
        return 0;
    } else {
        neu_cid_free(cid);
        nlog_warn("Failed to parse IED or DataTypeTemplates, %d, %d",
                  ied_parsed, templates_parsed);
        xmlFreeDoc(doc);
        return -1;
    }
}

void neu_cid_free(cid_t *cid)
{
    for (int i = 0; i < cid->template.n_lnos; i++) {
        if (cid->template.lnos[i].n_dos > 0) {
            free(cid->template.lnos[i].dos);
        }
    }
    if (cid->template.n_lnos > 0) {
        free(cid->template.lnos);
    }

    for (int i = 0; i < cid->template.n_dos; i++) {
        if (cid->template.dos[i].n_das > 0) {
            free(cid->template.dos[i].das);
        }
    }
    if (cid->template.n_dos > 0) {
        free(cid->template.dos);
    }

    for (int i = 0; i < cid->ied.n_ldevices; i++) {
        for (int j = 0; j < cid->ied.ldevices[i].n_lns; j++) {
            if (cid->ied.ldevices[i].lns[j].n_reports > 0) {
                free(cid->ied.ldevices[i].lns[j].reports);
            }
            if (cid->ied.ldevices[i].lns[j].n_ctrls > 0) {
                free(cid->ied.ldevices[i].lns[j].ctrls);
            }

            for (int k = 0; k < cid->ied.ldevices[i].lns[j].n_datasets; k++) {
                if (cid->ied.ldevices[i].lns[j].datasets[k].n_dos > 0) {
                    free(cid->ied.ldevices[i].lns[j].datasets[k].dos);
                }
            }
            if (cid->ied.ldevices[i].lns[j].n_datasets > 0) {
                free(cid->ied.ldevices[i].lns[j].datasets);
            }
        }

        if (cid->ied.ldevices[i].n_lns > 0) {
            free(cid->ied.ldevices[i].lns);
        }
    }
    if (cid->ied.n_ldevices > 0) {
        free(cid->ied.ldevices);
    }

    return;
}

void neu_cid_to_msg(char *driver, cid_t *cid, neu_req_add_gtag_t *cmd)
{
    strcpy(cmd->driver, driver);
    cmd->n_group = 0;
    cmd->groups  = NULL;

    cmd->n_group += 1;
    cmd->groups = realloc(cmd->groups, cmd->n_group * sizeof(neu_gdatatag_t));
    neu_gdatatag_t *ctl_group = &cmd->groups[cmd->n_group - 1];
    memset(ctl_group, 0, sizeof(neu_gdatatag_t));

    strcpy(ctl_group->group, "Control");
    ctl_group->interval = 5000;

    cid_dataset_info_t *ctl_info = calloc(1, sizeof(cid_dataset_info_t));
    ctl_info->control            = true;
    strcpy(ctl_info->ied_name, cid->ied.name);
    ctl_group->context = ctl_info;

    for (int i = 0; i < cid->ied.n_ldevices; i++) {
        for (int j = 0; j < cid->ied.ldevices[i].n_lns; j++) {
            for (int k = 0; k < cid->ied.ldevices[i].lns[j].n_ctrls; k++) {
                ctl_group->n_tag += 1;
                ctl_group->tags = realloc(
                    ctl_group->tags, ctl_group->n_tag * sizeof(neu_datatag_t));
                neu_datatag_t *tag = &ctl_group->tags[ctl_group->n_tag - 1];
                memset(tag, 0, sizeof(neu_datatag_t));

                tag->name = calloc(1, NEU_TAG_NAME_LEN);
                snprintf(tag->name, NEU_TAG_NAME_LEN - 1, "%s/%s$%s",
                         cid->ied.ldevices[i].inst,
                         cid->ied.ldevices[i].lns[j].ctrls[k].do_name,
                         cid->ied.ldevices[i].lns[j].ctrls[k].sdi_name);

                tag->attribute   = NEU_ATTRIBUTE_WRITE;
                tag->type        = NEU_TYPE_BOOL;
                tag->address     = calloc(1, NEU_TAG_ADDRESS_LEN);
                tag->description = strdup("");
                snprintf(tag->address, NEU_TAG_ADDRESS_LEN - 1,
                         "%s%s/%s$%s$%s$%s", cid->ied.name,
                         cid->ied.ldevices[i].inst,
                         cid->ied.ldevices[i].lns[j].lnclass, "CO",
                         cid->ied.ldevices[i].lns[j].ctrls[k].do_name,
                         cid->ied.ldevices[i].lns[j].ctrls[k].sdi_name);
            }
        }
    }

    for (int i = 0; i < cid->ied.n_ldevices; i++) {
        for (int j = 0; j < cid->ied.ldevices[i].n_lns; j++) {
            for (int k = 0; k < cid->ied.ldevices[i].lns[j].n_datasets; k++) {
                for (int l = 0; l < cid->ied.ldevices[i].lns[j].n_reports;
                     l++) {
                    if (strcmp(
                            cid->ied.ldevices[i].lns[j].datasets[k].name,
                            cid->ied.ldevices[i].lns[j].reports[l].dataset) ==
                        0) {
                        if (cid->ied.ldevices[i].lns[j].datasets[k].n_dos ==
                            0) {
                            break;
                        }
                        cmd->n_group += 1;
                        cmd->groups = realloc(
                            cmd->groups, cmd->n_group * sizeof(neu_gdatatag_t));
                        neu_gdatatag_t *group = &cmd->groups[cmd->n_group - 1];
                        memset(group, 0, sizeof(neu_gdatatag_t));

                        cid_dataset_info_t *info =
                            calloc(1, sizeof(cid_dataset_info_t));

                        info->control = false;
                        info->buffered =
                            cid->ied.ldevices[i].lns[j].reports[l].buffered;
                        strcpy(info->ied_name, cid->ied.name);
                        strcpy(info->ldevice_inst, cid->ied.ldevices[i].inst);
                        strcpy(info->ln_class,
                               cid->ied.ldevices[i].lns[j].lnclass);
                        strcpy(info->report_name,
                               cid->ied.ldevices[i].lns[j].reports[l].name);
                        strcpy(info->report_id,
                               cid->ied.ldevices[i].lns[j].reports[l].id);
                        strcpy(info->dataset_name,
                               cid->ied.ldevices[i].lns[j].datasets[k].name);
                        group->context = info;

                        snprintf(group->group, NEU_GROUP_NAME_LEN - 1,
                                 "%s_%s_%s", cid->ied.ldevices[i].inst,
                                 cid->ied.ldevices[i].lns[j].lnclass,
                                 cid->ied.ldevices[i].lns[j].datasets[k].name);
                        group->interval =
                            cid->ied.ldevices[i].lns[j].reports[l].intg_pd;
                        group->n_tag =
                            cid->ied.ldevices[i].lns[j].datasets[k].n_dos;
                        group->tags =
                            calloc(group->n_tag, sizeof(neu_datatag_t));

                        for (int m = 0;
                             m < cid->ied.ldevices[i].lns[j].datasets[k].n_dos;
                             m++) {
                            group->tags[m].name = strdup(cid->ied.ldevices[i]
                                                             .lns[j]
                                                             .datasets[k]
                                                             .dos[m]
                                                             .name);
                            group->tags[m].attribute =
                                NEU_ATTRIBUTE_READ | NEU_ATTRIBUTE_SUBSCRIBE;
                            group->tags[m].address =
                                calloc(1, NEU_TAG_ADDRESS_LEN);
                            group->tags[m].description = strdup("");
                            switch (cid->ied.ldevices[i]
                                        .lns[j]
                                        .datasets[k]
                                        .dos[m]
                                        .fc) {
                            case ST:
                                group->tags[m].type = NEU_TYPE_BOOL;
                                snprintf(group->tags[m].address,
                                         NEU_TAG_ADDRESS_LEN - 1,
                                         "%s%s/%s$%s$%s", cid->ied.name,
                                         cid->ied.ldevices[i].inst,
                                         cid->ied.ldevices[i]
                                             .lns[j]
                                             .datasets[k]
                                             .dos[m]
                                             .lnclass,
                                         "ST",
                                         cid->ied.ldevices[i]
                                             .lns[j]
                                             .datasets[k]
                                             .dos[m]
                                             .name);
                                break;
                            case MX:
                                group->tags[m].type = NEU_TYPE_FLOAT;
                                snprintf(group->tags[m].address,
                                         NEU_TAG_ADDRESS_LEN - 1,
                                         "%s%s/%s$%s$%s", cid->ied.name,
                                         cid->ied.ldevices[i].inst,
                                         cid->ied.ldevices[i]
                                             .lns[j]
                                             .datasets[k]
                                             .dos[m]
                                             .lnclass,
                                         "MX",
                                         cid->ied.ldevices[i]
                                             .lns[j]
                                             .datasets[k]
                                             .dos[m]
                                             .name);
                                break;
                            default:
                                group->tags[m].type = NEU_TYPE_INT16;
                                break;
                            }
                        }

                        break;
                    }
                }
            }
        }
    }
}

static int parse_ied(xmlNode *xml_ied, cid_ied_t *ied, cid_template_t *template)
{
    int   ret      = -1;
    char *ied_name = (char *) xmlGetProp(xml_ied, (const xmlChar *) "name");
    if (strlen(ied_name) >= NEU_CID_IED_NAME_LEN) {
        xmlFree(ied_name);
        nlog_warn("IED name is too long");
        return -1;
    } else {
        strcpy(ied->name, ied_name);
        xmlFree(ied_name);
    }
    xmlNode *access_point = xml_ied->children;
    while (access_point != NULL) {
        if (access_point->type == XML_ELEMENT_NODE &&
            strcmp((const char *) access_point->name, "AccessPoint") == 0) {
            xmlNode *server = access_point->children;
            while (server != NULL) {
                if (server->type == XML_ELEMENT_NODE &&
                    strcmp((const char *) server->name, "Server") == 0) {
                    ret = parse_ldevice(server, ied, template);
                    break;
                }

                server = server->next;
            }
            break;
        }
        access_point = access_point->next;
    }

    return ret;
}

static int parse_ldevice(xmlNode *xml_server, cid_ied_t *ied,
                         cid_template_t *template)
{
    ied->ldevices    = NULL;
    ied->n_ldevices  = 0;
    xmlNode *ldevice = xml_server->children;

    while (ldevice != NULL) {
        if (ldevice->type == XML_ELEMENT_NODE &&
            strcmp((const char *) ldevice->name, "LDevice") == 0) {
            ied->n_ldevices++;
            ied->ldevices =
                realloc(ied->ldevices, ied->n_ldevices * sizeof(cid_ldevice_t));
            cid_ldevice_t *ldev = &ied->ldevices[ied->n_ldevices - 1];
            ldev->lns           = NULL;
            ldev->n_lns         = 0;
            memset(ldev->inst, 0, sizeof(ldev->inst));

            char *inst = (char *) xmlGetProp(ldevice, (const xmlChar *) "inst");
            if (strlen(inst) >= NEU_CID_INST_LEN) {
                xmlFree(inst);
                nlog_warn("LDevice inst is too long");
                return -1;
            } else {
                strcpy(ldev->inst, inst);
                xmlFree(inst);
            }

            int ret = parse_lnode(ldevice, ldev, template);
            if (ret != 0) {
                return ret;
            }
        }

        ldevice = ldevice->next;
    }

    return 0;
}

static int parse_lnode(xmlNode *xml_ldevice, cid_ldevice_t *ldev,
                       cid_template_t *template)
{
    xmlNode *lnode = xml_ldevice->children;
    while (lnode != NULL) {
        if (lnode->type == XML_ELEMENT_NODE &&
            strncmp((const char *) lnode->name, "LN", 2) == 0) {
            char *ln_class =
                (char *) xmlGetProp(lnode, (const xmlChar *) "lnClass");
            char *ln_type =
                (char *) xmlGetProp(lnode, (const xmlChar *) "lnType");
            if (strlen(ln_class) >= NEU_CID_LNCLASS_LEN ||
                strlen(ln_type) >= NEU_CID_LNTYPE_LEN) {
                xmlFree(ln_class);
                xmlFree(ln_type);
                nlog_warn("LN class/type is too long %d", (int) lnode->line);
                return -1;
            } else {
                ldev->n_lns += 1;
                ldev->lns = realloc(ldev->lns, ldev->n_lns * sizeof(cid_ln_t));
                cid_ln_t *ln = &ldev->lns[ldev->n_lns - 1];
                memset(ln->lnclass, 0, sizeof(ln->lnclass));
                memset(ln->lntype, 0, sizeof(ln->lntype));
                ln->datasets   = NULL;
                ln->n_datasets = 0;
                ln->reports    = NULL;
                ln->n_reports  = 0;
                ln->ctrls      = NULL;
                ln->n_ctrls    = 0;

                strcpy(ln->lnclass, ln_class);
                strcpy(ln->lntype, ln_type);
                xmlFree(ln_class);
                xmlFree(ln_type);

                xmlNode *child = lnode->children;
                while (child != NULL) {
                    if (child->type == XML_ELEMENT_NODE &&
                        strcmp((const char *) child->name, "DataSet") == 0) {
                        int ret = parse_dataset(child, ln);
                        if (ret != 0) {
                            return ret;
                        }
                    }

                    if (child->type == XML_ELEMENT_NODE &&
                        strcmp((const char *) child->name, "ReportControl") ==
                            0) {
                        int ret = parse_report(child, ln);
                        if (ret != 0) {
                            return ret;
                        }
                    }

                    if (child->type == XML_ELEMENT_NODE &&
                        strcmp((const char *) child->name, "DOI") == 0) {
                        for (int i = 0; i < template->n_lnos; i++) {
                            if (strcmp(template->lnos[i].id, ln->lntype) == 0 &&
                                template->lnos[i].n_dos > 0) {
                                int ret = parse_doi(child, ln, template, i);
                                if (ret != 0) {
                                    return ret;
                                }
                                break;
                            }
                        }
                    }

                    child = child->next;
                }
            }
        }

        lnode = lnode->next;
    }

    return 0;
}

static int parse_dataset(xmlNode *xml_dataset, cid_ln_t *ln)
{
    char *dataset_name =
        (char *) xmlGetProp(xml_dataset, (const xmlChar *) "name");
    if (strlen(dataset_name) >= NEU_CID_DATASET_NAME_LEN) {
        xmlFree(dataset_name);
        nlog_warn("Dataset name is too long %d", (int) xml_dataset->line);
        return -1;
    } else {
        ln->n_datasets += 1;
        ln->datasets =
            realloc(ln->datasets, ln->n_datasets * sizeof(cid_dataset_t));
        cid_dataset_t *dataset = &ln->datasets[ln->n_datasets - 1];
        dataset->n_dos         = 0;
        dataset->dos           = NULL;
        memset(dataset->name, 0, sizeof(dataset->name));

        strcpy(dataset->name, dataset_name);
        xmlFree(dataset_name);

        xmlNode *fcda = xml_dataset->children;
        while (fcda != NULL) {
            if (fcda->type == XML_ELEMENT_NODE &&
                strcmp((const char *) fcda->name, "FCDA") == 0) {
                dataset->n_dos += 1;
                dataset->dos =
                    realloc(dataset->dos, dataset->n_dos * sizeof(cid_do_t));
                cid_do_t *doi = &dataset->dos[dataset->n_dos - 1];
                memset(doi->name, 0, sizeof(doi->name));
                memset(doi->lnclass, 0, sizeof(doi->lnclass));

                char *do_name =
                    (char *) xmlGetProp(fcda, (const xmlChar *) "doName");
                char *lnclass =
                    (char *) xmlGetProp(fcda, (const xmlChar *) "lnClass");
                if (strlen(do_name) >= NEU_CID_DO_NAME_LEN ||
                    strlen(lnclass) >= NEU_CID_LNCLASS_LEN) {
                    xmlFree(do_name);
                    xmlFree(lnclass);
                    nlog_warn("DO name or lnclass is too long %d",
                              (int) fcda->line);
                    return -1;
                } else {
                    strcpy(doi->name, do_name);
                    strcpy(doi->lnclass, lnclass);
                    xmlFree(do_name);
                    xmlFree(lnclass);
                }

                char *fc = (char *) xmlGetProp(fcda, (const xmlChar *) "fc");
                if (fc != NULL && strcmp(fc, "ST") == 0) {
                    doi->fc = ST;
                } else if (fc != NULL && strcmp(fc, "MX") == 0) {
                    doi->fc = MX;
                } else {
                    doi->fc = UNKNOWN;
                    nlog_warn("FCDA unknown fc %s, %d", fc, (int) fcda->line);
                }

                xmlFree(fc);
            }

            fcda = fcda->next;
        }
    }
    return 0;
}

static int parse_report(xmlNode *xml_report, cid_ln_t *ln)
{
    char *report_name =
        (char *) xmlGetProp(xml_report, (const xmlChar *) "name");
    char *report_id =
        (char *) xmlGetProp(xml_report, (const xmlChar *) "rptID");
    char *dataset = (char *) xmlGetProp(xml_report, (const xmlChar *) "datSet");
    char *buffered =
        (char *) xmlGetProp(xml_report, (const xmlChar *) "buffered");
    char *intg_pd = (char *) xmlGetProp(xml_report, (const xmlChar *) "intgPd");

    if (strlen(report_name) >= NEU_CID_REPORT_NAME_LEN ||
        strlen(report_id) >= NEU_CID_REPORT_ID_LEN ||
        strlen(dataset) >= NEU_CID_DATASET_NAME_LEN) {
        xmlFree(report_name);
        xmlFree(report_id);
        xmlFree(dataset);
        xmlFree(buffered);
        xmlFree(intg_pd);

        nlog_warn("report name/id/datset too long %d", (int) xml_report->line);
        return -1;
    } else {
        ln->n_reports += 1;
        ln->reports =
            realloc(ln->reports, ln->n_reports * sizeof(cid_report_t));
        cid_report_t *report = &ln->reports[ln->n_reports - 1];
        memset(report->name, 0, sizeof(report->name));
        memset(report->id, 0, sizeof(report->id));
        memset(report->dataset, 0, sizeof(report->dataset));
        report->intg_pd  = 15000; // default
        report->buffered = false;

        strcpy(report->name, report_name);
        strcpy(report->id, report_id);
        strcpy(report->dataset, dataset);
        if (buffered != NULL && strcmp(buffered, "true") == 0) {
            report->buffered = true;
        }

        int interval = atoi(intg_pd);
        if (interval > 0) {
            report->intg_pd = interval;
        }

        xmlFree(report_name);
        xmlFree(report_id);
        xmlFree(dataset);
        xmlFree(buffered);
        xmlFree(intg_pd);
    }

    return 0;
}

static int parse_doi(xmlNode *xml_doi, cid_ln_t *ln, cid_template_t *template,
                     int index)
{
    char *doi_name = (char *) xmlGetProp(xml_doi, (const xmlChar *) "name");
    for (int i = 0; i < template->lnos[index].n_dos; i++) {
        if (strcmp(template->lnos[index].dos[i].name, doi_name) == 0) {
            for (int k = 0; k < template->n_dos; k++) {
                if (strcmp(template->dos[k].id,
                           template->lnos[index].dos[i].type) == 0) {
                    for (int j = 0; j < template->dos[k].n_das; j++) {
                        if (strcmp(template->dos[k].id,
                                   template->lnos[index].dos[i].type) == 0) {
                            ln->n_ctrls += 1;
                            ln->ctrls = realloc(
                                ln->ctrls, ln->n_ctrls * sizeof(cid_ctrl_t));
                            cid_ctrl_t *ctrl = &ln->ctrls[ln->n_ctrls - 1];
                            memset(ctrl->do_name, 0, sizeof(ctrl->do_name));
                            memset(ctrl->sdi_name, 0, sizeof(ctrl->sdi_name));

                            strcpy(ctrl->do_name, doi_name);
                            strcpy(ctrl->sdi_name,
                                   template->dos[k].das[j].name);
                        }
                    }
                    break;
                }
            }
            break;
        }
    }

    xmlFree(doi_name);

    return 0;
}

static int parse_template(xmlNode *xml_template, cid_template_t *template)
{
    template->dos    = NULL;
    template->lnos   = NULL;
    template->n_dos  = 0;
    template->n_lnos = 0;
    xmlNode *child   = xml_template->children;

    while (child != NULL) {
        if (child->type == XML_ELEMENT_NODE &&
            strcmp((const char *) child->name, "DOType") == 0) {
            char *id  = (char *) xmlGetProp(child, (const xmlChar *) "id");
            char *cdc = (char *) xmlGetProp(child, (const xmlChar *) "cdc");
            if (strlen(id) >= NEU_CID_DO_ID_LEN) {
                xmlFree(id);
                xmlFree(cdc);
                nlog_warn("DOType id is too long %d", (int) child->line);
                return -1;
            } else {
                if (strcmp(cdc, "SPC") == 0 || strcmp(cdc, "DPC") == 0) {
                    template->n_dos += 1;
                    template->dos = realloc(
                        template->dos, template->n_dos * sizeof(cid_tm_do_t));
                    cid_tm_do_t *tm_do = &template->dos[template->n_dos - 1];
                    memset(tm_do->id, 0, sizeof(tm_do->id));
                    tm_do->n_das = 0;
                    tm_do->das   = NULL;

                    strcpy(tm_do->id, id);
                    if (strcmp(cdc, "SPC") == 0) {
                        tm_do->cdc = SPC;
                    } else {
                        tm_do->cdc = DPC;
                    }

                    xmlNode *da = child->children;
                    while (da != NULL) {
                        if (da->type != XML_ELEMENT_NODE ||
                            strcmp((const char *) da->name, "DA") != 0) {
                            da = da->next;
                            continue;
                        }
                        char *fc =
                            (char *) xmlGetProp(da, (const xmlChar *) "fc");
                        if (fc != NULL && strcmp(fc, "CO") == 0) {
                            char *da_name = (char *) xmlGetProp(
                                da, (const xmlChar *) "name");
                            tm_do->n_das += 1;
                            tm_do->das = realloc(
                                tm_do->das, tm_do->n_das * sizeof(cid_tm_da_t));
                            cid_tm_da_t *tm_da = &tm_do->das[tm_do->n_das - 1];
                            memset(tm_da->name, 0, sizeof(tm_da->name));

                            tm_da->fc = CO;
                            strcpy(tm_da->name, da_name);

                            xmlFree(da_name);
                        }

                        xmlFree(fc);
                        da = da->next;
                    }
                }
                xmlFree(id);
                xmlFree(cdc);
            }
        }
        child = child->next;
    }

    xmlNode *child_node = xml_template->children;
    while (child_node != NULL) {
        if (child_node->type == XML_ELEMENT_NODE &&
            strcmp((const char *) child_node->name, "LNodeType") == 0) {
            char *id = (char *) xmlGetProp(child_node, (const xmlChar *) "id");

            template->n_lnos += 1;
            template->lnos       = realloc(template->lnos,
                                     template->n_lnos * sizeof(cid_tm_lno_t));
            cid_tm_lno_t *tm_lno = &template->lnos[template->n_lnos - 1];
            memset(tm_lno->id, 0, sizeof(tm_lno->id));
            tm_lno->n_dos = 0;
            tm_lno->dos   = NULL;

            strncpy(tm_lno->id, id, NEU_CID_DO_ID_LEN);
            xmlFree(id);
            xmlNode *dot = child_node->children;

            while (dot != NULL) {
                if (dot->type == XML_ELEMENT_NODE &&
                    strcmp((const char *) dot->name, "DO") == 0) {
                    char *do_type =
                        (char *) xmlGetProp(dot, (const xmlChar *) "type");
                    for (int i = 0; i < template->n_dos; i++) {
                        if (strcmp(template->dos[i].id, do_type) == 0) {
                            char *do_name = (char *) xmlGetProp(
                                dot, (const xmlChar *) "name");
                            tm_lno->n_dos += 1;
                            tm_lno->dos = realloc(tm_lno->dos,
                                                  tm_lno->n_dos *
                                                      sizeof(cid_tm_lno_do_t));
                            cid_tm_lno_do_t *tm_lno_do =
                                &tm_lno->dos[tm_lno->n_dos - 1];
                            memset(tm_lno_do->name, 0, sizeof(tm_lno_do->name));
                            memset(tm_lno_do->type, 0, sizeof(tm_lno_do->type));

                            strcpy(tm_lno_do->name, do_name);
                            strcpy(tm_lno_do->type, do_type);
                            xmlFree(do_name);
                            break;
                        }
                    }

                    xmlFree(do_type);
                }

                dot = dot->next;
            }
        }

        child_node = child_node->next;
    }

    return 0;
}

char *neu_cid_info_to_string(cid_dataset_info_t *info)
{
    neu_json_elem_t elems[] = {
        {
            .name       = "control",
            .t          = NEU_JSON_BOOL,
            .v.val_bool = info->control,
        },
        {
            .name       = "buffered",
            .t          = NEU_JSON_BOOL,
            .v.val_bool = info->buffered,
        },
        {
            .name      = "ied_name",
            .t         = NEU_JSON_STR,
            .v.val_str = info->ied_name,
        },
        {
            .name      = "ldevice_inst",
            .t         = NEU_JSON_STR,
            .v.val_str = info->ldevice_inst,
        },
        {
            .name      = "ln_class",
            .t         = NEU_JSON_STR,
            .v.val_str = info->ln_class,
        },
        {
            .name      = "report_name",
            .t         = NEU_JSON_STR,
            .v.val_str = info->report_name,
        },
        {
            .name      = "report_id",
            .t         = NEU_JSON_STR,
            .v.val_str = info->report_id,
        },
        {
            .name      = "dataset_name",
            .t         = NEU_JSON_STR,
            .v.val_str = info->dataset_name,
        },
    };

    char *result = NULL;
    void *json   = neu_json_encode_new();
    neu_json_encode_field(json, elems, NEU_JSON_ELEM_SIZE(elems));
    neu_json_encode(json, &result);
    neu_json_encode_free(json);

    return result;
}

cid_dataset_info_t *neu_cid_info_from_string(const char *str)
{
    neu_json_elem_t elems[] = {
        {
            .name = "control",
            .t    = NEU_JSON_BOOL,
        },
        {
            .name = "buffered",
            .t    = NEU_JSON_BOOL,
        },
        {
            .name = "ied_name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "ldevice_inst",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "ln_class",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "report_name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "report_id",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "dataset_name",
            .t    = NEU_JSON_STR,
        },
    };
    void *json = neu_json_decode_new(str);
    int   ret = neu_json_decode_by_json(json, NEU_JSON_ELEM_SIZE(elems), elems);
    if (ret != 0) {
        neu_json_decode_free(json);
        return NULL;
    } else {
        cid_dataset_info_t *info = calloc(1, sizeof(cid_dataset_info_t));
        info->control            = elems[0].v.val_bool;
        info->buffered           = elems[1].v.val_bool;
        strcpy(info->ied_name, elems[2].v.val_str);
        strcpy(info->ldevice_inst, elems[3].v.val_str);
        strcpy(info->ln_class, elems[4].v.val_str);
        strcpy(info->report_name, elems[5].v.val_str);
        strcpy(info->report_id, elems[6].v.val_str);
        strcpy(info->dataset_name, elems[7].v.val_str);

        free(elems[2].v.val_str);
        free(elems[3].v.val_str);
        free(elems[4].v.val_str);
        free(elems[5].v.val_str);
        free(elems[6].v.val_str);
        free(elems[7].v.val_str);

        neu_json_decode_free(json);
        return info;
    }
}