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

static cid_tm_do_type_t *find_do_type(cid_template_t *template,
                                      const char *type_id, const char *do_name)
{
    cid_tm_lno_type_t *lno      = NULL;
    cid_tm_do_type_t * dotype   = NULL;
    char *             ref_type = NULL;

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

    return dotype;
}

typedef struct {
    char            all_name[NEU_CID_LEN64];
    cid_basictype_e btype;
} da_basic_type_t;

static int find_da_basic_type(cid_template_t *template, const char *datype_id,
                              const char *seg_name, da_basic_type_t *da_types)
{
    char name[NEU_CID_LEN64] = { 0 };
    int  index               = 0;
    for (int i = 0; i < template->n_datypes; i++) {
        if (strcmp(template->datypes[i].id, datype_id) == 0) {
            for (int j = 0; j < template->datypes[i].n_bdas; j++) {
                if (strlen(seg_name) > 0) {
                    snprintf(name, sizeof(name), "%s.%s", seg_name,
                             template->datypes[i].bdas[j].name);
                } else {
                    snprintf(name, sizeof(name), "%s",
                             template->datypes[i].bdas[j].name);
                }
                if (template->datypes[i].bdas[j].btype == Struct) {
                    index += find_da_basic_type(
                        template, template->datypes[i].bdas[j].ref_type, name,
                        da_types + index);
                } else {
                    strcpy(da_types[index].all_name, name);
                    da_types[index].btype = template->datypes[i].bdas[j].btype;
                    index += 1;
                }
            }
            break;
        }
    }

    return index;
}

static int find_basic_type(cid_template_t *template, const char *type_id,
                           const char *do_name, const char *da_name,
                           cid_fc_e fc, cid_basictype_e **btypes)
{
    *btypes                  = NULL;
    cid_tm_do_type_t *dotype = NULL;
    int               index  = 0;

    dotype = find_do_type(template, type_id, do_name);
    if (dotype == NULL) {
        nlog_warn("Failed to find do type %s", type_id);
        return 0;
    }

    for (int i = 0; i < dotype->n_das; i++) {
        if (fc != dotype->das[i].fc) {
            continue;
        }

        if (dotype->das[i].btype != Struct) {
            index += 1;
            *btypes = realloc(*btypes, index * sizeof(cid_basictype_e));
            (*btypes)[index - 1] = dotype->das[i].btype;
            continue;
        }

        da_basic_type_t da_types[32] = { 0 };
        int n_da_types = find_da_basic_type(template, dotype->das[i].ref_type,
                                            dotype->das[i].name, da_types);
        for (int j = 0; j < n_da_types; j++) {
            if (strlen(da_name) > 0) {
                if (strcmp(da_types[j].all_name, da_name) == 0) {
                    index += 1;
                    *btypes = realloc(*btypes, index * sizeof(cid_basictype_e));
                    (*btypes)[index - 1] = da_types[j].btype | 0x80;
                    break;
                }
            } else {
                index += 1;
                *btypes = realloc(*btypes, index * sizeof(cid_basictype_e));
                (*btypes)[index - 1] = da_types[j].btype | 0x80;
            }
        }
    }

    return index;
}

static void update_dataset(cid_dataset_t *dataset, cid_ldevice_t *ldev,
                           cid_template_t *template)
{
    for (int i = 0; i < dataset->n_fcda; i++) {
        // LN lnType
        const char *type_id =
            find_type_id(ldev, dataset->fcdas[i].prefix,
                         dataset->fcdas[i].lnclass, dataset->fcdas[i].lninst);
        if (type_id != NULL) {
            cid_basictype_e *bs  = NULL;
            int              ret = find_basic_type(
                template, type_id, dataset->fcdas[i].do_name,
                dataset->fcdas[i].da_name, dataset->fcdas[i].fc, &bs);
            if (ret > 0) {
                if (ret <= 16) {
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
            nlog_warn("Failed to find LN lnType for %s %s %s %s %s %d %s",
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
    for (int i = 0; i < n_dois; i++) {
        cid_tm_do_type_t *tm_do =
            find_do_type(template, ref_type, dois[i].name);

        if (tm_do == NULL) {
            continue;
        }

        for (int k = 0; k < tm_do->n_das; k++) {
            cid_tm_do_da_t *da = &tm_do->das[k];

            if (da->btype != Struct) {
                // CO must be Struct
                if (da->fc == SP || da->fc == SG) {
                    dois[i].n_ctls += 1;
                    dois[i].ctls = realloc(
                        dois[i].ctls, dois[i].n_ctls * sizeof(cid_doi_ctl_t));
                    cid_doi_ctl_t *ctl = &dois[i].ctls[dois[i].n_ctls - 1];
                    memset(ctl, 0, sizeof(cid_doi_ctl_t));

                    ctl->btype = da->btype;
                    ctl->fc    = da->fc;
                    strcpy(ctl->da_name, da->name);
                }
            }

            da_basic_type_t da_types[32] = { 0 };
            int             n_da_types   = find_da_basic_type(
                template, tm_do->das[k].ref_type, tm_do->das[k].name, da_types);

            if (da->fc == SP || da->fc == SG) {
                for (int j = 0; j < n_da_types; j++) {
                    dois[i].n_ctls += 1;
                    dois[i].ctls = realloc(
                        dois[i].ctls, dois[i].n_ctls * sizeof(cid_doi_ctl_t));
                    cid_doi_ctl_t *ctl = &dois[i].ctls[dois[i].n_ctls - 1];
                    memset(ctl, 0, sizeof(cid_doi_ctl_t));

                    ctl->btype = da_types[j].btype;
                    ctl->fc    = da->fc;
                    strcpy(ctl->da_name, da_types[j].all_name);
                }
            }

            if (da->fc == CO) {
                dois[i].n_ctls += 1;
                dois[i].ctls       = realloc(dois[i].ctls,
                                       dois[i].n_ctls * sizeof(cid_doi_ctl_t));
                cid_doi_ctl_t *ctl = &dois[i].ctls[dois[i].n_ctls - 1];
                memset(ctl, 0, sizeof(cid_doi_ctl_t));

                ctl->btype = da_types[0].btype;
                ctl->fc    = da->fc;
                strcpy(ctl->da_name, da->name);
                ctl->n_co_types = n_da_types;
                for (int j = 0; j < n_da_types; j++) {
                    ctl->co_types[j] = da_types[j].btype;
                }
            }
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
                                cid_dai_t *t_dai = &doi->dais[doi->n_dais - 1];
                                memset(t_dai, 0, sizeof(cid_dai_t));

                                strncpy(t_dai->name, dai_name,
                                        sizeof(t_dai->name) - 1);
                                strncpy(t_dai->sdi_name, sdi_name,
                                        sizeof(t_dai->sdi_name) - 1);
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
                            tm_da->btype =
                                decode_basictype((const char *) btype);
                            tm_da->fc = F_UNKNOWN;
                            tm_da->fc = decode_fc((const char *) fc);
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
                        tm_bda->btype = decode_basictype((const char *) btype);
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

static void ld_ctrl_do_to_group(const char *ied_name, const char *ap_name,
                                cid_ldevice_t *ld, neu_req_add_gtag_t *cmd)
{
    bool exist_ctl = false;
    for (int i = 0; i < ld->n_lns; i++) {
        for (int k = 0; k < ld->lns[i].n_dois; k++) {
            if (ld->lns[i].dois[k].n_ctls > 0) {
                exist_ctl = true;
                break;
            }
        }
    }

    if (!exist_ctl) {
        return;
    }

    cmd->n_group += 1;
    cmd->groups = realloc(cmd->groups, cmd->n_group * sizeof(neu_gdatatag_t));
    neu_gdatatag_t *group = &cmd->groups[cmd->n_group - 1];
    memset(group, 0, sizeof(neu_gdatatag_t));

    cid_dataset_info_t *g_info = calloc(1, sizeof(cid_dataset_info_t));

    g_info->control = true;
    strcpy(g_info->ied_name, ied_name);
    strcpy(g_info->ldevice_inst, ld->inst);

    group->context = g_info;
    snprintf(group->group, NEU_GROUP_NAME_LEN, "%s/%s$Control", ap_name,
             ld->inst);
    group->interval = 10000;

    for (int i = 0; i < ld->n_lns; i++) {
        for (int j = 0; j < ld->lns[i].n_dois; j++) {
            for (int k = 0; k < ld->lns[i].dois[j].n_ctls; k++) {
                cid_doi_ctl_t *ctl = &ld->lns[i].dois[j].ctls[k];
                char           ln_name[NEU_CID_LEN32] = { 0 };
                int            offset                 = 0;

                if (strlen(ld->lns[i].lnprefix) > 0) {
                    offset += snprintf(ln_name, NEU_CID_LEN32 - offset, "%s",
                                       ld->lns[i].lnprefix);
                }
                offset += snprintf(ln_name + offset, NEU_CID_LEN32 - offset,
                                   "%s", ld->lns[i].lnclass);
                if (strlen(ld->lns[i].lninst) > 0) {
                    offset += snprintf(ln_name + offset, NEU_CID_LEN32 - offset,
                                       "%s", ld->lns[i].lninst);
                }

                group->n_tag += 1;
                group->tags =
                    realloc(group->tags, group->n_tag * sizeof(neu_datatag_t));
                neu_datatag_t *tag = &group->tags[group->n_tag - 1];
                memset(tag, 0, sizeof(neu_datatag_t));

                tag->name = calloc(1, NEU_TAG_NAME_LEN);
                snprintf(tag->name, NEU_TAG_NAME_LEN, "%s$%s$%s", ln_name,
                         ld->lns[i].dois[j].name, ctl->da_name);
                tag->attribute   = NEU_ATTRIBUTE_WRITE;
                tag->address     = calloc(1, NEU_TAG_ADDRESS_LEN);
                tag->description = calloc(1, NEU_TAG_DESCRIPTION_LEN);
                snprintf(tag->description, NEU_TAG_DESCRIPTION_LEN, "%s",
                         fc_to_str(ctl->fc));
                snprintf(tag->address, NEU_TAG_ADDRESS_LEN, "%s%s/%s$%s$%s$%s",
                         ied_name, ld->inst, ln_name, fc_to_str(ctl->fc),
                         ld->lns[i].dois[j].name, ctl->da_name);
                if (ctl->fc == CO) {
                    tag->n_format = ctl->n_co_types;
                    for (int l = 0; l < ctl->n_co_types; l++) {
                        tag->format[l] = ctl->co_types[l];
                    }
                } else {
                    for (size_t index = 0; index < strlen(tag->address);
                         index++) {
                        if (tag->address[index] == '.') {
                            tag->address[index] = '$';
                        }
                    }
                    tag->n_format  = 1;
                    tag->format[0] = ctl->btype;
                }
                switch (ctl->btype & 0x7f) {
                case BOOLEAN:
                    tag->type = NEU_TYPE_BOOL;
                    break;
                case INT8:
                    tag->type = NEU_TYPE_INT8;
                    break;
                case INT16:
                    tag->type = NEU_TYPE_INT16;
                    break;
                case INT24:
                    tag->type = NEU_TYPE_INT32;
                    break;
                case INT32:
                    tag->type = NEU_TYPE_INT32;
                    break;
                case INT128:
                    tag->type = NEU_TYPE_INT64;
                    break;
                case INT8U:
                    tag->type = NEU_TYPE_UINT8;
                    break;
                case INT16U:
                    tag->type = NEU_TYPE_UINT16;
                    break;
                case INT24U:
                    tag->type = NEU_TYPE_UINT32;
                    break;
                case INT32U:
                    tag->type = NEU_TYPE_UINT32;
                    break;
                case FLOAT32:
                    tag->type = NEU_TYPE_FLOAT;
                    break;
                case FLOAT64:
                    tag->type = NEU_TYPE_DOUBLE;
                    break;
                case Enum:
                    tag->type = NEU_TYPE_UINT8;
                    break;
                case Dbpos: // TODO
                    tag->type = NEU_TYPE_UINT8;
                    break;
                case Tcmd:
                    tag->type = NEU_TYPE_UINT8;
                    break;
                case Quality:
                    tag->type = NEU_TYPE_UINT16;
                    break;
                case Timestamp:
                    tag->type = NEU_TYPE_INT64;
                    break;
                case VisString32:
                    tag->type = NEU_TYPE_STRING;
                    break;
                case VisString64:
                    tag->type = NEU_TYPE_STRING;
                    break;
                case VisString255:
                    tag->type = NEU_TYPE_STRING;
                    break;
                case Octet64: // TODO
                    tag->type = NEU_TYPE_UINT64;
                    break;
                case Struct: // TODO
                    tag->type = NEU_TYPE_UINT8;
                    break;
                case EntryTime: // TODO
                    tag->type = NEU_TYPE_INT64;
                    break;
                case Unicode255: // TODO
                    tag->type = NEU_TYPE_STRING;
                    break;
                case Check: // TODO
                    tag->type = NEU_TYPE_UINT8;
                    break;
                case T_UNKNOWN:
                    tag->type = NEU_TYPE_UINT8;
                    break;
                }
            }
        }
    }
}

static void ln_dataset_to_group(const char *ied_name, const char *ap_name,
                                const char *ld_inst, const cid_ln_t *ln,
                                neu_req_add_gtag_t *cmd)
{
    for (int i = 0; i < ln->n_reports; i++) {
        for (int j = 0; j < ln->n_datasets; j++) {
            if (strcmp(ln->reports[i].dataset, ln->datasets[j].name) == 0) {
                if (ln->datasets[j].n_fcda == 0) {
                    break;
                }

                cmd->n_group += 1;
                cmd->groups =
                    realloc(cmd->groups, cmd->n_group * sizeof(neu_gdatatag_t));
                neu_gdatatag_t *group = &cmd->groups[cmd->n_group - 1];
                memset(group, 0, sizeof(neu_gdatatag_t));

                cid_dataset_info_t *g_info =
                    calloc(1, sizeof(cid_dataset_info_t));

                g_info->control = false;
                strcpy(g_info->ied_name, ied_name);
                strcpy(g_info->ldevice_inst, ld_inst);
                strcpy(g_info->ln_class, ln->lnclass);
                g_info->buffered = ln->reports[i].buffered;
                strcpy(g_info->report_name, ln->reports[i].name);
                strcpy(g_info->report_id, ln->reports[i].id);
                strcpy(g_info->dataset_name, ln->datasets[j].name);

                group->context = g_info;

                snprintf(group->group, NEU_GROUP_NAME_LEN, "%s/%s$%s$%s",
                         ap_name, ld_inst, ln->lnclass, ln->datasets[j].name);
                group->interval = 10000;
                if (ln->reports[i].intg_pd >= 100) {
                    group->interval = ln->reports[i].intg_pd;
                }
                group->n_tag = ln->datasets[j].n_fcda;
                group->tags =
                    calloc(1, ln->datasets[j].n_fcda * sizeof(neu_datatag_t));

                for (int k = 0; k < ln->datasets[j].n_fcda; k++) {
                    neu_datatag_t *tag = &group->tags[k];

                    tag->name = calloc(1, NEU_TAG_NAME_LEN);
                    if (strlen(ln->datasets[j].fcdas[k].da_name) > 0) {
                        snprintf(tag->name, NEU_TAG_NAME_LEN, "%s%s%s.%s.%s",
                                 ln->datasets[j].fcdas[k].prefix,
                                 ln->datasets[j].fcdas[k].lnclass,
                                 ln->datasets[j].fcdas[k].lninst,
                                 ln->datasets[j].fcdas[k].do_name,
                                 ln->datasets[j].fcdas[k].da_name);
                    } else {
                        snprintf(tag->name, NEU_TAG_NAME_LEN, "%s%s%s.%s",
                                 ln->datasets[j].fcdas[k].prefix,
                                 ln->datasets[j].fcdas[k].lnclass,
                                 ln->datasets[j].fcdas[k].lninst,
                                 ln->datasets[j].fcdas[k].do_name);
                    }
                    tag->attribute   = NEU_ATTRIBUTE_READ;
                    tag->address     = calloc(1, NEU_TAG_ADDRESS_LEN);
                    tag->description = calloc(1, NEU_TAG_DESCRIPTION_LEN);
                    snprintf(tag->description, NEU_TAG_DESCRIPTION_LEN, "%s",
                             fc_to_str(ln->datasets[j].fcdas[k].fc));
                    if (strlen(ln->datasets[j].fcdas[k].da_name) > 0) {
                        snprintf(tag->address, NEU_TAG_ADDRESS_LEN,
                                 "%s/%s$%s$%s$%s", ld_inst,
                                 ln->datasets[j].fcdas[k].lnclass,
                                 fc_to_str(ln->datasets[j].fcdas[k].fc),
                                 ln->datasets[j].fcdas[k].do_name,
                                 ln->datasets[j].fcdas[k].da_name);
                    } else {
                        snprintf(tag->address, NEU_TAG_ADDRESS_LEN,
                                 "%s/%s$%s$%s", ld_inst,
                                 ln->datasets[j].fcdas[k].lnclass,
                                 fc_to_str(ln->datasets[j].fcdas[k].fc),
                                 ln->datasets[j].fcdas[k].do_name);
                    }
                    switch (ln->datasets[j].fcdas[k].btypes[0] & 0x7f) {
                    case BOOLEAN:
                        tag->type = NEU_TYPE_BOOL;
                        break;
                    case INT8:
                        tag->type = NEU_TYPE_INT8;
                        break;
                    case INT16:
                        tag->type = NEU_TYPE_INT16;
                        break;
                    case INT24:
                        tag->type = NEU_TYPE_INT32;
                        break;
                    case INT32:
                        tag->type = NEU_TYPE_INT32;
                        break;
                    case INT128:
                        tag->type = NEU_TYPE_INT64;
                        break;
                    case INT8U:
                        tag->type = NEU_TYPE_UINT8;
                        break;
                    case INT16U:
                        tag->type = NEU_TYPE_UINT16;
                        break;
                    case INT24U:
                        tag->type = NEU_TYPE_UINT32;
                        break;
                    case INT32U:
                        tag->type = NEU_TYPE_UINT32;
                        break;
                    case FLOAT32:
                        tag->type = NEU_TYPE_FLOAT;
                        break;
                    case FLOAT64:
                        tag->type = NEU_TYPE_DOUBLE;
                        break;
                    case Enum:
                        tag->type = NEU_TYPE_UINT8;
                        break;
                    case Dbpos: // TODO
                        tag->type = NEU_TYPE_UINT8;
                        break;
                    case Tcmd:
                        tag->type = NEU_TYPE_UINT8;
                        break;
                    case Quality:
                        tag->type = NEU_TYPE_UINT16;
                        break;
                    case Timestamp:
                        tag->type = NEU_TYPE_INT64;
                        break;
                    case VisString32:
                        tag->type = NEU_TYPE_STRING;
                        break;
                    case VisString64:
                        tag->type = NEU_TYPE_STRING;
                        break;
                    case VisString255:
                        tag->type = NEU_TYPE_STRING;
                        break;
                    case Octet64: // TODO
                        tag->type = NEU_TYPE_UINT64;
                        break;
                    case Struct: // TODO
                        tag->type = NEU_TYPE_UINT8;
                        break;
                    case EntryTime: // TODO
                        tag->type = NEU_TYPE_INT64;
                        break;
                    case Unicode255: // TODO
                        tag->type = NEU_TYPE_STRING;
                        break;
                    case Check: // TODO
                        tag->type = NEU_TYPE_UINT8;
                        break;
                    case T_UNKNOWN:
                        tag->type = NEU_TYPE_UINT8;
                        break;
                    }

                    tag->n_format = ln->datasets[j].fcdas[k].n_btypes;
                    for (int l = 0; l < ln->datasets[j].fcdas[k].n_btypes;
                         l++) {
                        tag->format[l] = ln->datasets[j].fcdas[k].btypes[l];
                    }
                }

                break;
            }
        }
    }
}

void neu_cid_to_msg(char *driver, cid_t *cid, neu_req_add_gtag_t *cmd)
{
    strcpy(cmd->driver, driver);
    cmd->n_group = 0;
    cmd->groups  = NULL;

    for (int i = 0; i < cid->ied.n_access_points; i++) {
        for (int j = 0; j < cid->ied.access_points[i].n_ldevices; j++) {
            ld_ctrl_do_to_group(cid->ied.name, cid->ied.access_points[i].name,
                                &cid->ied.access_points[i].ldevices[j], cmd);
            for (int k = 0; k < cid->ied.access_points[i].ldevices[j].n_lns;
                 k++) {
                ln_dataset_to_group(
                    cid->ied.name, cid->ied.access_points[i].name,
                    cid->ied.access_points[i].ldevices[j].inst,
                    &cid->ied.access_points[i].ldevices[j].lns[k], cmd);
            }
        }
    }
}