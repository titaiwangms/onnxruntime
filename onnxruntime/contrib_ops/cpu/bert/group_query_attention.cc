// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "contrib_ops/cpu/bert/group_query_attention.h"
#include "contrib_ops/cpu/bert/group_query_attention_helper.h"
#include "contrib_ops/cpu/bert/rotary_helper.h"
#include "contrib_ops/cpu/bert/attention_utils.h"
#include "contrib_ops/cpu/bert/rotary_embedding.h"
#include "contrib_ops/cpu/bert/rotary_embedding_helper.h"

#include "core/framework/tensorprotoutils.h"
#include "core/graph/onnx_protobuf.h"
#include "core/common/safeint.h"
#include "core/platform/threadpool.h"

#include <unsupported/Eigen/SpecialFunctions>
#include <vector>

using onnxruntime::concurrency::ThreadPool;

namespace onnxruntime {
namespace contrib {

// These ops are internal-only, so register outside of onnx
#define REGISTER_KERNEL_TYPED(T)                                        \
  ONNX_OPERATOR_TYPED_KERNEL_EX(                                        \
      GroupQueryAttention,                                              \
      kMSDomain,                                                        \
      1,                                                                \
      T,                                                                \
      kCpuExecutionProvider,                                            \
      KernelDefBuilder()                                                \
          .TypeConstraint("T", DataTypeImpl::GetTensorType<T>())        \
          .TypeConstraint("M", DataTypeImpl::GetTensorType<int32_t>()), \
      GroupQueryAttention<T>);

REGISTER_KERNEL_TYPED(float)
REGISTER_KERNEL_TYPED(MLFloat16)

template <typename T>
GroupQueryAttention<T>::GroupQueryAttention(const OpKernelInfo& info)
    : OpKernel(info), GQAAttentionBase(info, true) {}

template <typename T>
Status GroupQueryAttention<T>::Compute(OpKernelContext* context) const {
  const Tensor* query = context->Input<Tensor>(0);
  const Tensor* key = context->Input<Tensor>(1);
  const Tensor* value = context->Input<Tensor>(2);
  const Tensor* past_key = context->Input<Tensor>(3);
  const Tensor* past_value = context->Input<Tensor>(4);
  const Tensor* seqlens_k = context->Input<Tensor>(5);
  const Tensor* total_seqlen_tensor = context->Input<Tensor>(6);
  const Tensor* cos_cache = context->Input<Tensor>(7);
  const Tensor* sin_cache = context->Input<Tensor>(8);
  const Tensor* position_ids = context->Input<Tensor>(9);
  const Tensor* attention_bias = context->Input<Tensor>(10);
  const Tensor* head_sink = context->Input<Tensor>(11);

  GroupQueryAttentionParameters parameters = {};
  ORT_RETURN_IF_ERROR(group_query_attention_helper::CheckInputs(query,
                                                                key,
                                                                value,
                                                                past_key,
                                                                past_value,
                                                                cos_cache,
                                                                sin_cache,
                                                                &parameters,
                                                                num_heads_,
                                                                kv_num_heads_,
                                                                seqlens_k,
                                                                total_seqlen_tensor,
                                                                scale_,
                                                                softcap_));

  ORT_RETURN_IF_ERROR(group_query_attention_helper::CheckCustomAttentionInputs(position_ids,
                                                                               attention_bias,
                                                                               head_sink,
                                                                               parameters));

  const int batch_size = parameters.batch_size;
  const int sequence_length = parameters.sequence_length;
  const int present_kv_seqlen = parameters.seqlen_present_kv_cache;
  int head_size = parameters.head_size;
  int q_hidden_size = parameters.hidden_size;
  const bool packed_qkv = parameters.is_packed_qkv;

  std::vector<int64_t> output_shape(3);
  output_shape[0] = static_cast<int64_t>(batch_size);
  output_shape[1] = static_cast<int64_t>(sequence_length);
  output_shape[2] = static_cast<int64_t>(q_hidden_size);
  Tensor* output = context->Output(0, output_shape);

  std::vector<int64_t> present_k_shape({static_cast<int64_t>(batch_size), static_cast<int64_t>(kv_num_heads_), static_cast<int64_t>(present_kv_seqlen), static_cast<int64_t>(head_size)});
  std::vector<int64_t> present_v_shape({static_cast<int64_t>(batch_size), static_cast<int64_t>(kv_num_heads_), static_cast<int64_t>(present_kv_seqlen), static_cast<int64_t>(head_size)});
  Tensor* present_k = context->Output(1, present_k_shape);
  Tensor* present_v = context->Output(2, present_v_shape);

  AllocatorPtr allocator;
  ORT_RETURN_IF_ERROR(context->GetTempSpaceAllocator(&allocator));

  auto element_type = DataTypeImpl::GetType<T>();
  OrtValue Q;
  OrtValue K;
  OrtValue V;
  if (packed_qkv) {
    ORT_RETURN_IF_ERROR(MaybeTransposeToBNSH<T>(
        allocator, batch_size, num_heads_ + 2 * kv_num_heads_, sequence_length, head_size, query, Q));
  } else {
    ORT_RETURN_IF_ERROR(MaybeTransposeToBNSH<T>(
        allocator, batch_size, num_heads_, sequence_length, head_size, query, Q));
    ORT_RETURN_IF_ERROR(MaybeTransposeToBNSH<T>(
        allocator, batch_size, kv_num_heads_, sequence_length, head_size, key, K));
    ORT_RETURN_IF_ERROR(MaybeTransposeToBNSH<T>(
        allocator, batch_size, kv_num_heads_, sequence_length, head_size, value, V));
  }

  OrtValue RotaryQKV;
  OrtValue RotaryQ;
  OrtValue RotaryK;
  T* q_rotary = Q.GetMutable<Tensor>()->MutableData<T>();
  T* k_rotary = packed_qkv ? nullptr : K.GetMutable<Tensor>()->MutableData<T>();
  if (do_rotary_) {
    // Initialize rotary parameters
    rotary_embedding_helper::RotaryParameters rotary_params = {};
    rotary_params.batch_size = batch_size;
    rotary_params.sequence_length = sequence_length;
    rotary_params.hidden_size = q_hidden_size;
    rotary_params.head_size = head_size;
    rotary_params.rotary_embedding_dim = parameters.rotary_dim;
    rotary_params.num_heads = num_heads_;
    rotary_params.max_sequence_length = sequence_length;  // unused
    rotary_params.seq_stride = head_size;
    rotary_params.head_stride = sequence_length * rotary_params.seq_stride;
    rotary_params.batch_stride = (packed_qkv ? (num_heads_ + 2 * kv_num_heads_) : num_heads_) * rotary_params.head_stride;
    rotary_params.position_ids_format = !parameters.is_first_prompt ? 1 : 0;
    rotary_params.transposed = true;
    auto* tp = context->GetOperatorThreadPool();
    // Generate position ids
    const int pos_ids_size = parameters.is_first_prompt ? 1 : batch_size * sequence_length;
    std::vector<int64_t> default_pos_ids(pos_ids_size);
    const int64_t* pos_ids_data = default_pos_ids.data();

    if (position_ids != nullptr) {
      pos_ids_data = position_ids->Data<int64_t>();
    } else if (parameters.is_first_prompt) {
      default_pos_ids[0] = static_cast<int64_t>(0);
    } else {
      // Note: As of now, continuous decoding supports only batch size 1 and token generation supports only sequence length 1.
      for (int b = 0; b < batch_size; b++) {
        const int total_seqlen = seqlens_k->Data<int32_t>()[b] + 1;
        const int past_seqlen = total_seqlen - sequence_length;
        for (int s = 0; s < sequence_length; s++) {
          if (past_seqlen + s < total_seqlen) {
            default_pos_ids[b * sequence_length + s] = static_cast<int64_t>(past_seqlen) + s;
          } else {
            default_pos_ids[b * sequence_length + s] = static_cast<int64_t>(1);
          }
        }
      }
    }

    // Initialize separate buffers for rotary embeddings
    const T* q_input;
    const T* k_input;
    if (packed_qkv) {
      Tensor::InitOrtValue(element_type, TensorShape({batch_size, num_heads_ + 2 * kv_num_heads_, sequence_length, head_size}), allocator, RotaryQKV);
      q_input = Q.Get<Tensor>().Data<T>();
      k_input = q_input + num_heads_ * sequence_length * head_size;
      q_rotary = RotaryQKV.GetMutable<Tensor>()->MutableData<T>();
      k_rotary = q_rotary + num_heads_ * sequence_length * head_size;
    } else {
      Tensor::InitOrtValue(element_type, TensorShape({batch_size, num_heads_, sequence_length, head_size}), allocator, RotaryQ);
      Tensor::InitOrtValue(element_type, TensorShape({batch_size, kv_num_heads_, sequence_length, head_size}), allocator, RotaryK);
      q_input = Q.Get<Tensor>().Data<T>();
      k_input = K.Get<Tensor>().Data<T>();
      q_rotary = RotaryQ.GetMutable<Tensor>()->MutableData<T>();
      k_rotary = RotaryK.GetMutable<Tensor>()->MutableData<T>();
    }
    // Run rotary embedding for Q and K
    ORT_RETURN_IF_ERROR(RunRotaryEmbedding<T>(tp, rotary_params, q_input,
                                              pos_ids_data, cos_cache->Data<T>(),
                                              sin_cache->Data<T>(), q_rotary, rotary_interleaved_));

    rotary_params.num_heads = kv_num_heads_;
    rotary_params.hidden_size = parameters.kv_hidden_size;
    if (!packed_qkv) {
      rotary_params.batch_stride = kv_num_heads_ * rotary_params.head_stride;
    }
    ORT_RETURN_IF_ERROR(RunRotaryEmbedding<T>(tp, rotary_params, k_input,
                                              pos_ids_data, cos_cache->Data<T>(),
                                              sin_cache->Data<T>(), k_rotary, rotary_interleaved_));
    // Pack V into rotary QKV buffer
    if (packed_qkv) {
      const T* v_input = k_input + kv_num_heads_ * sequence_length * head_size;
      T* v_rotary = k_rotary + kv_num_heads_ * sequence_length * head_size;
      ORT_RETURN_IF_ERROR(rotary_helper::PackVIntoRotaryQKV<T>(tp,
                                                               parameters.batch_size,
                                                               parameters.sequence_length,
                                                               parameters.num_heads,
                                                               parameters.kv_num_heads,
                                                               parameters.head_size,
                                                               v_input,
                                                               v_rotary));
    }
  }

  ORT_RETURN_IF_ERROR(context->GetTempSpaceAllocator(&allocator));

  const T* head_sink_data = (head_sink != nullptr) ? head_sink->Data<T>() : nullptr;

  // Compute the attention score and apply the score to V
  return ApplyAttention(q_rotary, packed_qkv ? nullptr : k_rotary, packed_qkv ? nullptr : V.Get<Tensor>().Data<T>(),
                        head_sink_data, attention_bias, past_key, past_value, output, present_k, present_v,
                        seqlens_k, parameters, allocator, context);
}
}  // namespace contrib
}  // namespace onnxruntime
