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

static int parse_ied(xmlNode *xml_ied, cid_ied_t *ied);
static int parse_template(xmlNode *xml_template, cid_template_t *template);
static int parse_ldevice(xmlNode *xml_server, cid_access_point_t *ap);
static int parse_lnode(xmlNode *xml_ldevice, cid_ldevice_t *ldev);
static int parse_dataset(xmlNode *xml_dataset, cid_ln_t *ln);
static int parse_report(xmlNode *xml_report, cid_ln_t *ln);
static int parse_doi(xmlNode *xml_doi, cid_ln_t *ln);

static void fill_fcda_type(cid_t *cid);
static void fill_doi_ctls(cid_t *cid);

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
            int ret = parse_template(find, &cid->cid_template);
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
            int ret = parse_ied(find_ied, &cid->ied);
            if (ret == 0) {
                ied_parsed = true;
            }
        }

        find_ied = find_ied->next;
    }

    if (ied_parsed && templates_parsed) {
        xmlFreeDoc(doc);
        fill_fcda_type(cid);
        fill_doi_ctls(cid);
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
    for (int i = 0; i < cid->cid_template.n_lnotypes; i++) {
        free(cid->cid_template.lnotypes[i].dos);
    }
    for (int i = 0; i < cid->cid_template.n_dotypes; i++) {
        free(cid->cid_template.dotypes[i].das);
        if (cid->cid_template.dotypes[i].n_sdos > 0) {
            free(cid->cid_template.dotypes[i].sdos);
        }
    }
    for (int i = 0; i < cid->cid_template.n_datypes; i++) {
        free(cid->cid_template.datypes[i].bdas);
    }
    if (cid->cid_template.n_lnotypes > 0) {
        free(cid->cid_template.lnotypes);
    }
    if (cid->cid_template.n_dotypes > 0) {
        free(cid->cid_template.dotypes);
    }
    if (cid->cid_template.n_datypes > 0) {
        free(cid->cid_template.datypes);
    }

    for (int i = 0; i < cid->ied.n_access_points; i++) {
        for (int j = 0; j < cid->ied.access_points[i].n_ldevices; j++) {
            for (int k = 0; k < cid->ied.access_points[i].ldevices[j].n_lns;
                 k++) {
                for (int l = 0; l <
                     cid->ied.access_points[i].ldevices[j].lns[k].n_datasets;
                     l++) {
                    if (cid->ied.access_points[i]
                            .ldevices[j]
                            .lns[k]
                            .datasets[l]
                            .n_fcda > 0) {
                        free(cid->ied.access_points[i]
                                 .ldevices[j]
                                 .lns[k]
                                 .datasets[l]
                                 .fcdas);
                    }
                }
                if (cid->ied.access_points[i].ldevices[j].lns[k].n_datasets >
                    0) {
                    free(cid->ied.access_points[i].ldevices[j].lns[k].datasets);
                }

                for (int l = 0;
                     l < cid->ied.access_points[i].ldevices[j].lns[k].n_dois;
                     l++) {
                    if (cid->ied.access_points[i]
                            .ldevices[j]
                            .lns[k]
                            .dois[l]
                            .n_dais > 0) {
                        free(cid->ied.access_points[i]
                                 .ldevices[j]
                                 .lns[k]
                                 .dois[l]
                                 .dais);
                    }
                    if (cid->ied.access_points[i]
                            .ldevices[j]
                            .lns[k]
                            .dois[l]
                            .n_ctls > 0) {
                        free(cid->ied.access_points[i]
                                 .ldevices[j]
                                 .lns[k]
                                 .dois[l]
                                 .ctls);
                    }
                }
                if (cid->ied.access_points[i].ldevices[j].lns[k].n_dois > 0) {
                    free(cid->ied.access_points[i].ldevices[j].lns[k].dois);
                }
                if (cid->ied.access_points[i].ldevices[j].lns[k].n_reports >
                    0) {
                    free(cid->ied.access_points[i].ldevices[j].lns[k].reports);
                }
            }

            if (cid->ied.access_points[i].ldevices[j].n_lns > 0) {
                free(cid->ied.access_points[i].ldevices[j].lns);
            }
        }

        if (cid->ied.access_points[i].n_ldevices > 0) {
            free(cid->ied.access_points[i].ldevices);
        }
    }

    if (cid->ied.n_access_points > 0) {
        free(cid->ied.access_points);
    }
}

static const char *find_type_id(cid_ldevice_t *ldev, const char *prefix,
                                const char *ln_class, const char *ln_inst)
{
    for (int i = 0; i < ldev->n_lns; i++) {
        if (strcmp(ldev->lns[i].lnprefix, prefix) == 0 &&
            strcmp(ldev->lns[i].lnclass, ln_class) == 0 &&
            strcmp(ldev->lns[i].lninst, ln_inst) == 0) {
            return (const char *) ldev->lns[i].lntype;
        }
    }
    return NULL;
}

static int find_basic_type(cid_template_t *template, const char *type_id,
                           const char *do_name, const char *da_name,
                           cid_fc_e fc, cid_basictype_e **btypes)
{
    *btypes                     = NULL;
    cid_tm_lno_type_t *lno      = NULL;
    char *             ref_type = NULL;
    cid_tm_do_type_t * dotype   = NULL;

    char *do_name_end = strchr(do_name, '.');

    for (int i = 0; i < template->n_lnotypes; i++) {
        if (strcmp(template->lnotypes[i].id, type_id) == 0) {
            lno = &template->lnotypes[i];
            break;
        }
    }

    if (lno != NULL) {
        for (int i = 0; i < lno->n_dos; i++) {
            if (do_name_end != NULL) {
                if (strncmp(lno->dos[i].name, do_name,
                            strlen(lno->dos[i].name)) == 0) {
                    ref_type = lno->dos[i].ref_type;
                    break;
                }
            } else {
                if (strcmp(lno->dos[i].name, do_name) == 0) {
                    ref_type = lno->dos[i].ref_type;
                    break;
                }
            }
        }
    } else {
        nlog_warn("Failed to find lno type %s", type_id);
    }

    if (ref_type != NULL) {
        for (int i = 0; i < template->n_dotypes; i++) {
            if (strcmp(template->dotypes[i].id, ref_type) == 0) {
                dotype = &template->dotypes[i];
                break;
            }
        }
    } else {
        nlog_warn("Failed to find ref type %s, %s", type_id, do_name);
    }

    if (dotype != NULL && do_name_end != NULL) {
        for (int i = 0; i < dotype->n_sdos; i++) {
            if (strcmp(dotype->sdos[i].name, do_name_end + 1) == 0) {
                ref_type = dotype->sdos[i].ref_type;
                for (int j = 0; j < template->n_dotypes; j++) {
                    if (strcmp(template->dotypes[j].id, ref_type) == 0) {
                        dotype = &template->dotypes[j];
                        break;
                    }
                }
                break;
            }
        }
    }

    if (dotype != NULL) {
        int index = 0;
        for (int i = 0; i < dotype->n_das; i++) {
            if (fc == dotype->das[i].fc) {
                if (dotype->das[i].btype != Struct) {
                    index += 1;
                    *btypes = realloc(*btypes, index * sizeof(cid_basictype_e));
                    (*btypes)[index - 1] = dotype->das[i].btype;
                } else {
                    for (int k = 0; k < template->n_datypes; k++) {
                        if (strcmp(template->datypes[k].id,
                                   dotype->das[i].ref_type) == 0) {
                            if (strlen(da_name) > 0) {
                                for (int l = 0; l < template->datypes[k].n_bdas;
                                     l++) {
                                    char da[NEU_CID_LEN64] = { 0 };
                                    snprintf(da, sizeof(da), "%s.%s",
                                             dotype->das[i].name,
                                             template->datypes[k].bdas[l].name);
                                    if (strcmp(da, da_name) == 0) {
                                        index += 1;
                                        *btypes = realloc(
                                            *btypes,
                                            index * sizeof(cid_basictype_e));
                                        (*btypes)[index - 1] =
                                            template->datypes[k].bdas[l].btype;
                                        break;
                                    }
                                }
                            } else {
                                for (int a = 0; a < template->datypes[k].n_bdas;
                                     a++) {
                                    index += 1;
                                    *btypes = realloc(
                                        *btypes,
                                        index * sizeof(cid_basictype_e));
                                    (*btypes)[index - 1] =
                                        template->datypes[k].bdas[a].btype;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }

        return index;
    } else {
        nlog_warn("Failed to find do type %s", ref_type);
    }

    return 0;
}

static void update_dataset(cid_dataset_t *dataset, cid_ldevice_t *ldev,
                           cid_template_t *template)
{
    for (int i = 0; i < dataset->n_fcda; i++) {
        const char *type_id =
            find_type_id(ldev, dataset->fcdas[i].prefix,
                         dataset->fcdas[i].lnclass, dataset->fcdas[i].lninst);
        if (type_id != NULL) {
            cid_basictype_e *bs  = NULL;
            int              ret = find_basic_type(
                template, type_id, dataset->fcdas[i].do_name,
                dataset->fcdas[i].da_name, dataset->fcdas[i].fc, &bs);
            if (ret > 0) {
                if (ret < 8) {
                    memcpy(dataset->fcdas[i].btypes, bs,
                           ret * sizeof(cid_basictype_e));
                    dataset->fcdas[i].n_btypes = ret;
                } else {
                    nlog_warn(
                        "Too many basic types for %s %s %s %s %s %d %s",
                        dataset->fcdas[i].da_name, dataset->fcdas[i].do_name,
                        dataset->fcdas[i].lnclass, dataset->fcdas[i].lninst,
                        dataset->fcdas[i].prefix, dataset->fcdas[i].fc,
                        type_id);
                }
                free(bs);
            }
        } else {
            nlog_warn("Failed to find type id for %s %s %s %s %s %d %s",
                      dataset->fcdas[i].da_name, dataset->fcdas[i].do_name,
                      dataset->fcdas[i].lnclass, dataset->fcdas[i].lninst,
                      dataset->fcdas[i].prefix, dataset->fcdas[i].fc, type_id);
        }
    }
}

static void fill_fcda_type(cid_t *cid)
{
    for (int i = 0; i < cid->ied.n_access_points; i++) {
        for (int j = 0; j < cid->ied.access_points[i].n_ldevices; j++) {
            cid_ldevice_t *ldev = &cid->ied.access_points[i].ldevices[j];
            for (int k = 0; k < cid->ied.access_points[i].ldevices[j].n_lns;
                 k++) {
                if (cid->ied.access_points[i].ldevices[j].lns[k].n_datasets >
                    0) {
                    for (int l = 0; l < cid->ied.access_points[i]
                                            .ldevices[j]
                                            .lns[k]
                                            .n_datasets;
                         l++) {
                        update_dataset(&cid->ied.access_points[i]
                                            .ldevices[j]
                                            .lns[k]
                                            .datasets[l],
                                       ldev, &cid->cid_template);
                    }
                }
            }
        }
    }
}

static void update_doi(const char *ref_type, cid_doi_t *dois, int n_dois,
                       cid_template_t *template)
{
    cid_tm_lno_type_t *tm_lno = NULL;
    for (int i = 0; i < template->n_lnotypes; i++) {
        if (strcmp(template->lnotypes[i].id, ref_type) == 0) {
            tm_lno = &template->lnotypes[i];
            break;
        }
    }

    if (tm_lno == NULL) {
        nlog_warn("Failed to find lno type %s", ref_type);
        return;
    }

    for (int i = 0; i < n_dois; i++) {
        // find the doi type
        char *do_ref_type = NULL;
        char *do_name_end = strchr(dois[i].name, '.');
        for (int k = 0; k < tm_lno->n_dos; k++) {
            if (do_name_end != NULL) {
                if (strncmp(tm_lno->dos[k].name, dois[i].name,
                            strlen(tm_lno->dos[k].name)) == 0) {
                    do_ref_type = tm_lno->dos[k].ref_type;
                    break;
                }
            } else {
                if (strcmp(tm_lno->dos[k].name, dois[i].name) == 0) {
                    do_ref_type = tm_lno->dos[k].ref_type;
                    break;
                }
            }
        }

        if (do_ref_type != NULL) {
            if (do_name_end != NULL) {
                do_ref_type = NULL;
                for (int k = 0; k < template->n_dotypes; k++) {
                    if (strcmp(template->dotypes[k].id, do_ref_type) == 0) {
                        for (int l = 0; l < template->dotypes[k].n_sdos; l++) {
                            if (strcmp(template->dotypes[k].sdos[l].name,
                                       do_name_end + 1) == 0) {
                                do_ref_type =
                                    template->dotypes[k].sdos[l].ref_type;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }

        cid_tm_do_type_t *tm_do = NULL;
        if (do_ref_type != NULL) {
            for (int k = 0; k < template->n_dotypes; k++) {
                if (strcmp(template->dotypes[k].id, do_ref_type) == 0) {
                    tm_do = &template->dotypes[k];
                    break;
                }
            }
        } else {
            nlog_warn("Failed to find do type %s", do_ref_type);
        }

        if (tm_do != NULL) {
            for (int k = 0; k < tm_do->n_das; k++) {
                if (tm_do->das[k].fc == CO) {
                    dois[i].n_ctls += 1;
                    dois[i].ctls = realloc(
                        dois[i].ctls, dois[i].n_ctls * sizeof(cid_doi_ctl_t));
                    cid_doi_ctl_t *ctl = &dois[i].ctls[dois[i].n_ctls - 1];
                    memset(ctl, 0, sizeof(cid_doi_ctl_t));

                    strcpy(ctl->da_name, tm_do->das[k].name);
                    ctl->fc = tm_do->das[k].fc;
                    for (int j = 0; j < template->n_datypes; j++) {
                        if (strcmp(template->datypes[j].id,
                                   tm_do->das[k].ref_type) == 0) {
                            ctl->btype = template->datypes[j].bdas[0].btype;
                            break;
                        }
                    }
                }

                if (tm_do->das[k].fc == SP || tm_do->das[k].fc == SG) {
                    if (tm_do->das[k].btype != Struct) {
                        dois[i].n_ctls += 1;
                        dois[i].ctls =
                            realloc(dois[i].ctls,
                                    dois[i].n_ctls * sizeof(cid_doi_ctl_t));
                        cid_doi_ctl_t *ctl = &dois[i].ctls[dois[i].n_ctls - 1];
                        memset(ctl, 0, sizeof(cid_doi_ctl_t));

                        ctl->btype = tm_do->das[k].btype;
                        ctl->fc    = tm_do->das[k].fc;
                        strcpy(ctl->da_name, tm_do->das[k].name);
                    } else {
                        dois[i].n_ctls += 1;
                        dois[i].ctls =
                            realloc(dois[i].ctls,
                                    dois[i].n_ctls * sizeof(cid_doi_ctl_t));
                        cid_doi_ctl_t *ctl = &dois[i].ctls[dois[i].n_ctls - 1];
                        memset(ctl, 0, sizeof(cid_doi_ctl_t));

                        strcpy(ctl->da_name, tm_do->das[k].name);
                        ctl->fc = tm_do->das[k].fc;
                        for (int j = 0; j < template->n_datypes; j++) {
                            if (strcmp(template->datypes[j].id,
                                       tm_do->das[k].ref_type) == 0) {
                                ctl->btype = template->datypes[j].bdas[0].btype;
                                break;
                            }
                        }
                    }
                }
            }
        } else {
            nlog_warn("Failed to find do type %s", do_ref_type);
        }
    }
}

static void fill_doi_ctls(cid_t *cid)
{
    for (int i = 0; i < cid->ied.n_access_points; i++) {
        for (int j = 0; j < cid->ied.access_points[i].n_ldevices; j++) {
            for (int k = 0; k < cid->ied.access_points[i].ldevices[j].n_lns;
                 k++) {
                update_doi(cid->ied.access_points[i].ldevices[j].lns[k].lntype,
                           cid->ied.access_points[i].ldevices[j].lns[k].dois,
                           cid->ied.access_points[i].ldevices[j].lns[k].n_dois,
                           &cid->cid_template);
            }
        }
    }
}

static int parse_ied(xmlNode *xml_ied, cid_ied_t *ied)
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
            char *name =
                (char *) xmlGetProp(access_point, (const xmlChar *) "name");
            ied->n_access_points += 1;
            ied->access_points =
                realloc(ied->access_points,
                        ied->n_access_points * sizeof(cid_access_point_t));
            cid_access_point_t *ap =
                &ied->access_points[ied->n_access_points - 1];
            memset(ap, 0, sizeof(cid_access_point_t));

            if (name != NULL) {
                strcpy(ap->name, name);
                xmlFree(name);
            }

            xmlNode *server = access_point->children;
            while (server != NULL) {
                if (server->type == XML_ELEMENT_NODE &&
                    strcmp((const char *) server->name, "Server") == 0) {
                    ret = parse_ldevice(server, ap);
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

static int parse_ldevice(xmlNode *xml_server, cid_access_point_t *ap)
{
    ap->ldevices     = NULL;
    ap->n_ldevices   = 0;
    xmlNode *ldevice = xml_server->children;

    while (ldevice != NULL) {
        if (ldevice->type != XML_ELEMENT_NODE) {
            ldevice = ldevice->next;
            continue;
        }
        if (strcmp((const char *) ldevice->name, "LDevice") == 0) {
            ap->n_ldevices += 1;
            ap->ldevices =
                realloc(ap->ldevices, ap->n_ldevices * sizeof(cid_ldevice_t));
            cid_ldevice_t *ldev = &ap->ldevices[ap->n_ldevices - 1];
            memset(ldev, 0, sizeof(cid_ldevice_t));
            char *inst = (char *) xmlGetProp(ldevice, (const xmlChar *) "inst");
            if (inst != NULL) {
                strcpy(ldev->inst, inst);
                xmlFree(inst);
            }
            int ret = parse_lnode(ldevice, ldev);
            if (ret != 0) {
                return ret;
            }
        }

        ldevice = ldevice->next;
    }

    return 0;
}

static int parse_lnode(xmlNode *xml_ldevice, cid_ldevice_t *ldev)
{
    xmlNode *lnode = xml_ldevice->children;
    while (lnode != NULL) {
        if (lnode->type == XML_ELEMENT_NODE &&
            strncmp((const char *) lnode->name, "LN", 2) == 0) {
            char *ln_class =
                (char *) xmlGetProp(lnode, (const xmlChar *) "lnClass");
            char *ln_type =
                (char *) xmlGetProp(lnode, (const xmlChar *) "lnType");
            char *ln_prefix =
                (char *) xmlGetProp(lnode, (const xmlChar *) "prefix");
            char *ln_inst =
                (char *) xmlGetProp(lnode, (const xmlChar *) "inst");

            ldev->n_lns += 1;
            ldev->lns    = realloc(ldev->lns, ldev->n_lns * sizeof(cid_ln_t));
            cid_ln_t *ln = &ldev->lns[ldev->n_lns - 1];
            memset(ln, 0, sizeof(cid_ln_t));

            if (ln_class != NULL) {
                strncpy(ln->lnclass, ln_class, sizeof(ln->lnclass) - 1);
                xmlFree(ln_class);
            }
            if (ln_type != NULL) {
                strncpy(ln->lntype, ln_type, sizeof(ln->lntype) - 1);
                xmlFree(ln_type);
            }
            if (ln_prefix != NULL) {
                strncpy(ln->lnprefix, ln_prefix, sizeof(ln->lnprefix) - 1);
                xmlFree(ln_prefix);
            }
            if (ln_inst != NULL) {
                strncpy(ln->lninst, ln_inst, sizeof(ln->lninst) - 1);
                xmlFree(ln_inst);
            }

            xmlNode *child = lnode->children;
            while (child != NULL) {
                if (child->type != XML_ELEMENT_NODE) {
                    child = child->next;
                    continue;
                }

                if (strcmp((const char *) child->name, "DataSet") == 0) {
                    int ret = parse_dataset(child, ln);
                    if (ret != 0) {
                        return ret;
                    }
                }
                if (strcmp((const char *) child->name, "ReportControl") == 0) {
                    int ret = parse_report(child, ln);
                    if (ret != 0) {
                        return ret;
                    }
                }
                if (strcmp((const char *) child->name, "DOI") == 0) {
                    int ret = parse_doi(child, ln);
                    if (ret != 0) {
                        return ret;
                    }
                }

                child = child->next;
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
    if (dataset_name != NULL) {
        ln->n_datasets += 1;
        ln->datasets =
            realloc(ln->datasets, ln->n_datasets * sizeof(cid_dataset_t));
        cid_dataset_t *dataset = &ln->datasets[ln->n_datasets - 1];
        memset(dataset, 0, sizeof(cid_dataset_t));

        strncpy(dataset->name, dataset_name, sizeof(dataset->name) - 1);
        xmlFree(dataset_name);

        xmlNode *xfcda = xml_dataset->children;
        while (xfcda != NULL) {
            if (xfcda->type != XML_ELEMENT_NODE) {
                xfcda = xfcda->next;
                continue;
            }

            if (strcmp((const char *) xfcda->name, "FCDA") == 0) {
                char *da_name =
                    (char *) xmlGetProp(xfcda, (const xmlChar *) "daName");
                char *do_name =
                    (char *) xmlGetProp(xfcda, (const xmlChar *) "doName");
                char *fc = (char *) xmlGetProp(xfcda, (const xmlChar *) "fc");
                char *ln_inst =
                    (char *) xmlGetProp(xfcda, (const xmlChar *) "lnInst");
                char *ln_class =
                    (char *) xmlGetProp(xfcda, (const xmlChar *) "lnClass");
                char *ld_inst =
                    (char *) xmlGetProp(xfcda, (const xmlChar *) "ldInst");
                char *prefix =
                    (char *) xmlGetProp(xfcda, (const xmlChar *) "prefix");

                dataset->n_fcda += 1;
                dataset->fcdas   = realloc(dataset->fcdas,
                                         dataset->n_fcda * sizeof(cid_fcda_t));
                cid_fcda_t *fcda = &dataset->fcdas[dataset->n_fcda - 1];
                memset(fcda, 0, sizeof(cid_fcda_t));

                if (da_name != NULL) {
                    strncpy(fcda->da_name, da_name, sizeof(fcda->da_name) - 1);
                    xmlFree(da_name);
                }
                if (do_name != NULL) {
                    strncpy(fcda->do_name, do_name, sizeof(fcda->do_name) - 1);
                    xmlFree(do_name);
                }
                if (ln_class != NULL) {
                    strncpy(fcda->lnclass, ln_class, sizeof(fcda->lnclass) - 1);
                    xmlFree(ln_class);
                }
                if (prefix != NULL) {
                    strncpy(fcda->prefix, prefix, sizeof(fcda->prefix) - 1);
                    xmlFree(prefix);
                }
                if (ln_inst != NULL) {
                    strncpy(fcda->lninst, ln_inst, sizeof(fcda->lninst) - 1);
                    xmlFree(ln_inst);
                }
                if (ld_inst != NULL) {
                    strncpy(fcda->ldinst, ld_inst, sizeof(fcda->ldinst) - 1);
                    xmlFree(ld_inst);
                }
                fcda->fc = F_UNKNOWN;
                if (fc != NULL) {
                    fcda->fc = decode_fc((const char *) fc);
                    xmlFree(fc);
                }
                if (fcda->fc == F_UNKNOWN) {
                    nlog_warn("Unknown FC %d", (int) xfcda->line);
                }
            }

            xfcda = xfcda->next;
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

    ln->n_reports += 1;
    ln->reports = realloc(ln->reports, ln->n_reports * sizeof(cid_report_t));
    cid_report_t *report = &ln->reports[ln->n_reports - 1];
    memset(report, 0, sizeof(cid_report_t));

    if (report_name != NULL) {
        strncpy(report->name, report_name, sizeof(report->name) - 1);
        xmlFree(report_name);
    }
    if (report_id != NULL) {
        strncpy(report->id, report_id, sizeof(report->id) - 1);
        xmlFree(report_id);
    }
    if (dataset != NULL) {
        strncpy(report->dataset, dataset, sizeof(report->dataset) - 1);
        xmlFree(dataset);
    }
    if (buffered != NULL) {
        if (strcmp(buffered, "true") == 0) {
            report->buffered = true;
        } else {
            report->buffered = false;
        }
        xmlFree(buffered);
    }
    if (intg_pd != NULL) {
        int interval = atoi(intg_pd);
        if (interval > 0) {
            report->intg_pd = interval;
        } else {
            report->intg_pd = 15000;
        }
        xmlFree(intg_pd);
    }

    return 0;
}

static int parse_doi(xmlNode *xml_doi, cid_ln_t *ln)
{
    char *name = (char *) xmlGetProp(xml_doi, (const xmlChar *) "name");
    if (name != NULL) {
        ln->n_dois += 1;
        ln->dois       = realloc(ln->dois, ln->n_dois * sizeof(cid_doi_t));
        cid_doi_t *doi = &ln->dois[ln->n_dois - 1];
        memset(doi, 0, sizeof(cid_doi_t));

        strncpy(doi->name, name, sizeof(doi->name) - 1);
        xmlFree(name);

        xmlNode *child = xml_doi->children;
        while (child != NULL) {
            if (child->type != XML_ELEMENT_NODE) {
                child = child->next;
                continue;
            }
            if (strcmp((const char *) child->name, "DAI") == 0) {
                char *dai_name =
                    (char *) xmlGetProp(child, (const xmlChar *) "name");
                if (dai_name != NULL) {
                    doi->n_dais += 1;
                    doi->dais =
                        realloc(doi->dais, doi->n_dais * sizeof(cid_dai_t));
                    cid_dai_t *dai = &doi->dais[doi->n_dais - 1];
                    memset(dai, 0, sizeof(cid_dai_t));

                    strncpy(dai->name, dai_name, sizeof(dai->name) - 1);
                    xmlFree(dai_name);
                }
            }

            if (strcmp((const char *) child->name, "SDI") == 0) {
                char *sdi_name =
                    (char *) xmlGetProp(child, (const xmlChar *) "name");
                if (sdi_name != NULL) {
                    xmlNode *dai = child->children;
                    while (dai != NULL) {
                        if (dai->type != XML_ELEMENT_NODE) {
                            dai = dai->next;
                            continue;
                        }

                        if (strcmp((const char *) dai->name, "DAI") == 0) {
                            char *dai_name = (char *) xmlGetProp(
                                dai, (const xmlChar *) "name");
                            if (dai_name != NULL) {
                                doi->n_dais += 1;
                                doi->dais = realloc(
                                    doi->dais, doi->n_dais * sizeof(cid_dai_t));
                                cid_dai_t *dai = &doi->dais[doi->n_dais - 1];
                                memset(dai, 0, sizeof(cid_dai_t));

                                strncpy(dai->name, dai_name,
                                        sizeof(dai->name) - 1);
                                strncpy(dai->sdi_name, sdi_name,
                                        sizeof(dai->sdi_name) - 1);
                                xmlFree(dai_name);
                            }
                        }

                        dai = dai->next;
                    }
                    xmlFree(sdi_name);
                }
            }

            child = child->next;
        }
    }
    return 0;
}

static int parse_template(xmlNode *xml_template, cid_template_t *template)
{
    template->lnotypes   = NULL;
    template->n_lnotypes = 0;
    template->dotypes    = NULL;
    template->n_dotypes  = 0;
    template->datypes    = NULL;
    template->n_datypes  = 0;

    xmlNode *child = xml_template->children;

    while (child != NULL) {
        if (child->type != XML_ELEMENT_NODE) {
            child = child->next;
            continue;
        }
        if (strcmp((const char *) child->name, "LNodeType") == 0) {
            char *id = (char *) xmlGetProp(child, (const xmlChar *) "id");
            char *ln_class =
                (char *) xmlGetProp(child, (const xmlChar *) "lnClass");
            if (id != NULL && ln_class != NULL) {
                template->n_lnotypes += 1;
                template->lnotypes =
                    realloc(template->lnotypes,
                            template->n_lnotypes * sizeof(cid_tm_lno_type_t));
                cid_tm_lno_type_t *tm_lno =
                    &template->lnotypes[template->n_lnotypes - 1];
                memset(tm_lno, 0, sizeof(cid_tm_lno_type_t));

                strcpy(tm_lno->id, id);
                strcpy(tm_lno->ln_class, ln_class);

                xmlNode *xdo = child->children;
                while (xdo != NULL) {
                    if (xdo->type != XML_ELEMENT_NODE) {
                        xdo = xdo->next;
                        continue;
                    }
                    char *name =
                        (char *) xmlGetProp(xdo, (const xmlChar *) "name");
                    char *ref_type =
                        (char *) xmlGetProp(xdo, (const xmlChar *) "type");

                    if (name != NULL && ref_type != NULL) {
                        tm_lno->n_dos += 1;
                        tm_lno->dos =
                            realloc(tm_lno->dos,
                                    tm_lno->n_dos * sizeof(cid_tm_lno_do_t));
                        cid_tm_lno_do_t *tm_do =
                            &tm_lno->dos[tm_lno->n_dos - 1];
                        memset(tm_do, 0, sizeof(cid_tm_lno_do_t));

                        strcpy(tm_do->name, name);
                        strcpy(tm_do->ref_type, ref_type);
                    }

                    if (name != NULL) {
                        xmlFree(name);
                    }
                    if (ref_type != NULL) {
                        xmlFree(ref_type);
                    }

                    xdo = xdo->next;
                }
            }

            if (id != NULL) {
                xmlFree(id);
            }
            if (ln_class != NULL) {
                xmlFree(ln_class);
            }
        }

        if (strcmp((const char *) child->name, "DOType") == 0) {
            char *id = (char *) xmlGetProp(child, (const xmlChar *) "id");
            if (id != NULL && strlen(id) < NEU_CID_ID_LEN) {
                template->n_dotypes += 1;
                template->dotypes =
                    realloc(template->dotypes,
                            template->n_dotypes * sizeof(cid_tm_do_type_t));
                cid_tm_do_type_t *tm_do =
                    &template->dotypes[template->n_dotypes - 1];
                memset(tm_do, 0, sizeof(cid_tm_do_type_t));

                strcpy(tm_do->id, id);
                xmlNode *da = child->children;
                while (da != NULL) {
                    if (da->type != XML_ELEMENT_NODE) {
                        da = da->next;
                        continue;
                    }

                    if (strcmp((const char *) da->name, "SDO") == 0) {
                        char *sdo_name =
                            (char *) xmlGetProp(da, (const xmlChar *) "name");
                        char *sdo_ref_type =
                            (char *) xmlGetProp(da, (const xmlChar *) "type");
                        if (sdo_name != NULL && sdo_ref_type != NULL) {
                            tm_do->n_sdos += 1;
                            tm_do->sdos =
                                realloc(tm_do->sdos,
                                        tm_do->n_sdos * sizeof(cid_tm_sdo_t));
                            cid_tm_sdo_t *tm_sdo =
                                &tm_do->sdos[tm_do->n_sdos - 1];
                            memset(tm_sdo, 0, sizeof(cid_tm_sdo_t));

                            strcpy(tm_sdo->name, sdo_name);
                            strcpy(tm_sdo->ref_type, sdo_ref_type);
                        }

                        if (sdo_name != NULL) {
                            xmlFree(sdo_name);
                        }
                        if (sdo_ref_type != NULL) {
                            xmlFree(sdo_ref_type);
                        }
                    }

                    if (strcmp((const char *) da->name, "DA") == 0) {
                        char *btype =
                            (char *) xmlGetProp(da, (const xmlChar *) "bType");
                        char *fc =
                            (char *) xmlGetProp(da, (const xmlChar *) "fc");
                        char *name =
                            (char *) xmlGetProp(da, (const xmlChar *) "name");
                        char *ref_type =
                            (char *) xmlGetProp(da, (const xmlChar *) "type");

                        if (btype != NULL && fc != NULL && name != NULL) {
                            tm_do->n_das += 1;
                            tm_do->das =
                                realloc(tm_do->das,
                                        tm_do->n_das * sizeof(cid_tm_do_da_t));
                            cid_tm_do_da_t *tm_da =
                                &tm_do->das[tm_do->n_das - 1];
                            memset(tm_da, 0, sizeof(cid_tm_do_da_t));

                            strcpy(tm_da->name, name);
                            if (ref_type != NULL) {
                                strcpy(tm_da->ref_type, ref_type);
                            }
                            tm_da->btype = T_UNKNOWN;
                            if (btype != NULL) {
                                tm_da->btype =
                                    decode_basictype((const char *) btype);
                            }
                            tm_da->fc = F_UNKNOWN;
                            if (fc != NULL) {
                                tm_da->fc = decode_fc((const char *) fc);
                            }
                            if (tm_da->btype == T_UNKNOWN) {
                                nlog_warn("Unknown btype %s, %d", btype,
                                          (int) da->line);
                            }
                            if (tm_da->fc == F_UNKNOWN) {
                                nlog_warn("Unknown fc %s, %d", fc,
                                          (int) da->line);
                            }
                        }

                        if (btype != NULL) {
                            xmlFree(btype);
                        }
                        if (fc != NULL) {
                            xmlFree(fc);
                        }
                        if (name != NULL) {
                            xmlFree(name);
                        }
                        if (ref_type != NULL) {
                            xmlFree(ref_type);
                        }
                    }

                    da = da->next;
                }
            }

            if (id != NULL) {
                xmlFree(id);
            }
        }

        if (strcmp((const char *) child->name, "DAType") == 0) {
            char *id = (char *) xmlGetProp(child, (const xmlChar *) "id");
            if (id != NULL && strlen(id) < NEU_CID_ID_LEN) {
                template->n_datypes += 1;
                template->datypes =
                    realloc(template->datypes,
                            template->n_datypes * sizeof(cid_tm_da_type_t));
                cid_tm_da_type_t *tm_dat =
                    &template->datypes[template->n_datypes - 1];
                memset(tm_dat, 0, sizeof(cid_tm_da_type_t));

                strcpy(tm_dat->id, id);
                xmlNode *bda = child->children;
                while (bda != NULL) {
                    if (bda->type != XML_ELEMENT_NODE) {
                        bda = bda->next;
                        continue;
                    }
                    char *btype =
                        (char *) xmlGetProp(bda, (const xmlChar *) "bType");
                    char *name =
                        (char *) xmlGetProp(bda, (const xmlChar *) "name");
                    char *ref_type =
                        (char *) xmlGetProp(bda, (const xmlChar *) "type");

                    if (btype != NULL && name != NULL) {
                        tm_dat->n_bdas += 1;
                        tm_dat->bdas =
                            realloc(tm_dat->bdas,
                                    tm_dat->n_bdas * sizeof(cid_tm_bda_type_t));
                        cid_tm_bda_type_t *tm_bda =
                            &tm_dat->bdas[tm_dat->n_bdas - 1];
                        memset(tm_bda, 0, sizeof(cid_tm_bda_type_t));
                        strcpy(tm_bda->name, name);
                        if (ref_type != NULL) {
                            strcpy(tm_bda->ref_type, ref_type);
                        }
                        tm_bda->btype = T_UNKNOWN;
                        if (btype != NULL) {
                            tm_bda->btype =
                                decode_basictype((const char *) btype);
                        }
                        if (tm_bda->btype == T_UNKNOWN) {
                            nlog_warn("Unknown btype %s, %d", btype,
                                      (int) bda->line);
                        }
                    }

                    if (btype != NULL) {
                        xmlFree(btype);
                    }
                    if (name != NULL) {
                        xmlFree(name);
                    }
                    if (ref_type != NULL) {
                        xmlFree(ref_type);
                    }

                    bda = bda->next;
                }
            } else {
                nlog_warn("skip, DAType id is null or too long %d",
                          (int) child->line);
            }
            if (id != NULL) {
                xmlFree(id);
            }
        }

        child = child->next;
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

void neu_cid_to_msg(char *driver, cid_t *cid, neu_req_add_gtag_t *cmd)
{
    strcpy(cmd->driver, driver);
    cmd->n_group = 0;
    cmd->groups  = NULL;
    (void) cid;

    // cmd->n_group += 1;
    // cmd->groups = realloc(cmd->groups, cmd->n_group *
    // sizeof(neu_gdatatag_t)); neu_gdatatag_t *ctl_group =
    // &cmd->groups[cmd->n_group - 1]; memset(ctl_group, 0,
    // sizeof(neu_gdatatag_t));

    // strcpy(ctl_group->group, "Control");
    // ctl_group->interval = 5000;

    // cid_dataset_info_t *ctl_info = calloc(1, sizeof(cid_dataset_info_t));
    // ctl_info->control            = true;
    // strcpy(ctl_info->ied_name, cid->ied.name);
    // ctl_group->context = ctl_info;

    // for (int i = 0; i < cid->ied.n_ldevices; i++) {
    // for (int j = 0; j < cid->ied.ldevices[i].n_lns; j++) {
    // for (int k = 0; k < cid->ied.ldevices[i].lns[j].n_ctrls; k++) {
    // ctl_group->n_tag += 1;
    // ctl_group->tags = realloc(
    // ctl_group->tags, ctl_group->n_tag * sizeof(neu_datatag_t));
    // neu_datatag_t *tag = &ctl_group->tags[ctl_group->n_tag - 1];
    // memset(tag, 0, sizeof(neu_datatag_t));

    // tag->name = calloc(1, NEU_TAG_NAME_LEN);
    // snprintf(tag->name, NEU_TAG_NAME_LEN - 1, "%s/%s$%s",
    // cid->ied.ldevices[i].inst,
    // cid->ied.ldevices[i].lns[j].ctrls[k].do_name,
    // cid->ied.ldevices[i].lns[j].ctrls[k].sdi_name);

    // tag->attribute   = NEU_ATTRIBUTE_WRITE;
    // tag->type        = NEU_TYPE_BOOL;
    // tag->address     = calloc(1, NEU_TAG_ADDRESS_LEN);
    // tag->description = strdup("");
    // snprintf(tag->address, NEU_TAG_ADDRESS_LEN - 1,
    //"%s%s/%s%s%s$%s$%s$%s", cid->ied.name,
    // cid->ied.ldevices[i].inst,
    // cid->ied.ldevices[i].lns[j].lnprefix,
    // cid->ied.ldevices[i].lns[j].lnclass,
    // cid->ied.ldevices[i].lns[j].lninst, "CO",
    // cid->ied.ldevices[i].lns[j].ctrls[k].do_name,
    // cid->ied.ldevices[i].lns[j].ctrls[k].sdi_name);
    //}
    //}
    //}

    // for (int i = 0; i < cid->ied.n_ldevices; i++) {
    // for (int j = 0; j < cid->ied.ldevices[i].n_lns; j++) {
    // for (int k = 0; k < cid->ied.ldevices[i].lns[j].n_datasets; k++) {
    // for (int l = 0; l < cid->ied.ldevices[i].lns[j].n_reports;
    // l++) {
    // if (strcmp(
    // cid->ied.ldevices[i].lns[j].datasets[k].name,
    // cid->ied.ldevices[i].lns[j].reports[l].dataset) ==
    // 0) {
    // if (cid->ied.ldevices[i].lns[j].datasets[k].n_fcda ==
    // 0) {
    // break;
    //}
    // cmd->n_group += 1;
    // cmd->groups = realloc(
    // cmd->groups, cmd->n_group * sizeof(neu_gdatatag_t));
    // neu_gdatatag_t *group = &cmd->groups[cmd->n_group - 1];
    // memset(group, 0, sizeof(neu_gdatatag_t));

    // cid_dataset_info_t *info =
    // calloc(1, sizeof(cid_dataset_info_t));

    // info->control = false;
    // info->buffered =
    // cid->ied.ldevices[i].lns[j].reports[l].buffered;
    // strcpy(info->ied_name, cid->ied.name);
    // strcpy(info->ldevice_inst, cid->ied.ldevices[i].inst);
    // strcpy(info->ln_class,
    // cid->ied.ldevices[i].lns[j].lnclass);
    // strcpy(info->report_name,
    // cid->ied.ldevices[i].lns[j].reports[l].name);
    // strcpy(info->report_id,
    // cid->ied.ldevices[i].lns[j].reports[l].id);
    // strcpy(info->dataset_name,
    // cid->ied.ldevices[i].lns[j].datasets[k].name);
    // group->context = info;

    // snprintf(group->group, NEU_GROUP_NAME_LEN - 1,
    //"%s.%s.%s", cid->ied.ldevices[i].inst,
    // cid->ied.ldevices[i].lns[j].lnclass,
    // cid->ied.ldevices[i].lns[j].datasets[k].name);
    // group->interval =
    // cid->ied.ldevices[i].lns[j].reports[l].intg_pd;
    // group->n_tag =
    // cid->ied.ldevices[i].lns[j].datasets[k].n_fcda;
    // group->tags =
    // calloc(group->n_tag, sizeof(neu_datatag_t));

    // for (int m = 0;
    // m < cid->ied.ldevices[i].lns[j].datasets[k].n_fcda;
    // m++) {
    // group->tags[m].name = calloc(1, NEU_TAG_NAME_LEN);
    // snprintf(group->tags[m].name, NEU_TAG_NAME_LEN - 1,
    //"%s%s%s.%s.%s",
    // cid->ied.ldevices[i]
    //.lns[j]
    //.datasets[k]
    //.fcdas[m]
    //.prefix,
    // cid->ied.ldevices[i]
    //.lns[j]
    //.datasets[k]
    //.fcdas[m]
    //.lnclass,
    // cid->ied.ldevices[i]
    //.lns[j]
    //.datasets[k]
    //.fcdas[m]
    //.lninst,
    // cid->ied.ldevices[i]
    //.lns[j]
    //.datasets[k]
    //.fcdas[m]
    //.do_name,
    // cid->ied.ldevices[i]
    //.lns[j]
    //.datasets[k]
    //.fcdas[m]
    //.da_name);
    // if (group->tags[m]
    //.name[strlen(group->tags[m].name) - 1] ==
    //'.') {
    // group->tags[m]
    //.name[strlen(group->tags[m].name) - 1] =
    //'\0';
    //}
    // group->tags[m].attribute =
    // NEU_ATTRIBUTE_READ | NEU_ATTRIBUTE_SUBSCRIBE;
    // group->tags[m].address =
    // calloc(1, NEU_TAG_ADDRESS_LEN);
    // group->tags[m].description = strdup("");
    // switch (cid->ied.ldevices[i]
    //.lns[j]
    //.datasets[k]
    //.fcdas[m]
    //.fc) {
    // case ST:
    // group->tags[m].type = NEU_TYPE_BOOL;
    // snprintf(group->tags[m].address,
    // NEU_TAG_ADDRESS_LEN - 1,
    //"%s%s/%s$%s$%s", cid->ied.name,
    // cid->ied.ldevices[i].inst,
    // cid->ied.ldevices[i]
    //.lns[j]
    //.datasets[k]
    //.fcdas[m]
    //.lnclass,
    //"ST",
    // cid->ied.ldevices[i]
    //.lns[j]
    //.datasets[k]
    //.fcdas[m]
    //.do_name);
    // break;
    // case MX:
    // group->tags[m].type = NEU_TYPE_FLOAT;
    // snprintf(group->tags[m].address,
    // NEU_TAG_ADDRESS_LEN - 1,
    //"%s%s/%s$%s$%s", cid->ied.name,
    // cid->ied.ldevices[i].inst,
    // cid->ied.ldevices[i]
    //.lns[j]
    //.datasets[k]
    //.fcdas[m]
    //.lnclass,
    //"MX",
    // cid->ied.ldevices[i]
    //.lns[j]
    //.datasets[k]
    //.fcdas[m]
    //.do_name);
    // break;
    // default:
    // group->tags[m].type = NEU_TYPE_INT16;
    // break;
    //}
    //}

    // break;
    //}
    //}
    //}
    //}
    //}
}