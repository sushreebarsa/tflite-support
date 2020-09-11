// Copyright 2020 The TensorFlow Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/**
 * Sentencepiece tflite detokenizer implementation.
 */
#include <algorithm>
#include <iterator>

#include "flatbuffers/flexbuffers.h"  // from @flatbuffers
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/context.h"
#include "tensorflow/lite/kernels/internal/tensor.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/string_util.h"
#include "tensorflow_lite_support/custom_ops/kernel/sentencepiece/optimized_decoder.h"
#include "tensorflow_lite_support/custom_ops/kernel/sentencepiece/sentencepiece_detokenizer.h"

namespace tflite {
namespace support {
namespace ops {
namespace custom {
namespace sentencepiece_detokenizer {

constexpr int kOutputValuesInd = 0;
// Initializes text encoder object from serialized parameters.
void* Initialize(TfLiteContext* /*context*/, const char* /*buffer*/,
                 size_t /*length*/) {
  return nullptr;
}
void Free(TfLiteContext* /*context*/, void* /*buffer*/) {}

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  // TODO(mgubin): Add checks for input and output tensors.
  TfLiteTensor& output_values =
      context->tensors[node->outputs->data[kOutputValuesInd]];
  SetTensorToDynamic(&output_values);
  // TODO(mgubin): Check input types.

  return kTfLiteOk;
}

TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  const TfLiteTensor& model_tensor =
      context->tensors[node->inputs->data[tensorflow::ops::kSPModelIndex]];
  const auto model_buffer_data = model_tensor.data.data;
  const TfLiteTensor& input_encoded =
      context->tensors[node->inputs->data[tensorflow::ops::kInputIndex]];
  const int32_t* input_encoded_data = input_encoded.data.i32;
  const TfLiteTensor& input_splits =
      context->tensors[node->inputs->data[tensorflow::ops::kInputSplits]];
  const int num_of_sentences = NumElements(input_splits.dims) - 1;
  const int32_t* input_splits_data = input_splits.data.i32;

  DynamicBuffer buf;

  std::vector<int> codes_for_split;
  int input_offset = 0;
  for (int i = 0; i < num_of_sentences; i++) {
    // Create a vector of int32 from input according to spans.
    const int split_size = input_splits_data[i + 1] - input_splits_data[i];
    codes_for_split.clear();
    std::copy(input_encoded_data + input_offset,
              input_encoded_data + input_offset + split_size,
              std::back_inserter(codes_for_split));
    const auto res =
        tflite::support::ops::DecodeString(codes_for_split, model_buffer_data);
    TF_LITE_ENSURE_MSG(context, res.type == DecoderResultType::SUCCESS,
                       "Sentencepiece decoding failed");
    buf.AddString(res.decoded.data(), res.decoded.length());
    input_offset += split_size;
  }
  TfLiteTensor& output_values =
      context->tensors[node->outputs->data[kOutputValuesInd]];
  buf.WriteToTensor(&output_values, nullptr);
  return kTfLiteOk;
}
}  // namespace sentencepiece_detokenizer

TfLiteRegistration* Register_SENTENCEPIECE_DETOKENIZER() {
  static TfLiteRegistration r = {
      sentencepiece_detokenizer::Initialize, sentencepiece_detokenizer::Free,
      sentencepiece_detokenizer::Prepare, sentencepiece_detokenizer::Eval};
  return &r;
}
}  // namespace custom
}  // namespace ops
}  // namespace support
}  // namespace tflite