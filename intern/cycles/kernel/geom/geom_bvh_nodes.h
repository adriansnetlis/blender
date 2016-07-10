/*
 * Copyright 2011-2016, Blender Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// TODO(sergey): Look into avoid use of full Transform and use 3x3 matrix and
// 3-vector which might be faster.
ccl_device_inline Transform bvh_unaligned_node_fetch_space(KernelGlobals *kg,
                                                           int nodeAddr,
                                                           int child)
{
	Transform space;
	const int child_addr = nodeAddr + child * 3;
	space.x = kernel_tex_fetch(__bvh_nodes, child_addr+1);
	space.y = kernel_tex_fetch(__bvh_nodes, child_addr+2);
	space.z = kernel_tex_fetch(__bvh_nodes, child_addr+3);
	space.w = make_float4(0.0f, 0.0f, 0.0f, 1.0f);
	return space;
}

#if !defined(__KERNEL_SSE2__)
ccl_device_inline int bvh_aligned_node_intersect(KernelGlobals *kg,
                                                 const float3 P,
                                                 const float3 idir,
                                                 const float t,
                                                 const int nodeAddr,
                                                 const uint visibility,
                                                 float dist[2])
{

	/* fetch node data */
	float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr+0);
	float4 node0 = kernel_tex_fetch(__bvh_nodes, nodeAddr+1);
	float4 node1 = kernel_tex_fetch(__bvh_nodes, nodeAddr+2);
	float4 node2 = kernel_tex_fetch(__bvh_nodes, nodeAddr+3);

	/* intersect ray against child nodes */
	float c0lox = (node0.x - P.x) * idir.x;
	float c0hix = (node0.z - P.x) * idir.x;
	float c0loy = (node1.x - P.y) * idir.y;
	float c0hiy = (node1.z - P.y) * idir.y;
	float c0loz = (node2.x - P.z) * idir.z;
	float c0hiz = (node2.z - P.z) * idir.z;
	float c0min = max4(min(c0lox, c0hix), min(c0loy, c0hiy), min(c0loz, c0hiz), 0.0f);
	float c0max = min4(max(c0lox, c0hix), max(c0loy, c0hiy), max(c0loz, c0hiz), t);

	float c1lox = (node0.y - P.x) * idir.x;
	float c1hix = (node0.w - P.x) * idir.x;
	float c1loy = (node1.y - P.y) * idir.y;
	float c1hiy = (node1.w - P.y) * idir.y;
	float c1loz = (node2.y - P.z) * idir.z;
	float c1hiz = (node2.w - P.z) * idir.z;
	float c1min = max4(min(c1lox, c1hix), min(c1loy, c1hiy), min(c1loz, c1hiz), 0.0f);
	float c1max = min4(max(c1lox, c1hix), max(c1loy, c1hiy), max(c1loz, c1hiz), t);

	dist[0] = c0min;
	dist[1] = c1min;

#ifdef __VISIBILITY_FLAG__
	/* this visibility test gives a 5% performance hit, how to solve? */
	return (((c0max >= c0min) && (__float_as_uint(cnodes.x) & visibility))? 1: 0) |
	       (((c1max >= c1min) && (__float_as_uint(cnodes.y) & visibility))? 2: 0);
#else
	return ((c0max >= c0min)? 1: 0) |
	       ((c1max >= c1min)? 2: 0);
#endif
}

ccl_device_inline int bvh_aligned_node_intersect_robust(KernelGlobals *kg,
                                                        const float3 P,
                                                        const float3 idir,
                                                        const float t,
                                                        const float difl,
                                                        const float extmax,
                                                        const int nodeAddr,
                                                        const uint visibility,
                                                        float dist[2])
{

	/* fetch node data */
	float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr+0);
	float4 node0 = kernel_tex_fetch(__bvh_nodes, nodeAddr+1);
	float4 node1 = kernel_tex_fetch(__bvh_nodes, nodeAddr+2);
	float4 node2 = kernel_tex_fetch(__bvh_nodes, nodeAddr+3);

	/* intersect ray against child nodes */
	float c0lox = (node0.x - P.x) * idir.x;
	float c0hix = (node0.z - P.x) * idir.x;
	float c0loy = (node1.x - P.y) * idir.y;
	float c0hiy = (node1.z - P.y) * idir.y;
	float c0loz = (node2.x - P.z) * idir.z;
	float c0hiz = (node2.z - P.z) * idir.z;
	float c0min = max4(min(c0lox, c0hix), min(c0loy, c0hiy), min(c0loz, c0hiz), 0.0f);
	float c0max = min4(max(c0lox, c0hix), max(c0loy, c0hiy), max(c0loz, c0hiz), t);

	float c1lox = (node0.y - P.x) * idir.x;
	float c1hix = (node0.w - P.x) * idir.x;
	float c1loy = (node1.y - P.y) * idir.y;
	float c1hiy = (node1.w - P.y) * idir.y;
	float c1loz = (node2.y - P.z) * idir.z;
	float c1hiz = (node2.w - P.z) * idir.z;
	float c1min = max4(min(c1lox, c1hix), min(c1loy, c1hiy), min(c1loz, c1hiz), 0.0f);
	float c1max = min4(max(c1lox, c1hix), max(c1loy, c1hiy), max(c1loz, c1hiz), t);

	if(difl != 0.0f) {
		float hdiff = 1.0f + difl;
		float ldiff = 1.0f - difl;
		if(__float_as_int(cnodes.z) & PATH_RAY_CURVE) {
			c0min = max(ldiff * c0min, c0min - extmax);
			c0max = min(hdiff * c0max, c0max + extmax);
		}
		if(__float_as_int(cnodes.w) & PATH_RAY_CURVE) {
			c1min = max(ldiff * c1min, c1min - extmax);
			c1max = min(hdiff * c1max, c1max + extmax);
		}
	}

	dist[0] = c0min;
	dist[1] = c1min;

#ifdef __VISIBILITY_FLAG__
	/* this visibility test gives a 5% performance hit, how to solve? */
	return (((c0max >= c0min) && (__float_as_uint(cnodes.x) & visibility))? 1: 0) |
	       (((c1max >= c1min) && (__float_as_uint(cnodes.y) & visibility))? 2: 0);
#else
	return ((c0max >= c0min)? 1: 0) |
	       ((c1max >= c1min)? 2: 0);
#endif
}

ccl_device_inline bool bvh_unaligned_node_intersect_child(
        KernelGlobals *kg,
        const float3 P,
        const float3 dir,
        const float t,
        int nodeAddr,
        int child,
        float dist[2])
{
	Transform space  = bvh_unaligned_node_fetch_space(kg, nodeAddr, child);
	float3 aligned_dir = transform_direction(&space, dir);
	float3 aligned_P = transform_point(&space, P);
	float3 nrdir = -bvh_inverse_direction(aligned_dir);
	float3 tLowerXYZ = aligned_P * nrdir;
	float3 tUpperXYZ = tLowerXYZ - nrdir;
	const float tNearX = min(tLowerXYZ.x, tUpperXYZ.x);
	const float tNearY = min(tLowerXYZ.y, tUpperXYZ.y);
	const float tNearZ = min(tLowerXYZ.z, tUpperXYZ.z);
	const float tFarX  = max(tLowerXYZ.x, tUpperXYZ.x);
	const float tFarY  = max(tLowerXYZ.y, tUpperXYZ.y);
	const float tFarZ  = max(tLowerXYZ.z, tUpperXYZ.z);
	const float tNear  = max4(0.0f, tNearX, tNearY, tNearZ);
	const float tFar   = min4(t, tFarX, tFarY, tFarZ);
	*dist = tNear;
	return tNear <= tFar;
}

ccl_device_inline bool bvh_unaligned_node_intersect_child_robust(
        KernelGlobals *kg,
        const float3 P,
        const float3 dir,
        const float t,
        const float difl,
        int nodeAddr,
        int child,
        float dist[2])
{
	Transform space  = bvh_unaligned_node_fetch_space(kg, nodeAddr, child);
	float3 aligned_dir = transform_direction(&space, dir);
	float3 aligned_P = transform_point(&space, P);
	float3 nrdir = -bvh_inverse_direction(aligned_dir);
	float3 tLowerXYZ = aligned_P * nrdir;
	float3 tUpperXYZ = tLowerXYZ - nrdir;
	const float tNearX = min(tLowerXYZ.x, tUpperXYZ.x);
	const float tNearY = min(tLowerXYZ.y, tUpperXYZ.y);
	const float tNearZ = min(tLowerXYZ.z, tUpperXYZ.z);
	const float tFarX  = max(tLowerXYZ.x, tUpperXYZ.x);
	const float tFarY  = max(tLowerXYZ.y, tUpperXYZ.y);
	const float tFarZ  = max(tLowerXYZ.z, tUpperXYZ.z);
	const float tNear  = max4(0.0f, tNearX, tNearY, tNearZ);
	const float tFar   = min4(t, tFarX, tFarY, tFarZ);
	*dist = tNear;
	if(difl != 0.0f) {
		/* TODO(sergey): Same as for QBVH, needs a proper use. */
		const float round_down = 1.0f - difl;
		const float round_up = 1.0f + difl;
		return round_down*tNear <= round_up*tFar;
	}
	else {
		return tNear <= tFar;
	}
}

ccl_device_inline int bvh_unaligned_node_intersect(KernelGlobals *kg,
                                                   const float3 P,
                                                   const float3 dir,
                                                   const float3 idir,
                                                   const float t,
                                                   const int nodeAddr,
                                                   const uint visibility,
                                                   float dist[2])
{
	int mask = 0;
	float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr+0);
	if(bvh_unaligned_node_intersect_child(kg, P, dir, t, nodeAddr, 0, &dist[0])) {
#ifdef __VISIBILITY_FLAG__
		if((__float_as_uint(cnodes.x) & visibility))
#endif
		{
			mask |= 1;
		}
	}
	if(bvh_unaligned_node_intersect_child(kg, P, dir, t, nodeAddr, 1, &dist[1])) {
#ifdef __VISIBILITY_FLAG__
		if((__float_as_uint(cnodes.y) & visibility))
#endif
		{
			mask |= 2;
		}
	}
	return mask;
}

ccl_device_inline int bvh_unaligned_node_intersect_robust(KernelGlobals *kg,
                                                          const float3 P,
                                                          const float3 dir,
                                                          const float3 idir,
                                                          const float t,
                                                          const float difl,
                                                          const float extmax,
                                                          const int nodeAddr,
                                                          const uint visibility,
                                                          float dist[2])
{
	int mask = 0;
	float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr+0);
	if(bvh_unaligned_node_intersect_child_robust(kg, P, dir, t, difl, nodeAddr, 0, &dist[0])) {
#ifdef __VISIBILITY_FLAG__
		if((__float_as_uint(cnodes.x) & visibility))
#endif
		{
			mask |= 1;
		}
	}
	if(bvh_unaligned_node_intersect_child_robust(kg, P, dir, t, difl, nodeAddr, 1, &dist[1])) {
#ifdef __VISIBILITY_FLAG__
		if((__float_as_uint(cnodes.y) & visibility))
#endif
		{
			mask |= 2;
		}
	}
	return mask;
}

ccl_device_inline int bvh_node_intersect(KernelGlobals *kg,
                                         const float3 P,
                                         const float3 dir,
                                         const float3 idir,
                                         const float t,
                                         const int nodeAddr,
                                         const uint visibility,
                                         float dist[2])
{
	float4 node = kernel_tex_fetch(__bvh_nodes, nodeAddr);
	if(__float_as_uint(node.x) & PATH_RAY_NODE_UNALIGNED) {
		return bvh_unaligned_node_intersect(kg,
		                                    P,
		                                    dir,
		                                    idir,
		                                    t,
		                                    nodeAddr,
		                                    visibility,
		                                    dist);
	}
	else {
		return bvh_aligned_node_intersect(kg,
		                                  P,
		                                  idir,
		                                  t,
		                                  nodeAddr,
		                                  visibility,
		                                  dist);
	}
}

ccl_device_inline int bvh_node_intersect_robust(KernelGlobals *kg,
                                                const float3 P,
                                                const float3 dir,
                                                const float3 idir,
                                                const float t,
                                                const float difl,
                                                const float extmax,
                                                const int nodeAddr,
                                                const uint visibility,
                                                float dist[2])
{
	float4 node = kernel_tex_fetch(__bvh_nodes, nodeAddr);
	if(__float_as_uint(node.x) & PATH_RAY_NODE_UNALIGNED) {
		return bvh_unaligned_node_intersect_robust(kg,
		                                           P,
		                                           dir,
		                                           idir,
		                                           t,
		                                           difl,
		                                           extmax,
		                                           nodeAddr,
		                                           visibility,
		                                           dist);
	}
	else {
		return bvh_aligned_node_intersect_robust(kg,
		                                         P,
		                                         idir,
		                                         t,
		                                         difl,
		                                         extmax,
		                                         nodeAddr,
		                                         visibility,
		                                         dist);
	}
}
#else  /* !defined(__KERNEL_SSE2__) */

int ccl_device_inline bvh_aligned_node_intersect(
        KernelGlobals *kg,
        const float3& P,
        const float3& dir,
        const ssef& tsplat,
        const ssef Psplat[3],
        const ssef idirsplat[3],
        const shuffle_swap_t shufflexyz[3],
        const int nodeAddr,
        const uint visibility,
        float dist[2])
{
	/* Intersect two child bounding boxes, SSE3 version adapted from Embree */
	const ssef pn = cast(ssei(0, 0, 0x80000000, 0x80000000));

	/* fetch node data */
	const ssef *bvh_nodes = (ssef*)kg->__bvh_nodes.data + nodeAddr;

	/* intersect ray against child nodes */
	const ssef tminmaxx = (shuffle_swap(bvh_nodes[1], shufflexyz[0]) - Psplat[0]) * idirsplat[0];
	const ssef tminmaxy = (shuffle_swap(bvh_nodes[2], shufflexyz[1]) - Psplat[1]) * idirsplat[1];
	const ssef tminmaxz = (shuffle_swap(bvh_nodes[3], shufflexyz[2]) - Psplat[2]) * idirsplat[2];

	/* calculate { c0min, c1min, -c0max, -c1max} */
	ssef minmax = max(max(tminmaxx, tminmaxy), max(tminmaxz, tsplat));
	const ssef tminmax = minmax ^ pn;
	const sseb lrhit = tminmax <= shuffle<2, 3, 0, 1>(tminmax);

	dist[0] = tminmax[0];
	dist[1] = tminmax[1];

	int mask = movemask(lrhit);

#  ifdef __VISIBILITY_FLAG__
	/* this visibility test gives a 5% performance hit, how to solve? */
	float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr+0);
	int cmask = (((mask & 1) && (__float_as_uint(cnodes.x) & visibility))? 1: 0) |
	            (((mask & 2) && (__float_as_uint(cnodes.y) & visibility))? 2: 0);
	return cmask;
#  else
	return mask & 3;
#  endif
}

int ccl_device_inline bvh_aligned_node_intersect_robust(
        KernelGlobals *kg,
        const float3& P,
        const float3& dir,
        const ssef& tsplat,
        const ssef Psplat[3],
        const ssef idirsplat[3],
        const shuffle_swap_t shufflexyz[3],
        const float difl,
        const float extmax,
        const int nodeAddr,
        const uint visibility,
        float dist[2])
{
	/* Intersect two child bounding boxes, SSE3 version adapted from Embree */
	const ssef pn = cast(ssei(0, 0, 0x80000000, 0x80000000));

	/* fetch node data */
	const ssef *bvh_nodes = (ssef*)kg->__bvh_nodes.data + nodeAddr;

	/* intersect ray against child nodes */
	const ssef tminmaxx = (shuffle_swap(bvh_nodes[1], shufflexyz[0]) - Psplat[0]) * idirsplat[0];
	const ssef tminmaxy = (shuffle_swap(bvh_nodes[2], shufflexyz[1]) - Psplat[1]) * idirsplat[1];
	const ssef tminmaxz = (shuffle_swap(bvh_nodes[3], shufflexyz[2]) - Psplat[2]) * idirsplat[2];

	/* calculate { c0min, c1min, -c0max, -c1max} */
	ssef minmax = max(max(tminmaxx, tminmaxy), max(tminmaxz, tsplat));
	const ssef tminmax = minmax ^ pn;

	if(difl != 0.0f) {
		float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr+0);
		float4 *tminmaxview = (float4*)&tminmax;
		float& c0min = tminmaxview->x, &c1min = tminmaxview->y;
		float& c0max = tminmaxview->z, &c1max = tminmaxview->w;
		float hdiff = 1.0f + difl;
		float ldiff = 1.0f - difl;
		if(__float_as_int(cnodes.x) & PATH_RAY_CURVE) {
			c0min = max(ldiff * c0min, c0min - extmax);
			c0max = min(hdiff * c0max, c0max + extmax);
		}
		if(__float_as_int(cnodes.y) & PATH_RAY_CURVE) {
			c1min = max(ldiff * c1min, c1min - extmax);
			c1max = min(hdiff * c1max, c1max + extmax);
		}
	}

	const sseb lrhit = tminmax <= shuffle<2, 3, 0, 1>(tminmax);

	dist[0] = tminmax[0];
	dist[1] = tminmax[1];

	int mask = movemask(lrhit);

#  ifdef __VISIBILITY_FLAG__
	/* this visibility test gives a 5% performance hit, how to solve? */
	float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr+0);
	int cmask = (((mask & 1) && (__float_as_uint(cnodes.x) & visibility))? 1: 0) |
	            (((mask & 2) && (__float_as_uint(cnodes.y) & visibility))? 2: 0);
	return cmask;
#  else
	return mask & 3;
#  endif
}

int ccl_device_inline bvh_unaligned_node_intersect(KernelGlobals *kg,
                                                   const float3 P,
                                                   const float3 dir,
                                                   const ssef& tnear,
                                                   const ssef& tfar,
                                                   const int nodeAddr,
                                                   const uint visibility,
                                                   float dist[2])
{
	Transform space0 = bvh_unaligned_node_fetch_space(kg, nodeAddr, 0);
	Transform space1 = bvh_unaligned_node_fetch_space(kg, nodeAddr, 1);

	float3 aligned_dir0 = transform_direction(&space0, dir),
	       aligned_dir1 = transform_direction(&space1, dir);;
	float3 aligned_P0 = transform_point(&space0, P),
	       aligned_P1 = transform_point(&space1, P);
	float3 nrdir0 = -bvh_inverse_direction(aligned_dir0),
	       nrdir1 = -bvh_inverse_direction(aligned_dir1);

	ssef tLowerX = ssef(aligned_P0.x * nrdir0.x,
	                    aligned_P1.x * nrdir1.x,
	                    0.0f, 0.0f),
	     tLowerY = ssef(aligned_P0.y * nrdir0.y,
	                    aligned_P1.y * nrdir1.y,
	                    0.0f,
	                    0.0f),
	     tLowerZ = ssef(aligned_P0.z * nrdir0.z,
	                    aligned_P1.z * nrdir1.z,
	                    0.0f,
	                    0.0f);

	ssef tUpperX = tLowerX - ssef(nrdir0.x, nrdir1.x, 0.0f, 0.0f),
	     tUpperY = tLowerY - ssef(nrdir0.y, nrdir1.y, 0.0f, 0.0f),
	     tUpperZ = tLowerZ - ssef(nrdir0.z, nrdir1.z, 0.0f, 0.0f);

	ssef tnear_x = min(tLowerX, tUpperX);
	ssef tnear_y = min(tLowerY, tUpperY);
	ssef tnear_z = min(tLowerZ, tUpperZ);
	ssef tfar_x = max(tLowerX, tUpperX);
	ssef tfar_y = max(tLowerY, tUpperY);
	ssef tfar_z = max(tLowerZ, tUpperZ);

	const ssef tNear = max4(tnear_x, tnear_y, tnear_z, tnear);
	const ssef tFar = min4(tfar_x, tfar_y, tfar_z, tfar);
	sseb vmask = tNear <= tFar;
	dist[0] = tNear.f[0];
	dist[1] = tNear.f[1];

	int mask = (int)movemask(vmask);

#  ifdef __VISIBILITY_FLAG__
	/* this visibility test gives a 5% performance hit, how to solve? */
	float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr+0);
	int cmask = (((mask & 1) && (__float_as_uint(cnodes.x) & visibility))? 1: 0) |
	            (((mask & 2) && (__float_as_uint(cnodes.y) & visibility))? 2: 0);
	return cmask;
#  else
	return mask & 3;
#  endif
}

int ccl_device_inline bvh_unaligned_node_intersect_robust(KernelGlobals *kg,
                                                          const float3 P,
                                                          const float3 dir,
                                                          const ssef& tnear,
                                                          const ssef& tfar,
                                                          const float difl,
                                                          const int nodeAddr,
                                                          const uint visibility,
                                                          float dist[2])
{
	Transform space0 = bvh_unaligned_node_fetch_space(kg, nodeAddr, 0);
	Transform space1 = bvh_unaligned_node_fetch_space(kg, nodeAddr, 1);

	float3 aligned_dir0 = transform_direction(&space0, dir),
	       aligned_dir1 = transform_direction(&space1, dir);;
	float3 aligned_P0 = transform_point(&space0, P),
	       aligned_P1 = transform_point(&space1, P);
	float3 nrdir0 = -bvh_inverse_direction(aligned_dir0),
	       nrdir1 = -bvh_inverse_direction(aligned_dir1);

	ssef tLowerX = ssef(aligned_P0.x * nrdir0.x,
	                    aligned_P1.x * nrdir1.x,
	                    0.0f, 0.0f),
	     tLowerY = ssef(aligned_P0.y * nrdir0.y,
	                    aligned_P1.y * nrdir1.y,
	                    0.0f,
	                    0.0f),
	     tLowerZ = ssef(aligned_P0.z * nrdir0.z,
	                    aligned_P1.z * nrdir1.z,
	                    0.0f,
	                    0.0f);

	ssef tUpperX = tLowerX - ssef(nrdir0.x, nrdir1.x, 0.0f, 0.0f),
	     tUpperY = tLowerY - ssef(nrdir0.y, nrdir1.y, 0.0f, 0.0f),
	     tUpperZ = tLowerZ - ssef(nrdir0.z, nrdir1.z, 0.0f, 0.0f);

	ssef tnear_x = min(tLowerX, tUpperX);
	ssef tnear_y = min(tLowerY, tUpperY);
	ssef tnear_z = min(tLowerZ, tUpperZ);
	ssef tfar_x = max(tLowerX, tUpperX);
	ssef tfar_y = max(tLowerY, tUpperY);
	ssef tfar_z = max(tLowerZ, tUpperZ);

	const ssef tNear = max4(tnear_x, tnear_y, tnear_z, tnear);
	const ssef tFar = min4(tfar_x, tfar_y, tfar_z, tfar);
	sseb vmask;
	if(difl != 0.0f) {
		const float round_down = 1.0f - difl;
		const float round_up = 1.0f + difl;
		vmask = round_down*tNear <= round_up*tFar;
	}
	else {
		vmask = tNear <= tFar;
	}

	dist[0] = tNear.f[0];
	dist[1] = tNear.f[1];

	int mask = (int)movemask(vmask);

#  ifdef __VISIBILITY_FLAG__
	/* this visibility test gives a 5% performance hit, how to solve? */
	float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr+0);
	int cmask = (((mask & 1) && (__float_as_uint(cnodes.x) & visibility))? 1: 0) |
	            (((mask & 2) && (__float_as_uint(cnodes.y) & visibility))? 2: 0);
	return cmask;
#  else
	return mask & 3;
#  endif
}

ccl_device_inline int bvh_node_intersect(KernelGlobals *kg,
                                         const float3& P,
                                         const float3& dir,
                                         const ssef& tnear,
                                         const ssef& tfar,
                                         const ssef& tsplat,
                                         const ssef Psplat[3],
                                         const ssef idirsplat[3],
                                         const shuffle_swap_t shufflexyz[3],
                                         const int nodeAddr,
                                         const uint visibility,
                                         float dist[2])
{
	float4 node = kernel_tex_fetch(__bvh_nodes, nodeAddr);
	if(__float_as_uint(node.x) & PATH_RAY_NODE_UNALIGNED) {
		return bvh_unaligned_node_intersect(kg,
		                                    P,
		                                    dir,
		                                    tnear,
		                                    tfar,
		                                    nodeAddr,
		                                    visibility,
		                                    dist);
	}
	else {
		return bvh_aligned_node_intersect(kg,
		                                  P,
		                                  dir,
		                                  tsplat,
		                                  Psplat,
		                                  idirsplat,
		                                  shufflexyz,
		                                  nodeAddr,
		                                  visibility,
		                                  dist);
	}
}

ccl_device_inline int bvh_node_intersect_robust(KernelGlobals *kg,
                                                const float3& P,
                                                const float3& dir,
                                                const ssef& tnear,
                                                const ssef& tfar,
                                                const ssef& tsplat,
                                                const ssef Psplat[3],
                                                const ssef idirsplat[3],
                                                const shuffle_swap_t shufflexyz[3],
                                                const float difl,
                                                const float extmax,
                                                const int nodeAddr,
                                                const uint visibility,
                                                float dist[2])
{
	float4 node = kernel_tex_fetch(__bvh_nodes, nodeAddr);
	if(__float_as_uint(node.x) & PATH_RAY_NODE_UNALIGNED) {
		return bvh_unaligned_node_intersect_robust(kg,
		                                           P,
		                                           dir,
		                                           tnear,
		                                           tfar,
		                                           difl,
		                                           nodeAddr,
		                                           visibility,
		                                           dist);
	}
	else {
		return bvh_aligned_node_intersect_robust(kg,
		                                         P,
		                                         dir,
		                                         tsplat,
		                                         Psplat,
		                                         idirsplat,
		                                         shufflexyz,
		                                         difl,
		                                         extmax,
		                                         nodeAddr,
		                                         visibility,
		                                         dist);
	}
}
#endif  /* !defined(__KERNEL_SSE2__) */