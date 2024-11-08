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

#include "vad.h"
#include "model_parameters.h"

int vad(Conv2dData *inp_data, bool *is_voice)
{
    int ret               = ALGO_NORMAL;
    uint16_t conv_out_len = 0;
    double linear_out[2]  = {0};

    // Conv2dFilter filter = {
    //     .channel = 1, .col = 2, .row = 1, .filter_num = 2, .data = model_0_weight};
    Conv2dFilter filter = {
        .channel = 1, .col = 8, .row = 1, .filter_num = 8, .data = model_0_weight};
    BatchNorm2d bn            = {.beta  = model_1_bias,
                                 .gamma = model_1_weight,
                                 .mean  = model_1_running_mean,
                                 .var   = model_1_running_var,
                                 .size  = 8};
    Conv2dConfig conv_config  = {.pad = 0, .stride = 8, .bn = &bn, .filter = &filter};
    LinearParam linear_config = {
        .inp_size = 240, .fea_size = 2, .weight = output_weight, .bias = output_bias};

    Conv2dData conv_out;

    *is_voice = false;

    memset(&conv_out, 0, sizeof(Conv2dData));
    // conv_out_len  = cal_conv_out_len(inp_data->col, 0, 2, 2);
    // printf("inp_data->col: %d\n", inp_data->col);
    conv_out_len  = cal_conv_out_len(inp_data->col, 0, 8, 8);
    // printf("conv_out_len: %d\n", conv_out_len);
    conv_out.data = (double *)malloc(sizeof(double) * conv_out_len * 8);
    if (!conv_out.data) {
        return ALGO_MALLOC_FAIL;
    }

    ret = conv2d_bn_no_bias(inp_data, &conv_config, &conv_out);
    if (ret != ALGO_NORMAL) {
        goto func_exit;
    }

    ret = leaky_relu(0.01, conv_out.data, conv_out.channel * conv_out.col * conv_out.row,
                     conv_out.data);
    if (ret != ALGO_NORMAL) {
        goto func_exit;
    }

    ret = linear_layer(conv_out.data, &linear_config, linear_out);
    if (ret != ALGO_NORMAL) {
        goto func_exit;
    }

    if (linear_out[1] > linear_out[0]) {
        *is_voice = true;
    }

func_exit:
    if (conv_out.data) {
        free(conv_out.data);
    }

    return ret;
}
