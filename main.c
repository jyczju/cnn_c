/*
 * Copyright (c) 2024, VeriSilicon Holdings Co., Ltd. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdbool.h>

#include "vad.h"
#include "algo_error_code.h"

#define RAW_FS     (8000)
#define OBJ_FS     (8000)
#define FRAME_STEP (120) // 0.015 * 8000
#define FRAME_LEN  (240) // 0.03 * 8000

uint64_t get_rows(char *file_dir)
{
    char line[1024];
    uint64_t i = 0;
    // FILE *stream = NULL;
    // fopen_s(&stream, file_dir, "r");
    FILE *stream = fopen(file_dir, "r");
    if (stream) {
        while (fgets(line, 1024, stream)) {
            i++;
        }

        fclose(stream);
    }
    return i;
}

double get_abs_max(char *file_dir)
{
    char line[1024];
    double max = 0;
    double cur_data = 0;
    // FILE *stream = NULL;
    // fopen_s(&stream, file_dir, "r");
    FILE *stream = fopen(file_dir, "r");
    if (stream) {
        while (fgets(line, 1024, stream)) {
            cur_data = strtod(line, NULL);
            if (cur_data < 0) {
                cur_data = -cur_data;
            }
            if (cur_data > max) {
                max = cur_data;
            }
        }

        fclose(stream);
    }
    return max;
}


void get_data(char *file_dir, double *data_buf)
{
    char line[1024];
    uint64_t i = 0;
    double abs_max = 32767;

    abs_max = get_abs_max(file_dir) + 1e-6;
    FILE *stream = fopen(file_dir, "r");

    if (stream) {
        while (fgets(line, 1024, stream)) {
            data_buf[i] = strtod(line, NULL)/abs_max;

            i++;
        }
        fclose(stream);
    }
}

void downsample(double *raw_data, uint64_t raw_size, uint16_t raw_fs, uint16_t obj_fs, double *out,
                uint64_t *out_size)
{
    uint16_t interval = raw_fs / obj_fs;
    uint64_t i        = 0;

    *out_size = 0;
    for (i = 0; i < raw_size; i += interval) {
        out[(*out_size)++] = raw_data[i];
    }
}

/**
 * voice_segment: 2n: start index, 2n+1:end index
 */
void cal_voice_segment(int8_t *pred_class, const uint64_t *pred_idx_in_data,
                       uint64_t pred_class_size, uint64_t raw_data_size, uint64_t *voice_segment,
                       uint64_t *voice_segment_size)
{
    uint64_t i = 0, voice_segment_cnt = 0;
    int8_t diff_vaule = 0;
    bool is_start     = true;

    *voice_segment_size = 0;

    for (i = 1; i < pred_class_size; i++) {
        diff_vaule = pred_class[i] - pred_class[i - 1];

        if (diff_vaule == 1) {
            voice_segment[voice_segment_cnt++] = pred_idx_in_data[i];
            is_start                           = false;
        }

        if (diff_vaule == -1) {
            if (is_start) {
                voice_segment[voice_segment_cnt++] = 0;
            }
            voice_segment[voice_segment_cnt++] = pred_idx_in_data[i];
            is_start                           = true;
        }
    }

    if (!is_start) {
        voice_segment[voice_segment_cnt++] = raw_data_size - 1;
    }

    *voice_segment_size = voice_segment_cnt;
}

int main()
{
    char file_dir[] = "./data_2.txt";
    FILE *file      = fopen("./predict/data_2.txt", "w");

    int ret            = ALGO_NORMAL;
    uint64_t data_size = 0, down_size = 0, pred_cnt = 0, i = 0, voice_seg_size = 0;
    double *total_data       = NULL;
    bool vad_out             = false;
    int8_t *total_pred       = NULL;
    uint64_t *total_pred_idx = NULL, *all_voice_segment = NULL;

    int8_t *tmp_pred       = NULL;
    static int8_t WINDOW_SIZE = 5;
    double DIV = 0;

    for (i = 1; i <= WINDOW_SIZE; i++)
    {
        DIV = DIV + i;
    }

    // Conv2dData vad_inp = {.channel = 1, .row = 1, .col = FRAME_LEN, .data = NULL};
    Conv2dData vad_inp = {.channel = 1, .row = 1, .col = FRAME_LEN, .data = NULL};

    data_size = get_rows(file_dir);
    printf("data_size = %llu\n", data_size);

    total_data = (double *)malloc(sizeof(double) * data_size);
    if (!total_data) {
        printf("malloc fail\n");
        return 0;
    }

    // get data and downsample
    get_data(file_dir, total_data);
    downsample(total_data, data_size, RAW_FS, OBJ_FS, total_data, &down_size);
    printf("down_size = %llu\n", down_size);

    total_pred = (int8_t *)malloc(sizeof(int8_t) * ((down_size - FRAME_LEN) / FRAME_STEP + 1)); //用于存储滤波后预测结果
    if (!total_pred) {
        printf("total_pred malloc fail\n");
        goto exit;
    }
    tmp_pred = (int8_t *)malloc(sizeof(int8_t) * ((down_size - FRAME_LEN) / FRAME_STEP + 1)); //用于存储预测结果
    if (!tmp_pred) {
        printf("tmp_pred malloc fail\n");
        goto exit;
    }
    total_pred_idx =
        (uint64_t *)malloc(sizeof(uint64_t) * ((down_size - FRAME_LEN) / FRAME_STEP + 1)); //用于存储预测结果的索引
    if (!total_pred_idx) {
        printf("total_pred_idx malloc fail\n");
        goto exit;
    }
    all_voice_segment =
        (uint64_t *)malloc(sizeof(uint64_t) * ((down_size - FRAME_LEN) / FRAME_STEP + 1));
    if (!all_voice_segment) {
        printf("malloc fail\n");
        goto exit;
    }

    // streaming audio data, frame by frame
    for (i = 0; i < down_size; i += FRAME_STEP) {
        if (i + FRAME_LEN - 1 > down_size) {
            break;
        }

        vad_inp.data = total_data + i;

        ret = vad(&vad_inp, &vad_out);
        if (ret != ALGO_NORMAL) {
            printf("ret = %d\n", ret);
            goto exit;
        }


        // 不滤波
        // total_pred[pred_cnt]       = (int8_t)vad_out;
        // total_pred_idx[pred_cnt] = i;


        // 加权平均投票法
        tmp_pred[pred_cnt]  = (int8_t)vad_out;

        if (pred_cnt < WINDOW_SIZE)
        {
            total_pred[pred_cnt]       = (int8_t)vad_out;
            total_pred_idx[pred_cnt] = i;
        }
        else
        {
            double sum = 0;
            for (int j = 0; j < WINDOW_SIZE; j++)
            {
                sum += tmp_pred[pred_cnt - j]*(WINDOW_SIZE * 1.0-j * 1.0) / DIV;
            }
            if (sum > 0.5)
            {
                total_pred[pred_cnt]       = 1;
                total_pred_idx[pred_cnt] = i;
            }
            else
            {
                total_pred[pred_cnt]       = 0;
                total_pred_idx[pred_cnt] = i;
            }
        }

        pred_cnt++;
    }

    // calaulate voice segments
    cal_voice_segment(total_pred, total_pred_idx, pred_cnt, down_size, all_voice_segment,
                      &voice_seg_size);

    // save the results to a file
    if (file) {
        for (i = 0; i < voice_seg_size; i += 2) {
            printf("%llu, %llu\n", all_voice_segment[i],all_voice_segment[i+1]);
            fprintf(file, "%llu, %llu\n", all_voice_segment[i], all_voice_segment[i + 1]);
        }
    }
    fclose(file);

exit:
    free(total_data);
    free(total_pred);
    free(total_pred_idx);
    free(all_voice_segment);

    return 0;
}