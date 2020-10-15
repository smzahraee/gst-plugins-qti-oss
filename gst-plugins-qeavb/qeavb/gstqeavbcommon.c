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

#include "gstqeavbcommon.h"

static int qeavb_read_line(char *buffer, int maxSize, FILE *fp)
{
  char tmp;
  int i = 0;
  int readSize = 0;
  int not_done = 1;
  while( i < maxSize && not_done){
    readSize = fread(&tmp, sizeof(char), 1, fp);
    if(readSize == 1){ //read successful and not eof
      *(buffer+i) = tmp;
      if(tmp == '\n' || tmp == '\r' || tmp == '\a'||tmp == '\0'){
        not_done = 0;
      }
    }
    //EOL
    if(i == 0 && readSize == 0){
      return 0;
    }
    i++;
  }
  *(buffer+i-1) = '\0';
  return i;
}

static __inline__ void qeavb_read_uint16(char* token, guint16 *val) {
    if(token) {
        *val = (guint16) atoi(token);
    }
}

static __inline__ void qeavb_read_int(char* token, int *val) {
  if(token) {
    *val = atoi(token);
  }
}

static __inline__ void qeavb_read_double(char* token, double *val) {
  if(token) {
    *val = atof(token);
  }
}

static __inline__ void qeavb_read_string(char* token, char* val, size_t val_len) {
  if (token) {
    g_strlcpy(val, token, val_len);
    val[val_len] = '\0';
    //strlcpy(val, token, val_len);
  }
}

static __inline__ int qeavb_read_mac(char* token, guint8* mac) {
  if (!token) {
    return -2;
  }
  unsigned int value[6];
  if(6 == sscanf(token, "%x:%x:%x:%x:%x:%x",
      value, value+1, value+2, value+3, value+4, value+5)){
    for (int i = 0; i < 6; i++) {
      mac[i] = (guint8) value[i];
    }
    return 0;
  } else{
    return -2;
  }
}

int qeavb_read_config_file(eavb_ioctl_stream_config_t* streamCtx, char* filepath)
{
  char *linebuff = NULL;
  char * token = NULL;
  FILE *fp = NULL;
  eavb_ioctl_stream_config_t* cfg_data = streamCtx;
  int bytesRead = 1;
  int err = 0;
  static char* delims = {" =\n\0\r"};

  memset(cfg_data, 0, sizeof(eavb_ioctl_stream_config_t));
  cfg_data->packing_factor = 1; // default packing factor to 1 which implies ruled by class - no overload
  cfg_data->wakeup_interval = 1; // default to 1 ms given QNX real clock resolution set 1ms
  cfg_data->tx_pkts_per_sec = 0;
  cfg_data->presentation_time_ms = -1;
  cfg_data->enforce_presentation_time = 0;
  cfg_data->max_stale_ms = 5000;
  cfg_data->mrp_enabled = 1;
  cfg_data->max_payload = 1412;
  cfg_data->bandwidth = 200000;
  cfg_data->vlan_id = 2;
  cfg_data->ieee1722_standard = 1;
  cfg_data->crf_type = 5;
  cfg_data->crf_event_callback_interval = 300;
  cfg_data->crf_dynamic_tx_adjust_interval = 1;
  cfg_data->crf_mcr_adjust_min_ppm = -300;
  cfg_data->crf_mcr_adjust_max_ppm = 300;
  cfg_data->crf_allow_dynamic_tx_adjust = 1;
  cfg_data->ring_buffer_elem_count = 10000;
  cfg_data->ring_buffer_mode = 0;
  cfg_data->endianness = 1;
  cfg_data->pcm_bit_depth = 8;
  cfg_data->enable_stats_reporting = 0;
  cfg_data->stats_reporting_interval = 30;
  cfg_data->stats_reporting_samples = 80000;
  cfg_data->enable_packet_tracking = 0;
  cfg_data->packet_tracking_interval = 30;
  cfg_data->talker_priority = 150;
  cfg_data->listener_priority = 150;
  cfg_data->crf_priority = 150;
  cfg_data->blocking_write_fill_level = 0.5;
  cfg_data->blocking_write_enabled = 0;
  cfg_data->create_crf_threads = 1;
  cfg_data->create_talker_thread = 1;
  cfg_data->listener_bpf_pkts_per_buff = 0;
  memset(cfg_data->dest_macaddr, 0, 6);
  memset(cfg_data->crf_dest_macaddr, 0, 6);

  linebuff = malloc(128);
  if (linebuff == NULL) {
    GST_ERROR("Allocate config read buffer\n");
    err = -1;
    goto cleanup;
  }

  GST_DEBUG("opening config file %s... \n",filepath);
  fp = fopen(filepath, "r");
  if(fp == NULL) {
    GST_ERROR("could not open %s for read\n",filepath);
    err = -1;
    goto cleanup;
  }

  while (err == 0) {
    char *saveptr = NULL;
    *linebuff = '\0';
    //get the new line, get 0 if eof and returns
    bytesRead = qeavb_read_line(linebuff, 128, fp);
    if(bytesRead <= 0) {
      break;
    }

    //Tokenize the first element, if it starts with #, skip the line
    //otherwise strcmp against known table of keywords
    token = strtok_r(linebuff, delims, &saveptr);
    if(token == NULL){
      continue;
    }

    //check if it's a comment, if so, ignore it
    if((char)*token == '#' || (char)*token == '/' || (char)*token == '*'){
      continue;
    }

    //check token against keywords
    if(strcmp("stream_id",token) == 0){
      qeavb_read_uint16(strtok_r(NULL, delims, &saveptr), &cfg_data->stream_id);
      GST_DEBUG("STREAM ID - %d\n",cfg_data->stream_id);
    } else if(strcmp("eth_interface",token) == 0){
      qeavb_read_string(strtok_r(NULL, delims, &saveptr), cfg_data->eth_interface, 16);
      GST_DEBUG("eth_interface: %s\n",cfg_data->eth_interface);
    } else if(strcmp("vlan_id",token) == 0){
      qeavb_read_uint16(strtok_r(NULL, delims, &saveptr), &cfg_data->vlan_id);
      GST_DEBUG("vlan_id: %d\n",cfg_data->vlan_id);
    } else if(strcmp("ring_buffer_elem_count",token) == 0){
      qeavb_read_uint16(strtok_r(NULL, delims, &saveptr), &cfg_data->ring_buffer_elem_count);
      GST_DEBUG("ring_buffer_elem_count: %d\n",cfg_data->ring_buffer_elem_count);
    } else if(strcmp("ring_buffer_mode",token) == 0){
      int mode = 0;
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &mode);
      cfg_data->ring_buffer_mode = mode;
      GST_DEBUG("ring_buffer_mode: %d\n",cfg_data->ring_buffer_mode);
    } else if(strcmp("avb_role",token) == 0) {
      token = strtok_r(NULL, delims, &saveptr);
      if (!token) {
        GST_ERROR("Unable to tokenize avb_role\n");
        err = -2;
        goto cleanup;
      }
      if(strcmp("talker", token) == 0) {
        cfg_data->avb_role = AVB_ROLE_TALKER;
        GST_DEBUG("Role is talker\n");
      } else if(strcmp("listener", token) == 0) {
        cfg_data->avb_role = AVB_ROLE_LISTENER;
        GST_DEBUG("Role is listener\n");
      } else if(strcmp("crf_talker", token) == 0) {
        cfg_data->avb_role = AVB_ROLE_CRF_TALKER;
        GST_DEBUG("Role is CRF talker\n");
      } else {
        GST_ERROR("Improper value for avb_role\n");
        err = -2;
        goto cleanup;
      }
    } else if(strcmp("dest_macaddr",token) == 0) {
      err = qeavb_read_mac(strtok_r(NULL, delims, &saveptr), &cfg_data->dest_macaddr[0]);
      if (err != 0) {
        GST_ERROR("Invalid Destination MAC addr\n");
      } else {
        GST_DEBUG("dest_macaddr: "MAC_STR"\n",MAC_TO_STR(cfg_data->dest_macaddr));
      }
    } else if(strcmp("stream_addr",token) == 0) {
      err = qeavb_read_mac(strtok_r(NULL, delims, &saveptr), &cfg_data->stream_addr[0]);
      if (err != 0) {
        GST_ERROR("Invalid Stream MAC addr\n");
      } else {
        GST_DEBUG("stream_macaddr: "MAC_STR"\n",MAC_TO_STR(cfg_data->stream_addr));
      }
    } else if((strcmp("crf_macaddr",token) == 0) ||
        (strcmp("crf_dest_macaddr",token) == 0)) {
      err = qeavb_read_mac(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_dest_macaddr[0]);
      if (err != 0) {
        GST_ERROR("Invalid Destination CRF MAC addr\n");
      } else {
        GST_DEBUG("crf_macaddr: "MAC_STR"\n",MAC_TO_STR(cfg_data->crf_dest_macaddr));
      }
    } else if(strcmp("crf_stream_addr",token) == 0) {
      err = qeavb_read_mac(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_stream_addr[0]);
      if (err != 0) {
        GST_ERROR("Invalid CRF Stream MAC addr\n");
      } else {
        GST_DEBUG("crf_stream_macaddr: "MAC_STR"\n",MAC_TO_STR(cfg_data->crf_stream_addr));
      }
    } else if(strcmp("mapping_type",token) == 0){
      token = strtok_r(NULL, delims, &saveptr);
      if (!token) {
        GST_ERROR("Unable to tokenize mapping_type\n");
        err = -2;
        goto cleanup;
      }
      if(strcmp("PCM",token) == 0){
        cfg_data->mapping_type= PCM;
      } else if(strcmp("MPEG2TS",token) == 0){
        cfg_data->mapping_type = MPEG2TS;
      } else if(strcmp("H264",token) == 0){
        cfg_data->mapping_type = H264;
      } else if(strcmp("MJPEG",token) == 0){
        cfg_data->mapping_type = MJPEG;
      } else{
        GST_ERROR("Mapping type of %s is not valid\n",token);
        err = -2;
        goto cleanup;
      }
      GST_DEBUG("stream type: %d %s\n",(int)cfg_data->mapping_type,token);
    } else if((strcmp("wakeup_interval",token) == 0) ||
        (strcmp("tx_interval",token) == 0)) {
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->wakeup_interval);
      if (cfg_data->wakeup_interval <= 0) {
        GST_DEBUG("invalid wakeup_interval: %d ms, defaulting to 1\n",
            cfg_data->wakeup_interval);
        cfg_data->wakeup_interval = 1;
      } else {
        GST_DEBUG("wakeup_interval: %d ms\n",cfg_data->wakeup_interval);
      }
    } else if(strcmp("tx_pkts_per_sec",token) == 0) {
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->tx_pkts_per_sec);
      GST_DEBUG("tx_pkts_per_sec: %d ms\n",cfg_data->tx_pkts_per_sec);
    } else if(strcmp("max_stale_time_ms",token) == 0) {
      int stale_ms = 0;
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &stale_ms);
      if (stale_ms < 0 || stale_ms > MAX_STALE_MS) {
        GST_ERROR("invalid max_stale_time_ms: %d ms. Must be between 0 and %d\n",
            stale_ms, MAX_STALE_MS);
      } else {
        cfg_data->max_stale_ms = stale_ms;
        GST_DEBUG("max_stale_time_ms: %d ms\n",stale_ms);
      }
    } else if(strcmp("presentation_time_ms",token) == 0) {
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->presentation_time_ms);
      GST_DEBUG("presentation_time_ms: %d ms\n",cfg_data->presentation_time_ms);
    } else if(strcmp("enforce_presentation_time",token) == 0) {
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->enforce_presentation_time);
      GST_DEBUG("enforce_presentation_time: %d \n",cfg_data->enforce_presentation_time);
    } else if(strcmp("sr_class_type",token) == 0){
      token = strtok_r(NULL, delims, &saveptr);
      if (!token) {
        GST_ERROR("Unable to tokenize sr_class_type\n");
        err = -2;
        goto cleanup;
      }
      if ((strcmp("A", token) == 0) || (strcmp("a", token) == 0)) {
        cfg_data->sr_class_type = 0;
      } else if ((strcmp("B", token) == 0) || (strcmp("b", token) == 0)) {
        cfg_data->sr_class_type = 1;

      } else if ((strcmp("AAF", token) == 0) || (strcmp("aaf", token) == 0)) {
        cfg_data->sr_class_type = 2;
      } else{
        GST_ERROR("Class %s not supported\n", token);
        err = -2;
        goto cleanup;
      }
    } else if(strcmp("packing_factor",token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->packing_factor);
      GST_DEBUG("packing_factor: %d \n",cfg_data->packing_factor);
    } else if(strcmp("bandwidth",token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->bandwidth);
      GST_DEBUG("bandwidth: %d \n",cfg_data->bandwidth);
    } else if((strcmp("max_video_payload",token) == 0) ||
        (strcmp("max_payload",token) == 0)){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->max_payload);
      GST_DEBUG("max_video_payload: %d bytes \n",cfg_data->max_payload);
    } else if(strcmp("mrp_enabled",token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->mrp_enabled);
      GST_DEBUG("mrp_enabled - %d\n", cfg_data->mrp_enabled);
    } else if(strcmp("pcm_bit_depth",token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->pcm_bit_depth);
      GST_DEBUG("pcm_bit_depth: %d \n", cfg_data->pcm_bit_depth);
    } else if(strcmp("pcm_channels",token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->pcm_channels);
      GST_DEBUG("num_pcm_channels: %d \n",cfg_data->pcm_channels);
    } else if(strcmp("sample_rate",token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->sample_rate);
      GST_DEBUG("sample_rate: %d hz \n",cfg_data->sample_rate);
    } else if(strcmp("talker_priority",token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->talker_priority);
      GST_DEBUG("talker_priority: %d \n",cfg_data->talker_priority);
    } else if(strcmp("listener_priority",token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->listener_priority);
      GST_DEBUG("listener_priority: %d \n",cfg_data->listener_priority);
    } else if(strcmp("crf_priority",token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_priority);
      GST_DEBUG("crf_priority: %d \n",cfg_data->crf_priority);
    } else if(strcmp("endianness", token) == 0){
      token = strtok_r(NULL, delims, &saveptr);
      if (!token) {
        GST_ERROR("Unable to tokenize endianness\n");
        err = -2;
        goto cleanup;
      }
      if(strcmp("big", token) == 0){
        cfg_data->endianness = 0;
      }
      else if(strcmp("little", token) == 0){
        cfg_data->endianness = 1;
      }
      GST_DEBUG("endianness of audio data is %s\n", token);
    } else if (strcmp("ieee1722_standard", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->ieee1722_standard);
      if ((cfg_data->ieee1722_standard != 0) && (cfg_data->ieee1722_standard != 1)) {
        cfg_data->ieee1722_standard = 1;
      }
      GST_DEBUG("ieee1722 version: %d \n",(cfg_data->ieee1722_standard == QAVB_IEEE_1722_ver_2010)? 2010 : 2016);
    } else if (strcmp("ieee1722_standard", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->ieee1722_standard);
      if ((cfg_data->ieee1722_standard != 0) && (cfg_data->ieee1722_standard != 1)) {
        cfg_data->ieee1722_standard = 1;
      }
      GST_DEBUG("ieee1722 version: %d \n",(cfg_data->ieee1722_standard == 0)? 2010 : 2016);
    } else if (strcmp("crf_mode", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_mode);
      if (cfg_data->crf_mode >= 4) {
        GST_DEBUG("Invalid CRF mode %d - CRF disabled \n", cfg_data->crf_mode);
        cfg_data->crf_mode = 0;
      }
      GST_DEBUG("crf_mode : %s \n",(cfg_data->crf_mode == 0)? "CRF disabled" : (cfg_data->crf_mode != 2 &&
            cfg_data->crf_mode != 3)? "CRF talker" : " CRF listener");
    } else if (strcmp("crf_type", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_type);
      if (cfg_data->crf_type >= 5) {
        GST_DEBUG("Invalid CRF type %d - CRF disabled \n", cfg_data->crf_type);
        cfg_data->crf_mode = 0;
      } else {
        GST_DEBUG("crf_type : %d \n", cfg_data->crf_type);
      }
    } else if (strcmp("crf_timestamping_interval", token) == 0){
      GST_DEBUG("Notice: Deprecated config parameter - " \
          "please use crf_timestamping_interval_local and " \
          "crf_timestamping_interval_remote instead");
      int interval = 0;
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &interval);
      if (interval == 0) {
        GST_DEBUG("Invalid CRF crf_timestamping_interval 0 - CRF disabled");
        cfg_data->crf_mode = 0;
      } else {
        // Old config item, try to set both local and remote intervals
        // but do not overwrite if they had previous values.
        if (cfg_data->crf_timestamping_interval_local == 0) {
          cfg_data->crf_timestamping_interval_local = interval;
          GST_DEBUG("crf_timestamping_interval_local : %d \n", cfg_data->crf_timestamping_interval_local);
        }
        if (cfg_data->crf_timestamping_interval_remote == 0) {
          cfg_data->crf_timestamping_interval_remote = interval;
          GST_DEBUG("crf_timestamping_interval_remote : %d \n", cfg_data->crf_timestamping_interval_remote);
        }
      }
    } else if (strcmp("crf_timestamping_interval_local", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_timestamping_interval_local);
      if (cfg_data->crf_timestamping_interval_local == 0) {
        GST_DEBUG("Invalid CRF crf_timestamping_interval %d - CRF disabled \n", cfg_data->crf_timestamping_interval_local);
        cfg_data->crf_mode = QEAVB_CRF_MODE_DISABLED;
      } else {
        GST_DEBUG("crf_timestamping_interval_local : %d \n", cfg_data->crf_timestamping_interval_local);
      }
    } else if (strcmp("crf_timestamping_interval_remote", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_timestamping_interval_remote);
      if (cfg_data->crf_timestamping_interval_remote == 0) {
        GST_DEBUG("Invalid CRF crf_timestamping_interval_remote %d - CRF disabled \n", cfg_data->crf_timestamping_interval_remote);
        cfg_data->crf_mode = QEAVB_CRF_MODE_DISABLED;
      } else {
        GST_DEBUG("crf_timestamping_interval_remote : %d \n", cfg_data->crf_timestamping_interval_remote);
      }
    } else if (strcmp("crf_allow_dynamic_tx_adjust", token) == 0){
      int dyn_tx = 0;
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &dyn_tx);
      if (dyn_tx == 0) {
        cfg_data->crf_allow_dynamic_tx_adjust = 0;
      } else {
        cfg_data->crf_allow_dynamic_tx_adjust = 1;
      }
      GST_DEBUG("crf_allow_dynamic_tx_adjust : %d \n", cfg_data->crf_allow_dynamic_tx_adjust);
    } else if (strcmp("crf_num_timestamps_per_pkt", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_num_timestamps_per_pkt);
      if (cfg_data->crf_num_timestamps_per_pkt == 0) {
        GST_DEBUG("Invalid CRF crf_num_timestamps_per_pkt %d - CRF disabled \n", cfg_data->crf_num_timestamps_per_pkt);
        cfg_data->crf_mode = QEAVB_CRF_MODE_DISABLED;
      } else {
        GST_DEBUG("crf_num_timestamps_per_pkt : %d \n", cfg_data->crf_num_timestamps_per_pkt);
      }
    } else if (strcmp("crf_mcr_adjust_min_ppm", token) == 0){
      int ppm = 0;
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &ppm);
      if (ppm > 0) {
        GST_DEBUG("Invalid crf_mcr_adjust_min_ppm %d, using default %lld \n",
            ppm, cfg_data->crf_mcr_adjust_min_ppm);
      } else {
        cfg_data->crf_mcr_adjust_min_ppm = ppm;
        GST_DEBUG("crf_mcr_adjust_min_ppm : %lld \n", cfg_data->crf_mcr_adjust_min_ppm);
      }
    } else if (strcmp("crf_mcr_adjust_max_ppm", token) == 0){
      int ppm = 0;
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &ppm);
      if (ppm < 0) {
        GST_DEBUG("Invalid crf_mcr_adjust_max_ppm %d, using default %lld \n",
            ppm, cfg_data->crf_mcr_adjust_max_ppm);
      } else {
        cfg_data->crf_mcr_adjust_max_ppm = ppm;
        GST_DEBUG("crf_mcr_adjust_max_ppm : %lld \n", cfg_data->crf_mcr_adjust_max_ppm);
      }
    } else if (strcmp("crf_stream_id", token) == 0){
      qeavb_read_uint16(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_stream_id);
      if (cfg_data->crf_stream_id == 0) {
        GST_DEBUG("Invalid CRF crf_stream_id %d - CRF disabled \n", cfg_data->crf_stream_id);
        cfg_data->crf_mode = 0;
      } else {
        GST_DEBUG("crf_stream_id : %d \n", cfg_data->crf_stream_id);
      }
    } else if (strcmp("crf_base_frequency", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_base_frequency);
      if (cfg_data->crf_base_frequency == 0) {
        GST_DEBUG("Invalid CRF base frequency %d - CRF disabled \n", cfg_data->crf_base_frequency);
        cfg_data->crf_mode = QEAVB_CRF_MODE_DISABLED;
      } else {
        GST_DEBUG("crf_base_frequency : %d \n", cfg_data->crf_base_frequency);
      }
    } else if (strcmp("crf_listener_ts_smoothing", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_listener_ts_smoothing);
      if (cfg_data->crf_listener_ts_smoothing < 2) {
        GST_DEBUG("Invalid crf_listener_ts_smoothing %d - setting to 2\n",
            cfg_data->crf_listener_ts_smoothing);
        cfg_data->crf_listener_ts_smoothing = 2;
      }
      GST_DEBUG("crf_listener_ts_smoothing : %d \n", cfg_data->crf_listener_ts_smoothing);
    } else if (strcmp("crf_talker_ts_smoothing", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_talker_ts_smoothing);
      if (cfg_data->crf_talker_ts_smoothing < 2) {
        GST_DEBUG("Invalid crf_talker_ts_smoothing %d - setting to 2\n",
            cfg_data->crf_talker_ts_smoothing);
        cfg_data->crf_talker_ts_smoothing = 2;
      }
      GST_DEBUG("crf_talker_ts_smoothing : %d \n", cfg_data->crf_talker_ts_smoothing);
    } else if (strcmp("crf_pull", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_pull);
      if (cfg_data->crf_pull >= QEAVB_CRF_PULL_MAX) {
        GST_DEBUG("Invalid CRF crf_pull %d - CRF disabled \n", cfg_data->crf_pull);
        cfg_data->crf_mode = QEAVB_CRF_MODE_DISABLED;
      } else {
        GST_DEBUG("crf_pull : %d \n", cfg_data->crf_pull);
      }
    } else if (strcmp("crf_event_callback_interval", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_event_callback_interval);
      GST_DEBUG("crf_event_callback_interval: %d \n",cfg_data->crf_event_callback_interval);
    } else if (strcmp("crf_dynamic_tx_adjust_interval", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->crf_dynamic_tx_adjust_interval);
      GST_DEBUG("crf_dynamic_tx_adjust_interval: %d \n",cfg_data->crf_dynamic_tx_adjust_interval);
    } else if (strcmp("enable_stats_reporting", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->enable_stats_reporting);
      GST_DEBUG("enable_stats_reporting: %d \n",cfg_data->enable_stats_reporting);
    } else if (strcmp("stats_reporting_interval", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->stats_reporting_interval);
      GST_DEBUG("stats_reporting_interval: %d \n",cfg_data->stats_reporting_interval);
    } else if (strcmp("stats_reporting_samples", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->stats_reporting_samples);
      GST_DEBUG("stats_reporting_samples: %d \n",cfg_data->stats_reporting_samples);
    } else if (strcmp("enable_packet_tracking", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->enable_packet_tracking);
      GST_DEBUG("enable_packet_tracking: %d \n",cfg_data->enable_packet_tracking);
    } else if (strcmp("packet_tracking_interval", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->packet_tracking_interval);
      GST_DEBUG("packet_tracking_interval: %d \n",cfg_data->packet_tracking_interval);
    } else if (strcmp("blocking_write_enabled", token) == 0){
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->blocking_write_enabled);
      GST_DEBUG("blocking_write_enabled: %d \n",cfg_data->blocking_write_enabled);
    } else if (strcmp("blocking_write_fill_level", token) == 0){
      qeavb_read_double(strtok_r(NULL, delims, &saveptr), &cfg_data->blocking_write_fill_level);
      if ((cfg_data->blocking_write_fill_level <= 0.0) || (cfg_data->blocking_write_fill_level >= 1.0)) {
        GST_DEBUG("Invalid blocking_write_fill_level %f, must be between 0 and 1.",
          cfg_data->blocking_write_fill_level);
        cfg_data->blocking_write_fill_level = 0.5;
      }
      GST_DEBUG("blocking_write_fill_level: %f \n",cfg_data->blocking_write_fill_level);
    } else if (strcmp("stream_interleaving_enabled", token) == 0) {
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->stream_interleaving_enabled);
      GST_DEBUG("stream_interleaving_enabled: %d \n",cfg_data->stream_interleaving_enabled);
    } else if (strcmp("create_talker_thread", token) == 0) {
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->create_talker_thread);
      GST_DEBUG("create_talker_thread: %d \n",cfg_data->create_talker_thread);
    }
    else if (strcmp("create_crf_threads", token) == 0) {
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->create_crf_threads);
      GST_DEBUG("create_crf_threads: %d \n",cfg_data->create_crf_threads);
    }
    else if (strcmp("listener_bpf_pkts_per_buff", token) == 0) {
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->listener_bpf_pkts_per_buff);
      GST_DEBUG("listener_bpf_pkts_per_buff: %d \n",cfg_data->listener_bpf_pkts_per_buff);
    }
    else if (strcmp("app_tx_block_enabled", token) == 0) {
      qeavb_read_int(strtok_r(NULL, delims, &saveptr), &cfg_data->app_tx_block_enabled);
      GST_DEBUG("listener_bpf_pkts_per_buff: %d \n",cfg_data->app_tx_block_enabled);
    }
  }

  // Do some post-configuration checks

  // CRF configuration
  if (cfg_data->crf_mode != 0) {
    if (cfg_data->mapping_type == PCM) {
      // Calculate default CRF values if not already configured via ini file
      // Recommended timestamps per second is 300, so the timestamping
      // interval is sample rate / 300.
      // ref: ieee 1722.2016 section 10.5, table 28
      if (cfg_data->crf_timestamping_interval_remote == 0) {
        cfg_data->crf_timestamping_interval_remote = cfg_data->sample_rate / 300;
        GST_DEBUG("crf_timestamping_interval_remote : %d",
            cfg_data->crf_timestamping_interval_remote);
      }
      if (cfg_data->crf_timestamping_interval_local == 0) {
        cfg_data->crf_timestamping_interval_local = cfg_data->sample_rate / 300;
        GST_DEBUG("crf_timestamping_interval_local : %d",
            cfg_data->crf_timestamping_interval_local);
      }
      if (cfg_data->crf_num_timestamps_per_pkt == 0) {
        if (cfg_data->avb_role == AVB_ROLE_CRF_TALKER)
          cfg_data->crf_num_timestamps_per_pkt = 1;
        else
          cfg_data->crf_num_timestamps_per_pkt = 6;
        GST_DEBUG("crf_num_timestamps_per_pkt : %d",
            cfg_data->crf_num_timestamps_per_pkt);
      }
      if (cfg_data->crf_type >= 5) {
        cfg_data->crf_type = 1;
        GST_DEBUG("crf_type : %d",
            cfg_data->crf_type);
      }

      if (cfg_data->crf_talker_ts_smoothing == 0) {
        int ts_smoothing = 0;
        // Determine default timestamp smoothing values based CRF mode
        if (cfg_data->crf_mode == 3) {
          // When comparing to nominal to remote rate, do the best we can
          // and only average over 1 packet worth of timestamps, which
          // by default will be 6 timestamps
          ts_smoothing = cfg_data->crf_num_timestamps_per_pkt;
        } else {
          // When comparing local to remote, use large values
          // to smoothen out app wakeup variability.
          ts_smoothing = 600;
        }
        cfg_data->crf_talker_ts_smoothing = ts_smoothing;
        GST_DEBUG("crf_talker_ts_smoothing : %d", cfg_data->crf_talker_ts_smoothing);
      }

      if (cfg_data->crf_listener_ts_smoothing == 0) {
        int ts_smoothing = 0;
        // Determine default timestamp smoothing values based CRF mode
        if (cfg_data->crf_mode == 3) {
          // this value is unused in nominal mode
          ts_smoothing = 1;
        } else {
          // When comparing local to remote, use large values
          // to smoothen out app wakeup variability.
          ts_smoothing = 600;
        }
        cfg_data->crf_listener_ts_smoothing = ts_smoothing;
        GST_DEBUG("crf_listener_ts_smoothing : %d", cfg_data->crf_listener_ts_smoothing);
      }
    }
  }

  if (cfg_data->presentation_time_ms == -1) {
    // Calculate presentation time based on class
    switch(cfg_data->sr_class_type) {
      case 0:
        cfg_data->presentation_time_ms = 2;
        break;
      case 1:
        cfg_data->presentation_time_ms = 10;
        break;
      case 2:
        cfg_data->presentation_time_ms = 15;
        break;
      default:
        break;
    }
  }

  if (cfg_data->tx_pkts_per_sec == 0) {
    // determine pkts_per_wake based on the class type.
    if (cfg_data->sr_class_type == 0) { // class A
      // Class A is 8000 packets per sec, which is 8 packets per milisecond.
      cfg_data->tx_pkts_per_sec = 8000;
      //cfg_data->pkts_per_wake = cfg_data->wakeup_interval * 8;
    } else if (cfg_data->sr_class_type == 1) { // class B
      // Class A is 4000 packets per sec, which is 4 packets per milisecond.
      cfg_data->tx_pkts_per_sec = 4000;
      //cfg_data->pkts_per_wake = cfg_data->wakeup_interval * 4;
    } else if (cfg_data->sr_class_type == 2) { // class C
      // Class C defaults overrides wakeup period to get an even number
      // of packets per wakeup. 750 pkts/sec.
      cfg_data->tx_pkts_per_sec = 750;
      if (cfg_data->wakeup_interval <= 4) {
        //cfg_data->pkts_per_wake = 3;
        cfg_data->wakeup_interval = 4;
      } else if(cfg_data->wakeup_interval <= 8) {
        //cfg_data->pkts_per_wake = 6;
        cfg_data->wakeup_interval = 8;
      } else { // args->cfg_data.wakeup_interval > 8
        //cfg_data->pkts_per_wake = 9;
        cfg_data->wakeup_interval = 12;
      }
    }
  }

cleanup:
  if (linebuff != NULL) {
    free(linebuff);
  }
  if (fp != NULL) {
    fclose(fp);
  }
  return err;
}

int qeavb_create_stream(int eavb_fd, eavb_ioctl_stream_config_t* config, eavb_ioctl_hdr_t* hdr)
{
  eavb_ioctl_create_stream_t msg_create_stream;
  int err = 0;
  GST_DEBUG("create stream");
  if (!config || !hdr) {
    err = -1;
    goto error;
  }
  memcpy(&(msg_create_stream.config), config, sizeof(eavb_ioctl_stream_config_t));
  err = ioctl(eavb_fd, EAVB_IOCTL_CREATE_STREAM, &msg_create_stream, sizeof(msg_create_stream));
  if (0 != err) {
    GST_ERROR ("create eavb stream error %d, exit!", err);
    goto error;
  }
  memcpy(hdr, &(msg_create_stream.hdr), sizeof(eavb_ioctl_hdr_t));
error:
  return err;
}

int qeavb_create_stream_remote(int fd, char* cfgfilepath, eavb_ioctl_hdr_t* hdr)
{
  int ret = 0;
  eavb_ioctl_create_stream_with_path_t req;
  memset(&req, 0, sizeof(req));
  memcpy(req.path, cfgfilepath, strlen(cfgfilepath)+1);
  GST_DEBUG("qeavb_create_stream_remote, file path %s", req.path);
  ret = ioctl(fd, EAVB_IOCTL_CREATE_STREAM_WITH_PATH, &req, sizeof(req));
  memcpy(hdr, &(req.hdr), sizeof(eavb_ioctl_hdr_t));
  GST_DEBUG("hdr->streamCtx %ld", hdr->streamCtx);
  return ret;
}

int qeavb_get_stream_info(int eavb_fd, eavb_ioctl_hdr_t* hdr, eavb_ioctl_stream_info_t* info)
{
  int err = 0;
  eavb_ioctl_get_stream_info_t msg_stream_info;
  if(!hdr || !info) {
    err = -1;
    goto error;
  }
  memcpy(&(msg_stream_info.hdr), hdr, sizeof(eavb_ioctl_hdr_t));
  GST_DEBUG("msg_stream_info.hdr.streamCtx %ld", msg_stream_info.hdr.streamCtx);
  err = ioctl(eavb_fd, EAVB_IOCTL_GET_STREAM_INFO, &msg_stream_info, sizeof(msg_stream_info));
  if (0 != err) {
    GST_ERROR("get stream info error %d, exit!", err);
    goto error;
  }
  memcpy(info, &(msg_stream_info.info), sizeof(eavb_ioctl_stream_info_t));
error:
  return err;
}

int qeavb_destroy_stream(int eavb_fd, eavb_ioctl_hdr_t* hdr)
{
  int err = 0;
  eavb_ioctl_destroy_stream_t msg_destroy;

  if (eavb_fd == -1 || !hdr){
    err = -1;
    goto error;
  }
  memcpy(&(msg_destroy.hdr), hdr, sizeof(eavb_ioctl_hdr_t));
  err = ioctl(eavb_fd, EAVB_IOCTL_DESTROY_STREAM, &msg_destroy, sizeof(msg_destroy));
  if (0 != err) {
    GST_ERROR("destroy stream error %d, exit!", err);
    goto error;
  }
error:
  return err;
}

int qeavb_connect_stream(int eavb_fd, eavb_ioctl_hdr_t* hdr)
{
  int err = 0;
  eavb_ioctl_connect_stream_t msg_connect;
  if (eavb_fd == -1 || !hdr){
    err = -1;
    goto error;
  }
  memcpy(&(msg_connect.hdr), hdr, sizeof(eavb_ioctl_hdr_t));
  GST_DEBUG("msg_connect.hdr.streamCtx %ld", msg_connect.hdr.streamCtx);
  err = ioctl(eavb_fd, EAVB_IOCTL_CONNECT_STREAM, &msg_connect, sizeof(msg_connect));
  if (0 != err) {
    GST_ERROR("connect stream error %d, exit!", err);
    goto error;
  }
error:
  return err;
}

int qeavb_disconnect_stream(int eavb_fd, eavb_ioctl_hdr_t* hdr)
{
  int err = 0;
  eavb_ioctl_disconnect_stream_t msg_disconnect;
  if (eavb_fd == -1 || !hdr){
    err = -1;
    goto error;
  }
  memcpy(&(msg_disconnect.hdr), hdr, sizeof(eavb_ioctl_hdr_t));
  err = ioctl(eavb_fd, EAVB_IOCTL_DISCONNECT_STREAM, &msg_disconnect, sizeof(msg_disconnect));
  if (0 != err) {
    GST_ERROR("disconnect stream error %d, exit!", err);
    goto error;
  }
error:
  return err;
}

int qeavb_receive_data(int eavb_fd, eavb_ioctl_hdr_t* hdr, eavb_ioctl_buf_data_t* buff)
{
  int ret = 0;
  if (-1 == eavb_fd || NULL == hdr || NULL == buff)
    return -1;
  GST_DEBUG("receive data hdr->streamCtx %ld\n", hdr->streamCtx);
  eavb_ioctl_receive_t req = {
    .hdr = *hdr,
    .data = *buff,
  };
  ret = ioctl(eavb_fd, EAVB_IOCTL_RECEIVE, &req, sizeof(req));

  if (0 == ret) {
    buff->hdr = req.data.hdr;
    return req.received;
  }
  else {
    return ret;
  }
}

int qeavb_receive_done(int eavb_fd, eavb_ioctl_hdr_t* hdr, eavb_ioctl_buf_data_t* data)
{
  int err = 0;
  eavb_ioctl_recv_done_t msg_recv_done;
  if (eavb_fd == -1 || !hdr || !data){
    err = -1;
    goto error;
  }
  memcpy(&(msg_recv_done.hdr), hdr, sizeof(eavb_ioctl_hdr_t));
  memcpy(&(msg_recv_done.data), data, sizeof(eavb_ioctl_buf_data_t));
  err = ioctl(eavb_fd, EAVB_IOCTL_RECV_DONE, &msg_recv_done, sizeof(msg_recv_done));
  if (0 != err) {
    GST_ERROR("receive done error %d, exit!", err);
    goto error;
  }

error:
  return err;
}

