/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2003
 *
 * Author(s):
 *	Volker Fischer, Phil Karn, Morgan Lius
 *
 * Description:
 *
	SSE2 fixed-point implementation of trellis update

	This code is based on a Viterbi sample code from
	Phil Karn, KA9Q (Dec 2001)
	simd-viterbi-2.0.3.zip -> viterbi27.c, mmxbfly27.s, ssebfly27.s
	homepage: http://www.ka9q.net

	Some comments to this MMX code:
	- To compare two 8-bit sized unsigned char, we need to apply a
	  special strategy:
	  psubusb mm5, mm1 // mm5 - mm1
	  pcmpeqb mm5, mm3 // mm3 = 0
	  We subtract unsigned with saturation and afterwards compare for
	  equal to zero. If value in mm1 is larger than the value in mm5, we
	  always get 0 as the result
	- Defining __asm Blocks as C Macros in windows: Put the __asm keyword in
	  front of each assembly instruction
	- If we want to use c-pointers to arrays (like "pOldTrelMetric"), we
	  first have to copy it to a register (like edx), otherwise we get
	  errors
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#include "ViterbiDecoder.h"

#if !defined(USE_MMX) && !defined(USE_SSE2)
/* Implementation *************************************************************/
void CViterbiDecoder::TrellisUpdateCPU(_DECISIONTYPE* pCurDec,
                                       _VITMETRTYPE* pCurTrelMetric, const _VITMETRTYPE* pOldTrelMetric,
                                        const _VITMETRTYPE* pchMet1, const _VITMETRTYPE* pchMet2)
{
#define CAP_ADD(a, b) (a+b>0xffff?0xffff:(a+b))

    /* c++ version of trellis update */
    for(int cur = 0; cur < 64; cur += 2)
    {
        int prev0 = cur / 2;
        int prev1 = 32 + prev0;
        int next = cur + 1;
        /* Calculate metrics from the two previous states, use the old
        metric from the previous states plus the "transition-metric" */
        
        const _VITMETRTYPE rFiStAccMetricPrev0 = CAP_ADD(pOldTrelMetric[prev0], chMet1[prev0]);
        const _VITMETRTYPE  rFiStAccMetricPrev1 = CAP_ADD(pOldTrelMetric[prev1], chMet2[prev0]);
        
        /* Take path with smallest metric */
        if (rFiStAccMetricPrev0 < rFiStAccMetricPrev1) {
            /* Save minimum metric for this state and store decision */
            pCurTrelMetric[cur] = rFiStAccMetricPrev0;
            pCurDec[cur] = 0;
        } else {
            /* Save minimum metric for this state and store decision */
            pCurTrelMetric[cur] = rFiStAccMetricPrev1;
            pCurDec[cur] = 1;
        }
        
        /* Second state in this set ----------------------------------- */
        /* The only difference is that we swapped the matric sets */
        const _VITMETRTYPE rSecStAccMetricPrev0 = CAP_ADD(pOldTrelMetric[prev0], chMet2[prev0]);
        const _VITMETRTYPE  rSecStAccMetricPrev1 = CAP_ADD(pOldTrelMetric[prev1], chMet1[prev0]);
        
        /* Take path with smallest metric */
        if (rSecStAccMetricPrev0 < rSecStAccMetricPrev1) {
            /* Save minimum metric for this state and store decision */
            pCurTrelMetric[next] = rSecStAccMetricPrev0;
            pCurDec[next] = 0;
        } else {
            /* Save minimum metric for this state and store decision */
            pCurTrelMetric[next] = rSecStAccMetricPrev1;
            pCurDec[next] = 1;
        }
    }

    // Normalize the result
    _VITMETRTYPE minium = pCurTrelMetric[0];
    if (minium <= 38250)
	return;

    for(int cur = 1; cur < 64; cur += 1)
    {
        if (minium > pCurTrelMetric[cur])
            minium = pCurTrelMetric[cur];
    }

    for(int cur = 0; cur < 64; cur += 1)
    {
        pCurTrelMetric[cur] -= minium;
    }
}
#endif
