/*
 *  Copyright © 2017-2020 Wellington Wallace
 *
 *  This file is part of PulseEffects.
 *
 *  PulseEffects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  PulseEffects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with PulseEffects.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef READ_KERNEL_HPP
#define READ_KERNEL_HPP

#include <samplerate.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sndfile.hh>
#include "gstpeconvolver.hpp"
#include "util.hpp"

namespace rk {

std::string log_tag = "convolver: ";

void autogain(std::vector<float>& left, std::vector<float>& right) {
  float power = 0.0F, peak = 0.0F;

  for (uint n = 0U; n < left.size(); n++) {
    peak = (left[n] > peak) ? left[n] : peak;
    peak = (right[n] > peak) ? right[n] : peak;
  }

  // normalize
  for (uint n = 0U; n < left.size(); n++) {
    left[n] /= peak;
    right[n] /= peak;
  }

  // find average power
  for (uint n = 0U; n < left.size(); n++) {
    power += left[n] * left[n] + right[n] * right[n];
  }

  power *= 0.5F;

  float autogain = std::min(1.0F, 1.0F / sqrtf(power));

  util::debug(log_tag + "autogain factor: " + std::to_string(autogain));

  for (uint n = 0U; n < left.size(); n++) {
    left[n] *= autogain;
    right[n] *= autogain;
  }
}

/* Mid-Side based Stereo width effect
   taken from https://github.com/tomszilagyi/ir.lv2/blob/automatable/ir.cc
*/
void ms_stereo(float width, std::vector<float>& left, std::vector<float>& right) {
  float w = width / 100.0F;
  float x = (1.0F - w) / (1.0F + w); /* M-S coeff.; L_out = L + x*R; R_out = x*L + R */

  for (uint i = 0U; i < left.size(); i++) {
    float L = left[i], R = right[i];

    left[i] = L + x * R;
    right[i] = R + x * L;
  }
}

bool read_file(GstPeconvolver* peconvolver) {
  if (peconvolver->kernel_path == nullptr) {
    util::debug(log_tag + "irs file path is null");

    return false;
  }

  SndfileHandle file = SndfileHandle(peconvolver->kernel_path);

  if (file.channels() == 0 || file.frames() == 0) {
    util::debug(log_tag + "irs file does not exists or it is empty: " + peconvolver->kernel_path);

    return false;
  }

  util::debug(log_tag + "irs file: " + peconvolver->kernel_path);
  util::debug(log_tag + "irs rate: " + std::to_string(file.samplerate()) + " Hz");
  util::debug(log_tag + "irs channels: " + std::to_string(file.channels()));
  util::debug(log_tag + "irs frames: " + std::to_string(file.frames()));

  // for now only stereo irs files are supported

  if (file.channels() == 2) {
    bool resample = false;
    float resample_ratio = 1.0F;
    uint total_frames_in, total_frames_out, frames_in, frames_out;

    frames_in = file.frames();
    total_frames_in = file.channels() * frames_in;

    std::vector<float> buffer(total_frames_in);

    file.readf(buffer.data(), frames_in);

    if (file.samplerate() != peconvolver->rate) {
      resample = true;

      resample_ratio = static_cast<float>(peconvolver->rate) / file.samplerate();

      frames_out = ceil(file.frames() * resample_ratio);
      total_frames_out = file.channels() * frames_out;
    } else {
      frames_out = frames_in;
      total_frames_out = file.channels() * frames_out;
    }

    // allocate arrays

    std::vector<float> kernel(total_frames_out);

    peconvolver->kernel_L.resize(frames_out);
    peconvolver->kernel_R.resize(frames_out);
    peconvolver->kernel_n_frames = frames_out;

    // resample if necessary

    if (resample) {
      util::debug(log_tag + "resampling irs to " + std::to_string(peconvolver->rate) + " Hz");

      SRC_STATE* src_state = src_new(SRC_SINC_BEST_QUALITY, file.channels(), nullptr);

      SRC_DATA src_data;

      /* code from
       * https://github.com/x42/convoLV2/blob/master/convolution.cc
       */

      // The number of frames of data pointed to by data_in
      src_data.input_frames = frames_in;

      // A pointer to the input data samples
      src_data.data_in = buffer.data();

      // Maximum number of frames pointer to by data_out
      src_data.output_frames = frames_out;

      // A pointer to the output data samples
      src_data.data_out = kernel.data();

      // Equal to output_sample_rate / input_sample_rate
      src_data.src_ratio = resample_ratio;

      // Equal to 0 if more input data is available and 1 otherwise
      src_data.end_of_input = 1;

      src_process(src_state, &src_data);

      src_delete(src_state);

      util::debug(log_tag + "irs frames after resampling " + std::to_string(frames_out));
    } else {
      util::debug(log_tag + "irs file does not need resampling");

      std::memcpy(kernel.data(), buffer.data(), total_frames_in * sizeof(float));
    }

    // deinterleave
    for (uint n = 0U; n < frames_out; n++) {
      peconvolver->kernel_L[n] = kernel[2U * n];
      peconvolver->kernel_R[n] = kernel[2U * n + 1U];
    }

    autogain(peconvolver->kernel_L, peconvolver->kernel_R);

    ms_stereo(peconvolver->ir_width, peconvolver->kernel_L, peconvolver->kernel_R);

    return true;
  } else {
    util::debug(log_tag + "only stereo impulse responses are supported." + "The impulse file was not loaded!");

    return false;
  }
}

}  // namespace rk

#endif
