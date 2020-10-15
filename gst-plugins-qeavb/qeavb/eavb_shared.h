/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * (IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __EAVB_SHARED_H__
#define __EAVB_SHARED_H__


#include <linux/types.h>

// TODO: 
#define EAVB_IOCTL_MAGIC    'B'

// ioctl request
#define EAVB_IOCTL_CREATE_STREAM    _IOWR(EAVB_IOCTL_MAGIC, 1, struct eavb_ioctl_create_stream)
#define EAVB_IOCTL_GET_STREAM_INFO  _IOWR(EAVB_IOCTL_MAGIC, 2, struct eavb_ioctl_get_stream_info)
#define EAVB_IOCTL_CONNECT_STREAM   _IOWR(EAVB_IOCTL_MAGIC, 3, struct eavb_ioctl_connect_stream)
#define EAVB_IOCTL_RECEIVE          _IOWR(EAVB_IOCTL_MAGIC, 4, struct eavb_ioctl_receive)
#define EAVB_IOCTL_RECV_DONE        _IOWR(EAVB_IOCTL_MAGIC, 5, struct eavb_ioctl_recv_done)
#define EAVB_IOCTL_TRANSMIT         _IOWR(EAVB_IOCTL_MAGIC, 6, struct eavb_ioctl_transmit)
#define EAVB_IOCTL_MMAP             _IOWR(EAVB_IOCTL_MAGIC, 7, struct eavb_ioctl_mmap)
#define EAVB_IOCTL_MUNMAP           _IOW(EAVB_IOCTL_MAGIC, 8, struct eavb_ioctl_munmap)
#define EAVB_IOCTL_DISCONNECT_STREAM    _IOW(EAVB_IOCTL_MAGIC, 9, struct eavb_ioctl_disconnect_stream)
#define EAVB_IOCTL_DESTROY_STREAM  _IOW(EAVB_IOCTL_MAGIC, 10, struct eavb_ioctl_destroy_stream)
#define EAVB_IOCTL_CREATE_STREAM_WITH_PATH    _IOWR(EAVB_IOCTL_MAGIC, 11, struct eavb_ioctl_create_stream_with_path)

// default value
#define STATION_ADDR_SIZE       8
#define IF_NAMESIZE             16
#define MAX_CONFIG_FILE_PATH    512

// Invalid value for config file
#define AVB_INVALID_ADDR        (0xFF)
#define AVB_INVALID_INTEGER     (-1)
#define AVB_INVALID_UINT        (-1)


typedef enum {
    AVB_ROLE_TALKER = 0,
    AVB_ROLE_LISTENER,
    AVB_ROLE_CRF_TALKER,
    AVB_ROLE_INVALID = -1
} avb_role_t;

typedef enum {
    NONE = 0,
    PCM,
    H264,
    MPEG2TS,
    MJPEG,
    CRF,
    MAPPING_TYPE_INVALID = -1
} stream_mapping_type_t;

typedef enum {
    RING_BUFFER_MODE_FILL = 0,     // Return error when new data doesn't fit in ring buffer
    RING_BUFFER_MODE_DROP_OLD = 1,  // Drop oldest samples to make room for new data
    RING_BUFFER_MODE_INVALID = -1  // Drop oldest samples to make room for new data
} ring_buffer_mode_t;

typedef enum {
    CLASS_A = 0,
    CLASS_B = 1,
    CLASS_AAF = 2,
    CLASS_INVALID = -1
} avb_class_t;

typedef enum {
	ENDIAN_BIG = 0,
	ENDIAN_LITTLE = 1,
	ENDIAN_INVALID = -1
} data_endianess_t;

enum {
    QAVB_IEEE_1722_ver_2010 = 0,
    QAVB_IEEE_1722_ver_2016,
    QAVB_IEEE_1722_ver_INVALID = -1,
} avb_ieee1722_version;

typedef struct eavb_ioctl_hdr {
    uint64_t streamCtx;
} eavb_ioctl_hdr_t;


/*
 * EAVB_IOCTL_CREATE_STREAM
 */

typedef struct eavb_ioctl_stream_config {
    //common
    uint16_t stream_id; 
    char eth_interface[IF_NAMESIZE];
    uint16_t vlan_id;   
    uint16_t ring_buffer_elem_count;
    ring_buffer_mode_t ring_buffer_mode;
    avb_role_t avb_role;      // talker = 0 or listener = 1
    uint8_t dest_macaddr[STATION_ADDR_SIZE];
    uint8_t stream_addr[STATION_ADDR_SIZE];
    uint8_t crf_dest_macaddr[STATION_ADDR_SIZE];    // "crf_macaddr" or "crf_dest_macaddr"
    uint8_t crf_stream_addr[STATION_ADDR_SIZE];
    stream_mapping_type_t mapping_type;
    int wakeup_interval; // "wakeup_interval" or "tx_interval"
    int tx_pkts_per_sec; //if not set, do default
    int max_stale_ms;     //int max_stale_ns = max_stale_ms*1000;
    int presentation_time_ms; //if not set, do default
    int enforce_presentation_time;
    int sr_class_type; //A = 0  B = 1 AAF = 2
    int packing_factor; // sets number of items to se sent on each tx / rx
    int bandwidth;

    // H.264
    int max_payload;    // "max_payload" or "max_video_payload"

    int mrp_enabled;

    //Audio Specific
    int pcm_bit_depth;
    int pcm_channels;
    int sample_rate; //in hz
    unsigned char endianness;       // 0 = big 1 = little

    int ieee1722_standard;

    // Thread priority in QNX side 
    // TODO: BE or FE
    int talker_priority;
    int listener_priority;
    int crf_priority;

    // CRF
    int crf_mode;               // 0 - disabled
                                // 1 - CRF talker (listener drives reference)
                                // 2 - CRF with talker reference (talker has CRF talker)

    int crf_type;               // 0 - custom
                                // 1- audio
                                // 2- video frame
                                // 3 - video line
                                // 4 - machine cycle

    int crf_timestamping_interval; // time interval after how many events timestamp is to be            produced
    int crf_timestamping_interval_remote; // time interval after how many events timestamp is to be            produced
                                // (base_frequency * pull) / timestamp_interval =
                                // # of timestamps per second
    int crf_timestamping_interval_local;
    int crf_allow_dynamic_tx_adjust; // enables/disables dynamic IPG adjustments
    int crf_num_timestamps_per_pkt;   // indicates how many CRF timestamps per each CRF packet

    int64_t crf_mcr_adjust_min_ppm;
    int64_t crf_mcr_adjust_max_ppm;

    uint16_t crf_stream_id;     // CRF stream ID
    int32_t crf_base_frequency; // CRF base frequency

    int32_t crf_listener_ts_smoothing;
    int32_t crf_talker_ts_smoothing;

    int crf_pull;              // multiplier for the base frequency;
    int crf_event_callback_interval;   // indicates how often to issue MCR callback events
                               // how many packets will generate one callback.
    int crf_dynamic_tx_adjust_interval; // Indicates how often to update IPG

    // stats
    int32_t enable_stats_reporting;
    int32_t stats_reporting_interval;
    int32_t stats_reporting_samples;

    // packet tracking
    int32_t enable_packet_tracking;
    int32_t packet_tracking_interval;
    int blocking_write_enabled;
    double blocking_write_fill_level;
    int app_tx_block_enabled;
    int stream_interleaving_enabled;
    int create_talker_thread;
    int create_crf_threads;
    int listener_bpf_pkts_per_buff;
} __attribute__ ((__packed__)) eavb_ioctl_stream_config_t;

typedef struct eavb_ioctl_create_stream {
    eavb_ioctl_stream_config_t config;  //IN
    eavb_ioctl_hdr_t hdr;               //OUT
} eavb_ioctl_create_stream_t;

typedef struct eavb_ioctl_create_stream_with_path {
    char path[MAX_CONFIG_FILE_PATH];	//IN
    eavb_ioctl_hdr_t hdr;               //OUT
} eavb_ioctl_create_stream_with_path_t;

/*
 * EAVB_IOCTL_GET_STREAM_INFO
 */

typedef struct eavb_ioctl_stream_info {
    //common
    avb_role_t role;
    stream_mapping_type_t mapping_type;
    unsigned int max_payload;      /* Max packet payload size */
    unsigned int pkts_per_wake;    /* Number of packets sent per wake */
    unsigned int wakeup_period_us; /* Time to sleep between wakes */

    //Audio Specific
    int pcm_bit_depth;             /* Audio bit depth 8/16/24/32 */
    int num_pcm_channels;          /* Audio channels 1/2 */
    int sample_rate;               /* Audio sample rate in hz */
    unsigned char endianness;      /* Audio sample endianness 0(big)/1(little) */

    unsigned int max_buffer_size;         /* Max buffer size (Bytes) allowed */
} __attribute__ ((__packed__)) eavb_ioctl_stream_info_t;

typedef struct eavb_ioctl_get_stream_info {
    eavb_ioctl_hdr_t hdr;           //IN
    eavb_ioctl_stream_info_t info;  //OUT
} eavb_ioctl_get_stream_info_t;


/*
 * EAVB_IOCTL_CONNECT_STREAM
 */

typedef struct eavb_ioctl_connect_stream {
    eavb_ioctl_hdr_t hdr;   //IN
} eavb_ioctl_connect_stream_t;


/*
 * EAVB_IOCTL_MMAP
 */

typedef struct eavb_ioctl_mem {
    int fd;         /* (optional)fd of ION */
    uint64_t va;   /* virtual address of continuous buffer */
    size_t size;    /* buffer size (it should less than "max_buffer_size") */
} __attribute__ ((__packed__)) eavb_ioctl_mem_t;


typedef struct eavb_ioctl_mmap {
    eavb_ioctl_hdr_t hdr;   //IN
    size_t alloc_size;      //IN
    eavb_ioctl_mem_t mem;   //OUT
} eavb_ioctl_mmap_t;


/*
 * EAVB_IOCTL_MUNMAP
 */

typedef struct eavb_ioctl_munmap {
    eavb_ioctl_hdr_t hdr;   //IN
    eavb_ioctl_mem_t mem;   //IN
} eavb_ioctl_munmap_t;


/*
 * EAVB_IOCTL_RECEIVE
 */

typedef struct eavb_ioctl_buf_hdr {
    uint32_t flag_end_of_frame:1;   // This flag is used for H.264 and MJPEG streams:
                                    // 1. H.264: Set for the very last packet of an access unit. 
                                    // 2. MJPEG  Set the last packet of a video frame. 
                                    // 
                                    //****************************************************************
    uint32_t flag_end_of_file:1;    // This flag is used in file transfer only:
                                    // Set for the last packet in the file 
                                    //****************************************************************
    uint32_t flag_reserved:30;
                                    //****************************************************************
    uint32_t event;                 // Audio event      Layout D3scription      Valid Channels
                                    // event value
                                    //  0               Static layout           Based on config
                                    //  1               Mono                    0               
                                    //  2               Stereo                  0, 1
                                    //  3               5.1                     0,1,2,3,4,5
                                    //  4               7.1                     0,1,2,3,4,5,6,7
                                    //  5-15            Custom                  Defined by System 
                                    //                                          Integrator
                                    //****************************************************************
    uint32_t reserved;
    uint32_t payload_size;          // Size of the payload (bytes)
} __attribute__ ((__packed__)) eavb_ioctl_buf_hdr_t;

typedef struct eavb_ioctl_buf_data {
    eavb_ioctl_buf_hdr_t hdr;
    uint64_t pbuf;         /* virtual address of buffer */
} __attribute__ ((__packed__)) eavb_ioctl_buf_data_t;

typedef struct eavb_ioctl_receive {
    eavb_ioctl_hdr_t hdr;       //IN
    //eavb_ioctl_mem_t mem;       //IN
    eavb_ioctl_buf_data_t data; //IN/OUT
    int32_t received;				//OUT
} eavb_ioctl_receive_t;


/*
 * EAVB_IOCTL_RECV_DONE
 */

typedef struct eavb_ioctl_recv_done {
    eavb_ioctl_hdr_t hdr;       //IN
    //eavb_ioctl_mem_t mem;       //IN
    eavb_ioctl_buf_data_t data; //IN
} eavb_ioctl_recv_done_t;


/*
 * EAVB_IOCTL_TRANSMIT
 */

typedef struct eavb_ioctl_transmit {
    eavb_ioctl_hdr_t hdr;       //IN
    //eavb_ioctl_mem_t mem;       //IN
    eavb_ioctl_buf_data_t data; //IN/OUT
    int32_t written;		//OUT
} eavb_ioctl_transmit_t;


/*
 * EAVB_IOCTL_DISCONNECT_STREAM
 */

typedef struct eavb_ioctl_disconnect_stream {
    eavb_ioctl_hdr_t hdr;       //IN
} eavb_ioctl_disconnect_stream_t;


/*
 * EAVB_IOCTL_DESTROY_STREAM
 */

typedef struct eavb_ioctl_destroy_stream {
    eavb_ioctl_hdr_t hdr;       //IN
} eavb_ioctl_destroy_stream_t;


/////////////////////////////////////////////
/* Utility Function */
/////////////////////////////////////////////


/**
 * eavb_ioctl_stream_config_t initialization function (set invalid value)
 */
//TODO: need to set all parameter.
#define EAVB_IOCTL_STREAM_CONFIG_INITIALIZER { .stream_id=AVB_INVALID_INTEGER, \
    .eth_interface={'\0'}, \
}





#endif /*__EAVB_SHARED_H__*/
