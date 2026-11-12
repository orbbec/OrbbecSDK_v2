/*
 * SSE2LSX.h
 *
 * LoongArch (LSX) implementations of a subset of SSE2/SSE/SSEx intrinsics.
 *
 * Portions of this file were adapted from the SIMDe project
 * (SIMD Everywhere, https://github.com/simd-everywhere/simde).
 * SIMDe is distributed under the MIT License:
 *
 *   Copyright (c) 2017 Evan Nemerson <evan@nemerson.com>
 *
 *   Permission is hereby granted, free of charge, to any person obtaining
 *   a copy of this software and associated documentation files (the
 *   "Software"), to deal in the Software without restriction, including
 *   without limitation the rights to use, copy, modify, merge, publish,
 *   distribute, sublicense, and/or sell copies of the Software, and to
 *   permit persons to whom the Software is furnished to do so, subject to
 *   the following conditions:
 *
 *   The above copyright notice and this permission notice shall be
 *   included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *   LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *   OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SSE2LSX_H
#define SSE2LSX_H

#include <stdint.h>
#include <lsxintrin.h>

#if defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE static inline __attribute__((always_inline))
#else
#define FORCE_INLINE static inline
#endif

FORCE_INLINE __m128i _mm_set1_epi16(short w)
{
    return __lsx_vreplgr2vr_h(w);
}

FORCE_INLINE __m128 _mm_set_ps1(float f) {
    __m128 v = (__m128)__lsx_vldrepl_w(&f, 0);
    return v;
}
#define _mm_set1_ps(f) _mm_set_ps1(f)

FORCE_INLINE __m128i _mm_setzero_si128(void) {
    return __lsx_vreplgr2vr_w(0);
}

FORCE_INLINE __m128i _mm_set1_epi32(int i) {
    return __lsx_vreplgr2vr_w(i);
}

FORCE_INLINE __m128i _mm_loadl_epi64(__m128i const *p)
{
    return __lsx_vinsgr2vr_d(__lsx_vldrepl_d(p, 0), 0, 1);
}

FORCE_INLINE __m128i _mm_load_si128(const __m128i *p)
{
    return __lsx_vld(p, 0);
}

FORCE_INLINE __m128i _mm_loadu_si128(const __m128i *p) {
    return __lsx_vld(p, 0);
}

FORCE_INLINE __m128 _mm_loadu_ps(const float *p) {
    return (__m128)__lsx_vld(p, 0);
}

FORCE_INLINE void _mm_store_si128(__m128i *p, __m128i a)
{
    __lsx_vst(a, p, 0);
}

FORCE_INLINE void _mm_storeu_ps(float *p, __m128 a) {
    __lsx_vst((__m128i)a, p, 0);
}

FORCE_INLINE __m128i _mm_add_epi32(__m128i a, __m128i b) {
    return __lsx_vadd_w(a, b);
}

FORCE_INLINE __m128i _mm_sub_epi32(__m128i a, __m128i b) {
    return __lsx_vsub_w(a, b);
}

FORCE_INLINE __m128 _mm_add_ps(__m128 a, __m128 b) {
    return __lsx_vfadd_s(a, b);
}

FORCE_INLINE __m128 _mm_sub_ps(__m128 a, __m128 b) {
    return __lsx_vfsub_s(a, b);
}

FORCE_INLINE __m128 _mm_mul_ps(__m128 a, __m128 b) {
    return __lsx_vfmul_s(a, b);
}

FORCE_INLINE __m128 _mm_div_ps(__m128 a, __m128 b) {
    return __lsx_vfdiv_s(a, b);
}

FORCE_INLINE __m128 _mm_sqrt_ps(__m128 a) {
    return __lsx_vfsqrt_s(a);
}

FORCE_INLINE __m128 _mm_cvtepi32_ps(__m128i a)
{
    return __lsx_vffint_s_w(a);
}

FORCE_INLINE __m128i _mm_cvttps_epi32(__m128 a)
{
    return __lsx_vftintrz_w_s(a);
}

FORCE_INLINE __m128i _mm_cvtsi32_si128(int a)
{
    return __lsx_vinsgr2vr_w(__lsx_vreplgr2vr_w(0), a, 0);
}

FORCE_INLINE __m128i _mm_cvtepi16_epi32(__m128i a)
{
    return __lsx_vsllwil_w_h(a, 0);
}

FORCE_INLINE __m128i _mm_cvtepu16_epi32(__m128i a)
{
    return __lsx_vsllwil_wu_hu(a, 0);
}

FORCE_INLINE __m128i _mm_unpacklo_epi16(__m128i a, __m128i b) {
    return __lsx_vilvl_h(b, a);
}

FORCE_INLINE __m128i _mm_unpackhi_epi16(__m128i a, __m128i b) {
    return __lsx_vilvh_h(b, a);
}

FORCE_INLINE __m128i _mm_packus_epi16(const __m128i a, const __m128i b)
{
    return __lsx_vssrarni_bu_h(b, a, 0);
}

FORCE_INLINE __m128i _mm_packus_epi32(__m128i a, __m128i b)
{
    return __lsx_vssrarni_hu_w(b, a, 0);
}

FORCE_INLINE __m128i _mm_packs_epi32(__m128i a, __m128i b)
{
    return __lsx_vssrarni_h_w(b, a, 0);
}

#define _mm_srli_epi32(a, imm) (__lsx_vsrli_w((a), (imm)))
#define _mm_slli_epi32(a, imm) (__lsx_vslli_w((a), (imm)))

FORCE_INLINE __m128i _mm_and_si128(__m128i a, __m128i b)
{
    return __lsx_vand_v(a, b);
}

FORCE_INLINE __m128i _mm_or_si128(__m128i a, __m128i b)
{
    return __lsx_vor_v(a, b);
}

FORCE_INLINE __m128 _mm_or_ps(__m128 a, __m128 b)
{
    return (__m128)__lsx_vor_v((__m128i)a, (__m128i)b);
}

FORCE_INLINE __m128 _mm_and_ps(__m128 a, __m128 b)
{
    return (__m128)__lsx_vand_v((__m128i)a, (__m128i)b);
}

FORCE_INLINE __m128 _mm_cmpge_ps(__m128 a, __m128 b)
{
    return (__m128)__lsx_vfcmp_cle_s(b, a);
}

FORCE_INLINE __m128 _mm_cmplt_ps(__m128 a, __m128 b)
{
    return (__m128)__lsx_vfcmp_clt_s(b, a);
}

FORCE_INLINE __m128i _mm_mullo_epi32(__m128i a, __m128i b)
{
    return __lsx_vmul_w(a, b);
}

FORCE_INLINE int _mm_cvtsi128_si32(__m128i a)
{
    return  __lsx_vpickve2gr_w(a, 0);
}

FORCE_INLINE __m128i _mm_unpacklo_epi8(__m128i a, __m128i b)
{
    return __lsx_vilvl_b(b, a);
}

FORCE_INLINE __m128i _mm_maddubs_epi16(__m128i a, __m128i b)
{
    __m128i temp_ev = __lsx_vmulwev_h_bu_b(a, b);
    __m128i temp_od = __lsx_vmulwod_h_bu_b(a, b);
    return __lsx_vsadd_h(temp_ev, temp_od);
}

FORCE_INLINE __m128i _mm_madd_epi16(__m128i a, __m128i b)
{
    __m128i temp_ev = __lsx_vmulwev_w_h(a, b);
    return __lsx_vmaddwod_w_h(temp_ev, a, b);
}

#define _mm_extract_epi32(a, imm) __lsx_vpickve2gr_w(a, imm)

/* Constants for use with _mm_prefetch. */
enum _mm_hint {
    _MM_HINT_NTA = 0, /* load data to L1 and L2 cache, mark it as NTA */
    _MM_HINT_T0 = 1,  /* load data to L1 and L2 cache */
    _MM_HINT_T1 = 2,  /* load data to L2 cache only */
    _MM_HINT_T2 = 3,  /* load data to L2 cache only, mark it as NTA */
};

FORCE_INLINE void _mm_prefetch(char const *p, int i)
{
    (void) i;
    switch (i) {
    case _MM_HINT_NTA:
        __builtin_prefetch(p, 0, 0);
        break;
    case _MM_HINT_T0:
        __builtin_prefetch(p, 0, 3);
        break;
    case _MM_HINT_T1:
        __builtin_prefetch(p, 0, 2);
        break;
    case _MM_HINT_T2:
        __builtin_prefetch(p, 0, 1);
        break;
    default:
        break;
    }
}

#endif  // SSE2LSX_H
