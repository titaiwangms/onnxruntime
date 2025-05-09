/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Licensed under the MIT License.

Module Name:

    sqnbitgemm_kernel_avx512.cpp.h

Abstract:

    This module implements the float/quantized n-bit integer matrix
    multiplication kernels for x64 avx512vnni.

--*/

#include <algorithm>
#include <cassert>
#include <utility>

#include "qnbitgemm.h"
#include "sqnbitgemm_kernel_avx_common.h"
#include "sqnbitgemm_kernel_avx_common_fp32.h"
#include "sqnbitgemm_kernel_avx_common_int8.h"
#include "sqnbitgemm_kernel_avx512_int8_blklen16.h"
#include "sqnbitgemm_kernel_avx512_int8_blklen32.h"
#include "sqnbitgemm_kernel_avx512_int8_blklen64.h"
#include "sqnbitgemm_kernel_avx512_int8_blklen128.h"

MLAS_FORCEINLINE void
SQ4BitGemmM1Kernel_CompFp32(
    size_t BlkLen,
    const float* A,
    const std::byte* QuantBData,
    const float* QuantBScale,
    const std::byte* QuantBZeroPoint,
    float* C,
    size_t CountN,
    size_t CountK,
    size_t BlockStrideQuantB,
    const float* Bias
)
{
    if (BlkLen == 16) {
        if (QuantBZeroPoint != nullptr) {
            MlasQ4GemmKernelBlkLen16Avx512f<true>(
                A,
                QuantBData,
                QuantBScale,
                QuantBZeroPoint,
                C,
                1,
                CountN,
                CountK,
                BlockStrideQuantB,
                Bias,
                0,
                0
            );
        } else {
            MlasQ4GemmKernelBlkLen16Avx512f<false>(
                A,
                QuantBData,
                QuantBScale,
                QuantBZeroPoint,
                C,
                1,
                CountN,
                CountK,
                BlockStrideQuantB,
                Bias,
                0,
                0
            );
        }
    } else if (BlkLen == 32) {
        if (QuantBZeroPoint != nullptr) {
            MlasQ4GemmKernelBlkLen32PlusAvx512f<true, false>(
                BlkLen,
                A,
                QuantBData,
                QuantBScale,
                QuantBZeroPoint,
                C,
                1,
                CountN,
                CountK,
                BlockStrideQuantB,
                Bias,
                0,
                0
            );
        } else {
            MlasQ4GemmKernelBlkLen32PlusAvx512f<false, false>(
                BlkLen,
                A,
                QuantBData,
                QuantBScale,
                QuantBZeroPoint,
                C,
                1,
                CountN,
                CountK,
                BlockStrideQuantB,
                Bias,
                0,
                0
            );
        }
    } else /*if (BlkLen >= 64)*/ {
        if (QuantBZeroPoint != nullptr) {
            MlasQ4GemmKernelBlkLen32PlusAvx512f<true, true>(
                BlkLen,
                A,
                QuantBData,
                QuantBScale,
                QuantBZeroPoint,
                C,
                1,
                CountN,
                CountK,
                BlockStrideQuantB,
                Bias,
                0,
                0
            );
        } else {
            MlasQ4GemmKernelBlkLen32PlusAvx512f<false, true>(
                BlkLen,
                A,
                QuantBData,
                QuantBScale,
                QuantBZeroPoint,
                C,
                1,
                CountN,
                CountK,
                BlockStrideQuantB,
                Bias,
                0,
                0
            );
        }
    }
}

MLAS_FORCEINLINE
void
SQ4BitGemmM1Kernel_CompInt8_avx512vnni(
    size_t BlkLen,
    const std::byte* QuantA,
    const float* QuantAScale,
    const std::byte* QuantBData,
    const float* QuantBScale,
    const std::byte* QuantBZeroPoint,
    float* C,
    size_t CountN,
    size_t CountK,
    size_t BlockStrideQuantB,
    const float* Bias
)
{
    if (QuantBZeroPoint != nullptr) {
        assert(false);
    } else {
        constexpr bool HasZeroPoint = false;
        if (BlkLen == 16) {
            SQ4BitGemmM1Kernel_BlkLen16_CompInt8_Impl<HasZeroPoint>(
                QuantA,
                QuantBData,
                QuantBScale,
                QuantBZeroPoint,
                C,
                CountN,
                CountK,
                BlockStrideQuantB,
                Bias
            );
        } else if (BlkLen == 32) {
            SQ4BitGemmM1Kernel_BlkLen32_CompInt8_Impl<HasZeroPoint, accumulate_mul_sum_avx512vnni<HasZeroPoint>>(
                QuantA,
                QuantAScale,
                QuantBData,
                QuantBScale,
                QuantBZeroPoint,
                C,
                CountN,
                BlockStrideQuantB,
                Bias
            );
        } else {
            SQ4BitGemmM1Kernel_BlkLen64Plus_CompInt8_Impl<HasZeroPoint, dot_quad_avx512vnni>(
                BlkLen,
                QuantA,
                QuantBData,
                QuantBScale,
                QuantBZeroPoint,
                C,
                CountN,
                CountK,
                BlockStrideQuantB,
                Bias
            );
        }
    }
}

MLAS_FORCEINLINE
size_t
SQ4BitGemmKernel_BlkSum_CompInt8_avx512vnni(
    const size_t BlkLen,
    const std::byte* QuantA,
    const float* QuantAScale,
    const std::byte* QuantBData,
    const float* QuantBScale,
    const std::byte* /*QuantBZeroPoint*/,
    float* C,
    size_t CountM,
    size_t CountN,
    size_t /*CountK*/,
    size_t BlockCountK,
    const float* Bias,
    size_t ldc,
    const float* ABlockSum,
    const float* QuantBBlkSum
)
{
    if (BlkLen == 16) {
        MlasQ4Int8GemmKernelBlkLen16Avx512<true>(
            QuantA,
            QuantAScale,
            QuantBData,
            QuantBScale,
            C,
            CountM,
            CountN,
            BlockCountK,
            Bias,
            ldc
        );
    } else if (BlkLen == 32) {
        MlasQ4Int8GemmKernelBlkLen32Avx512<true>(
            QuantA,
            QuantAScale,
            QuantBData,
            QuantBScale,
            C,
            CountM,
            CountN,
            BlockCountK,
            Bias,
            ldc
        );
    } else if (BlkLen == 64) {
        MlasQ4Int8GemmKernelBlkLen64Avx512<true>(
            BlkLen,
            QuantA,
            QuantAScale,
            QuantBData,
            QuantBScale,
            C,
            CountM,
            CountN,
            BlockCountK,
            Bias,
            ldc
        );
    } else {
        MlasQ4Int8GemmKernelBlkLen128Avx512<true>(
            BlkLen,
            QuantA,
            QuantAScale,
            QuantBData,
            QuantBScale,
            C,
            CountM,
            CountN,
            BlockCountK,
            Bias,
            ldc
        );
    }

    float* c_blk = C;
    const float* b_blk_sum = QuantBBlkSum;

    size_t RowsRemaining = CountM;
    const float* a_blksum_row = ABlockSum;
    while (RowsRemaining > 0) {
        auto RowsHandled = GetMlasPlatform().GemmFloatKernel(
            a_blksum_row, b_blk_sum, c_blk, BlockCountK, RowsRemaining, CountN, BlockCountK, ldc, 1.f, false
        );

        c_blk += ldc * RowsHandled;
        a_blksum_row += BlockCountK * RowsHandled;
        RowsRemaining -= RowsHandled;
    }
    return CountM;
}

MLAS_FORCEINLINE
size_t
SQ8BitGemmKernel_BlkSum_CompInt8_avx512vnni(
    const size_t BlkLen,
    const std::byte* QuantA,
    const float* QuantAScale,
    const std::byte* QuantBData,
    const float* QuantBScale,
    const std::byte* /*QuantBZeroPoint*/,
    float* C,
    size_t CountM,
    size_t CountN,
    size_t /*CountK*/,
    size_t BlockCountK,
    const float* Bias,
    size_t ldc,
    const float* ABlockSum,
    const float* QuantBBlkSum
)
{
    if (BlkLen == 16) {
        MlasQ8Int8GemmKernelBlkLen16Avx512<true>(
            QuantA,
            QuantAScale,
            QuantBData,
            QuantBScale,
            C,
            CountM,
            CountN,
            BlockCountK,
            Bias,
            ldc
        );
    } else if (BlkLen == 32) {
        MlasQ8Int8GemmKernelBlkLen32Avx512<true>(
            QuantA,
            QuantAScale,
            QuantBData,
            QuantBScale,
            C,
            CountM,
            CountN,
            BlockCountK,
            Bias,
            ldc
        );
    } else if (BlkLen == 64) {
        MlasQ8Int8GemmKernelBlkLen64Avx512<true>(
            BlkLen,
            QuantA,
            QuantAScale,
            QuantBData,
            QuantBScale,
            C,
            CountM,
            CountN,
            BlockCountK,
            Bias,
            ldc
        );
    } else {
        MlasQ8Int8GemmKernelBlkLen128Avx512<true>(
            BlkLen,
            QuantA,
            QuantAScale,
            QuantBData,
            QuantBScale,
            C,
            CountM,
            CountN,
            BlockCountK,
            Bias,
            ldc
        );
    }

    float* c_blk = C;
    const float* b_blk_sum = QuantBBlkSum;

    size_t RowsRemaining = CountM;
    const float* a_blksum_row = ABlockSum;
    while (RowsRemaining > 0) {
        auto RowsHandled = GetMlasPlatform().GemmFloatKernel(
            a_blksum_row, b_blk_sum, c_blk, BlockCountK, RowsRemaining, CountN, BlockCountK, ldc, 1.f, false
        );

        c_blk += ldc * RowsHandled;
        a_blksum_row += BlockCountK * RowsHandled;
        RowsRemaining -= RowsHandled;
    }
    return CountM;
}

void MLASCALL
QuantizeARow_CompInt8_avx512(
    size_t BlkLen,
    const float* A,
    size_t CountK,
    std::byte* QuantA,
    float* QuantAScale,
    float* AScaledBlkSum  // scale_k * Sum_blklen(a_i)
);

static void
SQ4BitGemmPackQuantBDataAndBlkSum512vnni(
    size_t N,
    size_t K,
    size_t BlkLen,
    MLAS_QNBIT_GEMM_COMPUTE_TYPE ComputeType,
    const std::byte* QuantBDataBegin,
    const float* QuantBScaleBegin,
    bool HasZeroPoint,
    const std::byte* QuantBZPBegin,
    PackedQuantBDataStruct<float, 4>& PackedQuantB,
    MLAS_THREADPOOL* ThreadPool
)
{
    assert(BlkLen >= 16 && BlkLen % 16 == 0);

    const size_t BlockCountK = MlasDivRoundup(K, BlkLen);

    size_t SubBlkLen = (BlkLen == 16) ? 16 : (BlkLen == 32 ? 32 : 64);
    if (ComputeType == SQNBIT_CompInt8) {
        SubBlkLen = 128;
    }
    PackQuantBDataAndBlkSum(N, BlockCountK, BlkLen, SubBlkLen, QuantBDataBegin, QuantBScaleBegin,
        HasZeroPoint, QuantBZPBegin, PackedQuantB, ThreadPool);
}

static void
SQ8BitGemmPackQuantBDataAndBlkSum512vnni(
    size_t N,
    size_t K,
    size_t BlkLen,
    MLAS_QNBIT_GEMM_COMPUTE_TYPE ComputeType,
    const std::byte* QuantBDataBegin,
    const float* QuantBScaleBegin,
    bool HasZeroPoint,
    const std::byte* QuantBZPBegin,
    PackedQuantBDataStruct<float, 8>& PackedQuantB,
    MLAS_THREADPOOL* ThreadPool
)
{
    assert(BlkLen >= 16 && BlkLen % 16 == 0);

    const size_t BlockCountK = MlasDivRoundup(K, BlkLen);

    size_t SubBlkLen = (BlkLen == 16) ? 16 : (BlkLen == 32 ? 32 : 64);
    if (ComputeType == SQNBIT_CompInt8) {
        SubBlkLen = 128;
    }
    Q8PackQuantBDataAndBlkSum(N, BlockCountK, BlkLen, SubBlkLen, QuantBDataBegin, QuantBScaleBegin,
        HasZeroPoint, QuantBZPBegin, PackedQuantB, ThreadPool);
}

//
// Kernel dispatch structure definition.
//
const MLAS_QNBIT_GEMM_DISPATCH MlasSQNBitGemmDispatchAvx512vnni = []() {
    MLAS_QNBIT_GEMM_DISPATCH d;

    d.Q4BitGemmPackQuantBDataSize = QNBitGemmPackQuantBDataSize<4>;
    d.Q8BitGemmPackQuantBDataSize = QNBitGemmPackQuantBDataSize<8>;
    d.SQ4BitGemmPackQuantBData = SQ4BitGemmPackQuantBData;
    d.SQ4BitGemmPackQuantBDataAndBlkSum = SQ4BitGemmPackQuantBDataAndBlkSum512vnni;
    d.SQ8BitGemmPackQuantBDataAndBlkSum = SQ8BitGemmPackQuantBDataAndBlkSum512vnni;

    d.QNBitGemmPerGemmWorkspaceSize = QNBitGemmPerGemmWorkspaceSize;
    d.QNBitGemmPerGemmWorkspaceAlignment = QNBitGemmPerGemmWorkspaceAlignment;

    d.SQ4BitGemmM1Kernel_CompFp32 = SQ4BitGemmM1Kernel_CompFp32;
    d.SQ4BitBlkDequantBForSgemm_CompFp32 = Q4BitBlkDequantBForSgemm_CompFp32_avx2;

    d.SQ4BitGemmKernel_BlkSum_CompInt8 = SQ4BitGemmKernel_BlkSum_CompInt8_avx512vnni;
    d.SQ8BitGemmKernel_BlkSum_CompInt8 = SQ8BitGemmKernel_BlkSum_CompInt8_avx512vnni;
    d.QuantizeARowComputeBlkSum_CompInt8 = QuantizeARow_CompInt8_avx512;

    return d;
}();
