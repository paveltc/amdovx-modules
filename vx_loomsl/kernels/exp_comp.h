/*
Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

// header file for exposure compensation implementation on CPU prototype
#include "kernels.h"

#define MAX_NUM_IMAGES_IN_STITCHED_OUTPUT	16
#define USE_LUMA_VALUES_FOR_GAIN			1

typedef struct _block_gain_info
{
	vx_float32 *block_gain_buf;
	vx_uint32   offset;
	vx_uint32   num_blocks_row;
	vx_uint32   num_blocks_col;
}block_gain_info;

class CExpCompensator
{
public:
	CExpCompensator();
	virtual ~CExpCompensator();
	virtual vx_status Process();
	virtual vx_status Initialize(vx_node node, vx_float32 alpha, vx_float32 beta, vx_array valid_roi, vx_image input, vx_image output);
	virtual vx_status DeInitialize();
	virtual vx_status SolveForGains(vx_float32 alpha, vx_float32 beta, vx_uint32 *IMat, vx_uint32 *NMat, vx_uint32 num_images, vx_array pGains, vx_uint32 rows, vx_uint32 cols);

protected:
	vx_uint32	m_numImages;
	vx_node		m_node;
	vx_uint32	m_width, m_height, m_stride,m_stride_x;
	vx_uint32   m_blockgainsStride;
	vx_float32	m_alpha, m_beta;
	vx_image	m_InputImage, m_OutputImage;
	vx_array	m_valid_roi;
	vx_rectangle_t m_pRoi_rect[MAX_NUM_IMAGES_IN_STITCHED_OUTPUT][MAX_NUM_IMAGES_IN_STITCHED_OUTPUT];	// assuming 
	block_gain_info *m_pblockgainInfo;
	vx_uint32 **m_NMat, **m_IMat;
	vx_float64 **m_AMat;
	vx_float32 *m_Gains;
	vx_rectangle_t mValidRect[MAX_NUM_IMAGES_IN_STITCHED_OUTPUT];

// functions
	virtual vx_status CompensateGains();
	virtual vx_status ApplyGains(void *in_base_addr);

private:
	void solve_gauss(vx_float64 **A, vx_float32* g, int num);
	vx_status applygains_thread_func(vx_int32 img_num, char *in_base_addr);
};
