/*
 * Copyright (c) 2019 horizon.ai.
 *
 * Unpublished copyright. All rights reserved. This material contains
 * proprietary information that should be used or copied only within
 * horizon.ai, except with written permission of horizon.ai.
 * @author: xuecheng.cui
 * @file: hrsc_sdk_demo.cc
 * @date: 12/30/19
 * @brief: 
 */

#include "include/hrsc_sdk.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdlib>

HrscEffectConfig effect_cfg{};
char *output_path{nullptr};
int32_t switch_flag = 0;

static size_t wkp_count = 0;

FILE *voip_fp{nullptr};

template<class T>
static std::string to_string(const T &t) {
  std::ostringstream ostream;
  ostream << t;
  return ostream.str();
}

void DemoEventCallback(const void *cookie, HrscEventType event) {
  if (event == kHrscEventWkpNormal) {
    std::cout << "wkp success" << std::endl;
  }
}

void DemoCmdCallback(const void *cookie, const char *cmd) {
  std::cout << "cmd success: " << cmd << std::endl;
}

void DemoVoipDataCallback(const void *cookie, const HrscCallbackData *data) {
  if (switch_flag) {
    static FILE *voip = fopen("./voip.pcm", "wb");
    fwrite(data->audio_buffer.audio_data, 1, data->audio_buffer.size, voip);
    fflush(voip);
  }
}

void DemoDoaCallback(const void *cookie, int doa) {
  std::cout << "doa is " << doa << std::endl;
}

void DemoAsrCallback(const void *cookie, const char *asr) {
  std::cout << "asr is: " << asr << std::endl;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "./hrsctest -i input_audio -o output -cfg config \n");
    return -1;
  }

  char *input_audio{nullptr};
  char *cfg_path{nullptr};

  char *cus{nullptr};

  while (*argv) {
    if (strcmp(*argv, "-i") == 0) {
      ++argv;
      input_audio = *argv;
    } else if (strcmp(*argv, "-o") == 0) {
      ++argv;
      output_path = *argv;
    } else if (strcmp(*argv, "-cfg") == 0) {
      ++argv;
      cfg_path = *argv;
    } else if (strcmp(*argv, "-switch") == 0) {
      ++argv;
      if (*argv != nullptr) {
        switch_flag = strtoll(*argv, nullptr, 10);
      }
    } else if (strcmp(*argv, "-cus") == 0) {
      ++argv;
      cus = *argv;
    }
    if (*argv) argv++;
  }

  if (cfg_path == nullptr || input_audio == nullptr || output_path == nullptr) {
    fprintf(stderr, "./hrsctest -i input_audio -o output -cfg config \n");
    return -1;
  }

  HrscAudioConfig input_cfg;
  input_cfg.audio_channels = 6;
  input_cfg.sample_rate = 16000;
  input_cfg.audio_format = kHrscAudioFormatPcm16Bit;

  HrscAudioConfig output_cfg;
  output_cfg.audio_channels = 1;
  output_cfg.sample_rate = 16000;
  output_cfg.audio_format = kHrscAudioFormatPcm16Bit;

  effect_cfg.input_cfg = input_cfg;
  effect_cfg.output_cfg = output_cfg;
  effect_cfg.priv = &effect_cfg;
  effect_cfg.asr_timeout = 5000;
  effect_cfg.cfg_file_path = cfg_path;
  char tmp[128];
  memset(tmp, 0, 128);
  if (nullptr != cus) {
    FILE *fp = fopen(cus, "r");
    if (!fp) {
      effect_cfg.custom_wakeup_word = nullptr;
    } else {
      fseek(fp, 0, SEEK_END);
      auto len = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      fread(tmp, 1, len, fp);
      fclose(fp);
      effect_cfg.custom_wakeup_word = tmp;
    }
    printf("cus word is %s \n", tmp);
  } else {
    effect_cfg.custom_wakeup_word = nullptr;
  }

  effect_cfg.vad_timeout = 5000;
  effect_cfg.ref_ch_index = 10;
  effect_cfg.target_score = 0;
  effect_cfg.support_command_word = 0;
  effect_cfg.wakeup_prefix = 200;
  effect_cfg.wakeup_suffix = 200;
  effect_cfg.is_use_linear_mic_flag = 0;
  effect_cfg.asr_output_mode = 0;
  effect_cfg.asr_output_channel = 0;

  // 当切换到voip时，需要使用此设置开启，默认为识别模式
  effect_cfg.HrscVoipDataCallback = DemoVoipDataCallback;
  effect_cfg.HrscEventCallback = DemoEventCallback;
  effect_cfg.HrscCmdCallback = DemoCmdCallback;
  effect_cfg.HrscDoaCallbadk = DemoDoaCallback;
  effect_cfg.HrscAsrCallback = DemoAsrCallback;

  fprintf(stdout, "hrsc init success start ! \n");
  void *handle = HrscInit(&effect_cfg);
  if (handle == nullptr) {
    fprintf(stderr, "hrsc init error ! \n");
    return -1;
  }
  fprintf(stdout, "hrsc init success ! \n");

  if (HRSC_CODE_SUCCESS != HrscStart(handle)) {
    fprintf(stderr, "hrsc start error ! \n");
    return -1;
  }
  fprintf(stdout, "hrsc start success ! \n");

  FILE *input_fp = fopen(input_audio, "rb");
  if (!input_fp) {
    fprintf(stderr, "open input audio failed ! \n");
    return -1;
  }

  int v = 1;

  // HrscParamData data;
  // data.value = &v;
  // data.param_type = kHrscParasTypeVoipDataSwitch;
  // HrscSetParam(handle, &data);

  size_t input_len = effect_cfg.input_cfg.audio_channels * 512;
  char *input_buff = new char[input_len];
  HrscAudioBuffer hrsc_buffer;
  fprintf(stdout, "before send audio ! \n");
  while (fread(input_buff, sizeof(char), input_len, input_fp) >= input_len) {
    hrsc_buffer.audio_data = input_buff;
    hrsc_buffer.size = input_len;
    HrscProcess(handle, &hrsc_buffer);
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  fprintf(stdout, "over send audio ! \n");
  std::this_thread::sleep_for(std::chrono::seconds(2));
  fprintf(stdout, "befor stop sdk ! \n");
  HrscStop(handle);
  fprintf(stdout, "stop sdk success ! \n");
  HrscRelease(&handle);
  fprintf(stdout, "destory sdk success ! \n");
  delete[]input_buff;
  return 0;
}

