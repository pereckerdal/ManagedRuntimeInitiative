/*
 * Copyright 2000-2003 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Sun designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Sun in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 */

#include "GraphicsPrimitiveMgr.h"
#include "Region.h"

#include "sun_java2d_loops_MaskBlit.h"

/*
 * Class:     sun_java2d_loops_MaskBlit
 * Method:    MaskBlit
 * Signature: (Lsun/java2d/SurfaceData;Lsun/java2d/SurfaceData;Ljava/awt/Composite;IIIIII[BII)V
 */
JNIEXPORT void JNICALL
Java_sun_java2d_loops_MaskBlit_MaskBlit
    (JNIEnv *env, jobject self,
     jobject srcData, jobject dstData, jobject comp, jobject clip,
     jint srcx, jint srcy, jint dstx, jint dsty, jint width, jint height,
     jbyteArray maskArray, jint maskoff, jint maskscan)
{
    SurfaceDataOps *srcOps;
    SurfaceDataOps *dstOps;
    SurfaceDataRasInfo srcInfo;
    SurfaceDataRasInfo dstInfo;
    NativePrimitive *pPrim;
    CompositeInfo compInfo;
    RegionData clipInfo;

    pPrim = GetNativePrim(env, self);
    if (pPrim == NULL) {
        return;
    }
    if (pPrim->pCompType->getCompInfo != NULL) {
        (*pPrim->pCompType->getCompInfo)(env, &compInfo, comp);
    }
    if (Region_GetInfo(env, clip, &clipInfo)) {
        return;
    }

    srcOps = SurfaceData_GetOps(env, srcData);
    dstOps = SurfaceData_GetOps(env, dstData);
    if (srcOps == 0 || dstOps == 0) {
        return;
    }

    srcInfo.bounds.x1 = srcx;
    srcInfo.bounds.y1 = srcy;
    srcInfo.bounds.x2 = srcx + width;
    srcInfo.bounds.y2 = srcy + height;
    dstInfo.bounds.x1 = dstx;
    dstInfo.bounds.y1 = dsty;
    dstInfo.bounds.x2 = dstx + width;
    dstInfo.bounds.y2 = dsty + height;
    srcx -= dstx;
    srcy -= dsty;
    SurfaceData_IntersectBounds(&dstInfo.bounds, &clipInfo.bounds);
    if (srcOps->Lock(env, srcOps, &srcInfo, pPrim->srcflags) != SD_SUCCESS) {
        return;
    }
    if (dstOps->Lock(env, dstOps, &dstInfo, pPrim->dstflags) != SD_SUCCESS) {
        SurfaceData_InvokeUnlock(env, srcOps, &srcInfo);
        return;
    }
    SurfaceData_IntersectBlitBounds(&dstInfo.bounds, &srcInfo.bounds,
                                    srcx, srcy);
    Region_IntersectBounds(&clipInfo, &dstInfo.bounds);

    if (!Region_IsEmpty(&clipInfo)) {
        srcOps->GetRasInfo(env, srcOps, &srcInfo);
        dstOps->GetRasInfo(env, dstOps, &dstInfo);
        if (srcInfo.rasBase && dstInfo.rasBase) {
            SurfaceDataBounds span;
            unsigned char *pMask =
                (maskArray
                 ? (*env)->GetPrimitiveArrayCritical(env, maskArray, 0)
                 : 0);
            jint savesx = srcInfo.bounds.x1;
            jint savedx = dstInfo.bounds.x1;
            Region_StartIteration(env, &clipInfo);
            while (Region_NextIteration(&clipInfo, &span)) {
                void *pSrc = PtrCoord(srcInfo.rasBase,
                                      srcx + span.x1, srcInfo.pixelStride,
                                      srcy + span.y1, srcInfo.scanStride);
                void *pDst = PtrCoord(dstInfo.rasBase,
                                      span.x1, dstInfo.pixelStride,
                                      span.y1, dstInfo.scanStride);
                maskoff += ((span.y1 - dsty) * maskscan + (span.x1 - dstx));
                /*
                 * Fix for 4804375
                 * REMIND: There should probably be a better
                 * way to give the span coordinates to the
                 * inner loop.  This is only really needed
                 * for the 1, 2, and 4 bit loops.
                 */
                srcInfo.bounds.x1 = srcx + span.x1;
                dstInfo.bounds.x1 = span.x1;
                (*pPrim->funcs.maskblit)(pDst, pSrc,
                                         pMask, maskoff, maskscan,
                                         span.x2 - span.x1, span.y2 - span.y1,
                                         &dstInfo, &srcInfo,
                                         pPrim, &compInfo);
            }
            Region_EndIteration(env, &clipInfo);
            if (pMask) {
                (*env)->ReleasePrimitiveArrayCritical(env, maskArray,
                                                      pMask, JNI_ABORT);
            }
            srcInfo.bounds.x1 = savesx;
            dstInfo.bounds.x1 = savedx;
        }
        SurfaceData_InvokeRelease(env, dstOps, &dstInfo);
        SurfaceData_InvokeRelease(env, srcOps, &srcInfo);
    }
    SurfaceData_InvokeUnlock(env, dstOps, &dstInfo);
    SurfaceData_InvokeUnlock(env, srcOps, &srcInfo);
}
