/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006-2013 Los Alamos National Security, LLC. 
 *                         All rights reserved.
 * Copyright (c) 2010-2011 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef _MCA_OOB_TCP_SENDRECV_H_
#define _MCA_OOB_TCP_SENDRECV_H_

#include "orte_config.h"

#include "opal/class/opal_list.h"

#include "orte/mca/rml/base/base.h"

#include "oob_tcp.h"
#include "oob_tcp_hdr.h"

/* tcp structure for sending a message */
typedef struct {
    opal_list_item_t super;
    mca_oob_tcp_hdr_t hdr;
    orte_rml_send_t *msg;
    char *data;
    bool hdr_sent;
    int iovnum;
    char *sdptr;
    size_t sdbytes;
} mca_oob_tcp_send_t;
OBJ_CLASS_DECLARATION(mca_oob_tcp_send_t);

/* tcp structure for recving a message */
typedef struct {
    opal_list_item_t super;
    mca_oob_tcp_hdr_t hdr;
    bool hdr_recvd;
    char *data;
    char *rdptr;
    size_t rdbytes;
} mca_oob_tcp_recv_t;
OBJ_CLASS_DECLARATION(mca_oob_tcp_recv_t);

/* Queue a message to be sent to a specified peer. The macro
 * checks to see if a message is already in position to be
 * sent - if it is, then the message provided is simply added
 * to the peer's message queue. If not, then the provided message
 * is placed in the "ready" position
 *
 * If the provided boolean is true, then the send event for the
 * peer is checked and activated if not already active. This allows
 * the macro to either immediately send the message, or to queue
 * it as "pending" for later transmission - e.g., after the
 * connection procedure is completed
 *
 * p => pointer to mca_oob_tcp_peer_t
 * s => pointer to mca_oob_tcp_send_t
 * f => true if send event is to be activated
 */
#define MCA_OOB_TCP_QUEUE_MSG(p, s, f)                                  \
    do {                                                                \
        /* if there is no message on-deck, put this one there */        \
        if (NULL == (p)->send_msg) {                                    \
            (p)->send_msg = (s);                                        \
        } else {                                                        \
            /* add it to the queue */                                   \
            opal_list_append(&(p)->send_queue, &(s)->super);            \
        }                                                               \
        if ((f)) {                                                      \
            /* ensure the send event is active */                       \
            if (!(p)->send_ev_active) {                                 \
                opal_event_add(&(p)->send_event, 0);                    \
                (p)->send_ev_active = true;                             \
            }                                                           \
        }                                                               \
    }while(0);

/* queue a message to be sent by one of our modules - must
 * provide the following params:
 *
 * m - the RML message to be sent
 * p - the final recipient
 */
#define MCA_OOB_TCP_QUEUE_SEND(m, p)                                    \
    do {                                                                \
        mca_oob_tcp_send_t *msg;                                        \
        int i;                                                          \
        opal_output_verbose(5, orte_oob_base_framework.framework_output, \
                            "%s:[%s:%d] queue send to %s",              \
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),         \
                            __FILE__, __LINE__,                         \
                            ORTE_NAME_PRINT(&((m)->peer)));             \
        msg = OBJ_NEW(mca_oob_tcp_send_t);                              \
        /* setup the header */                                          \
        msg->hdr.origin = *ORTE_PROC_MY_NAME;                           \
        msg->hdr.dst = (m)->peer;                                       \
        msg->hdr.tag = (m)->tag;                                        \
        /* point to the actual message */                               \
        msg->msg = (m);                                                 \
        /* set the total number of bytes to be sent */                  \
        if (NULL != (m)->buffer) {                                      \
            msg->hdr.nbytes = (m)->buffer->bytes_used;                  \
        } else {                                                        \
            msg->hdr.nbytes = 0;                                        \
            for (i=0; i < (m)->count; i++) {                            \
                msg->hdr.nbytes += (m)->iov[i].iov_len;                 \
            }                                                           \
        }                                                               \
        /* prep header for xmission */                                  \
        MCA_OOB_TCP_HDR_HTON(&msg->hdr);                                \
        /* start the send with the header */                            \
        msg->sdptr = (char*)&msg->hdr;                                  \
        msg->sdbytes = sizeof(mca_oob_tcp_hdr_t);                       \
        /* add to the msg queue for this peer */                        \
        MCA_OOB_TCP_QUEUE_MSG((p), msg, true);                          \
    }while(0);

/* queue a message to be sent by one of our modules upon completing
 * the connection process - must provide the following params:
 *
 * m - the RML message to be sent
 * p - the final recipient
 */
#define MCA_OOB_TCP_QUEUE_PENDING(m, p)                                 \
    do {                                                                \
        mca_oob_tcp_send_t *msg;                                        \
        int i;                                                          \
        opal_output_verbose(5, orte_oob_base_framework.framework_output, \
                            "%s:[%s:%d] queue pending to %s",           \
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),         \
                            __FILE__, __LINE__,                         \
                            ORTE_NAME_PRINT(&((m)->peer)));             \
        msg = OBJ_NEW(mca_oob_tcp_send_t);                              \
        /* setup the header */                                          \
        msg->hdr.origin = *ORTE_PROC_MY_NAME;                           \
        msg->hdr.dst = (m)->peer;                                       \
        msg->hdr.tag = (m)->tag;                                        \
        /* point to the actual message */                               \
        msg->msg = (m);                                                 \
        /* set the total number of bytes to be sent */                  \
        if (NULL != (m)->buffer) {                                      \
            msg->hdr.nbytes = (m)->buffer->bytes_used;                  \
        } else {                                                        \
            msg->hdr.nbytes = 0;                                        \
            for (i=0; i < (m)->count; i++) {                            \
                msg->hdr.nbytes += (m)->iov[i].iov_len;                 \
            }                                                           \
        }                                                               \
        /* prep header for xmission */                                  \
        MCA_OOB_TCP_HDR_HTON(&msg->hdr);                                \
        /* start the send with the header */                            \
        msg->sdptr = (char*)&msg->hdr;                                  \
        msg->sdbytes = sizeof(mca_oob_tcp_hdr_t);                       \
        /* add to the msg queue for this peer */                        \
        MCA_OOB_TCP_QUEUE_MSG((p), msg, false);                         \
    }while(0);

/* queue a message for relay by one of our modules - must
 * provide the following params:
 *
 * m = the mca_oob_tcp_recv_t that was received
 * p - the next hop
*/
#define MCA_OOB_TCP_QUEUE_RELAY(m, p)                                   \
    do {                                                                \
        mca_oob_tcp_send_t *msg;                                        \
        opal_output_verbose(5, orte_oob_base_framework.framework_output, \
                            "%s:[%s:%d] queue relay to %s",             \
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),         \
                            __FILE__, __LINE__,                         \
                            ORTE_NAME_PRINT(&((p)->name)));             \
        msg = OBJ_NEW(mca_oob_tcp_send_t);                              \
        /* setup the header */                                          \
        msg->hdr.origin = (m)->hdr.origin;                              \
        msg->hdr.dst = (m)->hdr.dst;                                    \
        msg->hdr.tag = (m)->hdr.tag;                                    \
        /* point to the actual message */                               \
        msg->data = (m)->data;                                          \
        /* set the total number of bytes to be sent */                  \
        msg->hdr.nbytes = (m)->hdr.nbytes;                              \
        /* prep header for xmission */                                  \
        MCA_OOB_TCP_HDR_HTON(&msg->hdr);                                \
        /* start the send with the header */                            \
        msg->sdptr = (char*)&msg->hdr;                                  \
        msg->sdbytes = sizeof(mca_oob_tcp_hdr_t);                       \
        /* add to the msg queue for this peer */                        \
        MCA_OOB_TCP_QUEUE_MSG((p), msg, true);                          \
    }while(0);

/* State machine for processing message */
typedef struct {
    opal_object_t super;
    opal_event_t ev;
    mca_oob_tcp_module_t *mod;
    orte_rml_send_t *msg;
} mca_oob_tcp_msg_op_t;
OBJ_CLASS_DECLARATION(mca_oob_tcp_msg_op_t);

#define ORTE_ACTIVATE_TCP_POST_SEND(m, ms, cbfunc)                      \
    do {                                                                \
        mca_oob_tcp_msg_op_t *mop;                                      \
        opal_output_verbose(5, orte_oob_base_framework.framework_output, \
                            "%s:[%s:%d] post send to %s",               \
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),         \
                            __FILE__, __LINE__,                         \
                            ORTE_NAME_PRINT(&((ms)->peer)));            \
        mop = OBJ_NEW(mca_oob_tcp_msg_op_t);                            \
        mop->mod = (m);                                                 \
        mop->msg = (ms);                                                \
        opal_event_set((m)->ev_base, &mop->ev, -1,                      \
                       OPAL_EV_WRITE, (cbfunc), mop);                   \
        opal_event_set_priority(&mop->ev, ORTE_MSG_PRI);                \
        opal_event_active(&mop->ev, OPAL_EV_WRITE, 1);                  \
    } while(0);

typedef struct {
    opal_object_t super;
    opal_event_t ev;
    mca_oob_tcp_module_t *mod;
    orte_rml_send_t *rmsg;
    mca_oob_tcp_send_t *snd;
    orte_process_name_t hop;
} mca_oob_tcp_msg_error_t;
OBJ_CLASS_DECLARATION(mca_oob_tcp_msg_error_t);

#define ORTE_ACTIVATE_TCP_MSG_ERROR(m, s, r, h, cbfunc)                 \
    do {                                                                \
        mca_oob_tcp_msg_error_t *mop;                                   \
        mca_oob_tcp_send_t *snd;                                        \
        mca_oob_tcp_recv_t *proxy;                                      \
        opal_output_verbose(5, orte_oob_base_framework.framework_output, \
                            "%s:[%s:%d] post msg error to %s",          \
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),         \
                            __FILE__, __LINE__,                         \
                            ORTE_NAME_PRINT((h)));                      \
        mop = OBJ_NEW(mca_oob_tcp_msg_error_t);                         \
        mop->mod = (m);                                                 \
        if (NULL != (s)) {                                              \
            mop->snd = (s);                                             \
        } else if (NULL != (r)) {                                       \
            /* use a proxy so we can pass NULL into the macro */        \
            proxy = (r);                                                \
            /* create a send object for this message */                 \
            snd = OBJ_NEW(mca_oob_tcp_send_t);                          \
            /* transfer and prep the header */                          \
            snd->hdr = proxy->hdr;                                      \
            MCA_OOB_TCP_HDR_HTON(&snd->hdr);                            \
            /* point to the data */                                     \
            snd->data = proxy->data;                                    \
            /* start the message with the header */                     \
            snd->sdptr = (char*)&snd->hdr;                              \
            snd->sdbytes = sizeof(mca_oob_tcp_hdr_t);                   \
            /* protect the data */                                      \
            proxy->data = NULL;                                         \
        }                                                               \
        mop->hop.jobid = (h)->jobid;                                    \
        mop->hop.vpid = (h)->vpid;                                      \
        opal_event_set(orte_event_base, &mop->ev, -1,                   \
                       OPAL_EV_WRITE, (cbfunc), mop);                   \
        opal_event_set_priority(&mop->ev, ORTE_MSG_PRI);                \
        opal_event_active(&mop->ev, OPAL_EV_WRITE, 1);                  \
    } while(0);

#define ORTE_ACTIVATE_TCP_POST_RESEND(mop, cbfunc)                      \
    do {                                                                \
        mca_oob_tcp_msg_error_t *mp;                                    \
        opal_output_verbose(5, orte_oob_base_framework.framework_output, \
                            "%s:[%s:%d] post resend to %s",             \
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),         \
                            __FILE__, __LINE__,                         \
                            ORTE_NAME_PRINT(&((mop)->hop)));            \
        mp = OBJ_NEW(mca_oob_tcp_msg_error_t);                          \
        mp->mod = (mop)->mod;                                           \
        mp->snd = (mop)->snd;                                           \
        mp->hop = (mop)->hop;                                           \
        opal_event_set(mp->mod->ev_base, &mp->ev, -1,                   \
                       OPAL_EV_WRITE, (cbfunc), mp);                    \
        opal_event_set_priority(&mp->ev, ORTE_MSG_PRI);                 \
        opal_event_active(&mp->ev, OPAL_EV_WRITE, 1);                   \
    } while(0);

#define ORTE_ACTIVATE_TCP_NO_ROUTE(m, r, h, c)                          \
    do {                                                                \
        mca_oob_tcp_msg_error_t *mop;                                   \
        opal_output_verbose(5, orte_oob_base_framework.framework_output, \
                            "%s:[%s:%d] post no route to %s",           \
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),         \
                            __FILE__, __LINE__,                         \
                            ORTE_NAME_PRINT((h)));                      \
        mop = OBJ_NEW(mca_oob_tcp_msg_error_t);                         \
        mop->mod = (m);                                                 \
        mop->rmsg = (r);                                                \
        mop->hop.jobid = (h)->jobid;                                    \
        mop->hop.vpid = (h)->vpid;                                      \
        opal_event_set(orte_event_base, &mop->ev, -1,                   \
                       OPAL_EV_WRITE, (c), mop);                        \
        opal_event_set_priority(&mop->ev, ORTE_MSG_PRI);                \
        opal_event_active(&mop->ev, OPAL_EV_WRITE, 1);                  \
    } while(0);

#endif /* _MCA_OOB_TCP_SENDRECV_H_ */