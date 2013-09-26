/* Copyright (c) 2011, TrafficLab, Ericsson Research, Hungary 
 * Copyright (c) 2012, CPqD, Brazil
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Ericsson Research nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "ofl.h"
#include "ofl-actions.h"
#include "ofl-log.h"
#ifdef OTN_SUPPORT
#include "ofp.h"
#endif

#define LOG_MODULE ofl_act
OFL_LOG_INIT(LOG_MODULE)

void
ofl_actions_free(struct ofl_action_header *act, struct ofl_exp *exp) {
    switch (act->type) {
        case OFPAT_SET_FIELD:{
            struct ofl_action_set_field *a = (struct ofl_action_set_field*) act;
            free(a->field->value);
            free(a->field);
            free(a);
            return;
            break;        
        }
        case OFPAT_OUTPUT:
        case OFPAT_COPY_TTL_OUT:
        case OFPAT_COPY_TTL_IN:
        case OFPAT_SET_MPLS_TTL:
        case OFPAT_DEC_MPLS_TTL:
        case OFPAT_PUSH_VLAN:
        case OFPAT_POP_VLAN:
        case OFPAT_PUSH_MPLS:
        case OFPAT_POP_MPLS:
        case OFPAT_PUSH_PBB:
        case OFPAT_POP_PBB:
        case OFPAT_SET_QUEUE:
        case OFPAT_GROUP:
        case OFPAT_SET_NW_TTL:
        case OFPAT_DEC_NW_TTL: {
            break;
        }
        case OFPAT_EXPERIMENTER: {
            if (exp == NULL || exp->act == NULL || exp->act->free == NULL) {
                OFL_LOG_WARN(LOG_MODULE, "Freeing experimenter action, but no callback is given.");
                break;
            }
            exp->act->free(act);
            return;
        }
        default: {
        }
    }
    free(act);
}

ofl_err
ofl_utils_count_ofp_actions(void *data, size_t data_len, size_t *count) {
    struct ofp_action_header *act;
    uint8_t *d;

    d = (uint8_t *)data;
    *count = 0;

    /* this is needed so that buckets are handled correctly */
    while (data_len >= sizeof(struct ofp_action_header) -4 ) {
        act = (struct ofp_action_header *)d;
        if (data_len < ntohs(act->len) || ntohs(act->len) < sizeof(struct ofp_action_header) - 4) {
            OFL_LOG_WARN(LOG_MODULE, "Received action has invalid length.");
            return ofl_error(OFPET_BAD_ACTION, OFPBAC_BAD_LEN);
        }
        data_len -= ntohs(act->len);
        d += ntohs(act->len);
        (*count)++;
    }

    return 0;
}

#ifdef OTN_SUPPORT
int  
ofl_exp_msg_act_pack(struct ofl_action_header *src, struct ofp_action_header *dst)
{
   struct ofl_action_tlab_experimenter_header *sa = (struct ofl_action_tlab_experimenter_header *)src;
   struct ofp_action_tlab_experimenter_header *da = (struct ofp_action_tlab_experimenter_header *)dst;
   int i=0;

   da->len = htons(sizeof(struct ofp_action_tlab_experimenter_header));

   if (ofl_validate_tlabs_experimenter(sa->experimenter) != 0) {
       return ofp_mkerr(OFPET_BAD_ACTION, OFPBAC_BAD_EXPERIMENTER);
   }
   da->experimenter = htonl(sa->experimenter);

   switch(sa->action) {
       case OFPXMT_TLAB_GMPLS_LABEL:
           da->action = sa->action;
           for(i=0;i<4;i++) {
               da->payload[i] = htonl(sa->payload[i]);
           }
           break;
       default:
           return ofl_error(OFPET_BAD_ACTION, OFPBAC_BAD_EXP_TYPE);
   }

   return sizeof(struct ofp_action_tlab_experimenter_header);
}

ofl_err 
ofl_exp_msg_act_unpack(struct ofp_action_header *src, size_t *len, struct ofl_action_header **dst)
{
    struct ofl_action_tlab_experimenter_header *sa_act_exp_msg = (struct ofl_action_tlab_experimenter_header *)src;
    struct ofp_action_tlab_experimenter_header *da_act_exp_msg = (struct ofp_action_tlab_experimenter_header *)malloc(sizeof(struct ofp_action_tlab_experimenter_header));
    int i, label_len=0;

    
    if (*len < ntohs(sa_act_exp_msg->len)) {
          OFL_LOG_WARN(LOG_MODULE, "Received Tellabs Experimenter action has invalid length (%zu)., expecting atleast.(%zu)", *len,ntohs(sa_act_exp_msg->len));
          return ofl_error(OFPET_BAD_ACTION, OFPBAC_BAD_LEN);
    }

    da_act_exp_msg->experimenter = ntohl(sa_act_exp_msg->experimenter);

    if (ofl_validate_tlabs_experimenter(da_act_exp_msg->experimenter ) != 0 ) {
          free(da_act_exp_msg);
          return ofp_mkerr(OFPET_BAD_ACTION, OFPBAC_BAD_EXPERIMENTER);
    }

    da_act_exp_msg->len = ntohs(sa_act_exp_msg->len);

    switch(sa_act_exp_msg->action) {
          case OFPXMT_TLAB_GMPLS_LABEL :
               da_act_exp_msg->action = (sa_act_exp_msg->action);
               label_len = ntohs(sa_act_exp_msg->len)-(offsetof(struct ofl_action_tlab_experimenter_header ,payload));
               for(i=0 ;label_len>0 ;i++,label_len-=4)
                   da_act_exp_msg->payload[i] = ntohl(sa_act_exp_msg->payload[i]);
               break;
          default:
            return ofl_error(OFPET_BAD_ACTION, OFPBAC_BAD_EXP_TYPE);
    }

    *len -= ntohs(sa_act_exp_msg->len);
    *dst = (struct ofp_action_header *)da_act_exp_msg;
    return 0;
}

int 
ofl_exp_msg_act_free(struct ofl_action_header *act)
{
  return 0;
}

size_t  
ofl_exp_msg_act_ofp_len(struct ofl_action_header *act)
{
     return sizeof(struct ofp_action_tlab_experimenter_header);
}

char   *
ofl_exp_msg_act_to_string(struct ofl_action_header *act)
{

  return 0;
}

#endif
