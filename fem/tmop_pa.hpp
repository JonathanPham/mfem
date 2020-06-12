// Copyright (c) 2010-2020, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.

#ifndef MFEM_TMOP_PA_HPP
#define MFEM_TMOP_PA_HPP

#include "../config/config.hpp"
#include "../linalg/dtensor.hpp"

namespace mfem
{

namespace kernels
{

/// Load B1d & G1d matrices into shared memory
template<int MD1, int MQ1>
MFEM_HOST_DEVICE inline void LoadBG(const int D1D, const int Q1D,
                                    const DeviceTensor<2, const double> b,
                                    const DeviceTensor<2, const double> g,
                                    double s_BG[2][MQ1*MD1])
{
   const int tidz = MFEM_THREAD_ID(z);
   double (*B)[MD1] = (double (*)[MD1])(s_BG+0);
   double (*G)[MD1] = (double (*)[MD1])(s_BG+1);
   if (tidz == 0)
   {
      MFEM_FOREACH_THREAD(d,y,D1D)
      {
         MFEM_FOREACH_THREAD(q,x,Q1D)
         {
            B[q][d] = b(q,d);
            G[q][d] = g(q,d);
         }
      }
   }
   MFEM_SYNC_THREAD;
}

/// Load Bt1d & Gt1d matrices into shared memory
template<int MD1, int MQ1>
MFEM_HOST_DEVICE inline void LoadBGt(const int D1D, const int Q1D,
                                     const DeviceTensor<2, const double> b,
                                     const DeviceTensor<2, const double> g,
                                     double sBG[2][MQ1*MD1])
{
   const int tidz = MFEM_THREAD_ID(z);
   double (*Bt)[MQ1] = (double (*)[MQ1]) (sBG+0);
   double (*Gt)[MQ1] = (double (*)[MQ1]) (sBG+1);
   if (tidz == 0)
   {
      MFEM_FOREACH_THREAD(d,y,D1D)
      {
         MFEM_FOREACH_THREAD(q,x,Q1D)
         {
            Bt[d][q] = b(q,d);
            Gt[d][q] = g(q,d);
         }
      }
   }
   MFEM_SYNC_THREAD;
}

/// Load 2D input vector into shared memory
template<int MD1, int NBZ>
MFEM_HOST_DEVICE inline void LoadX(const int e,
                                   const int D1D,
                                   const DeviceTensor<4, const double> X,
                                   double s_X[2][NBZ][MD1*MD1])
{
   const int tidz = MFEM_THREAD_ID(z);
   double (*Xx)[MD1] = (double (*)[MD1])(s_X[0] + tidz);
   double (*Xy)[MD1] = (double (*)[MD1])(s_X[1] + tidz);
   MFEM_FOREACH_THREAD(dy,y,D1D)
   {
      MFEM_FOREACH_THREAD(dx,x,D1D)
      {
         Xx[dy][dx] = X(dx,dy,0,e);
         Xy[dy][dx] = X(dx,dy,1,e);
      }
   }
   MFEM_SYNC_THREAD;
}

/// 2D Grad, stage 1/2
template<int MD1, int MQ1, int NBZ>
MFEM_HOST_DEVICE inline void GradX(const int D1D, const int Q1D,
                                   const double s_BG[2][MQ1*MD1],
                                   const double s_X[2][NBZ][MD1*MD1],
                                   double s_DQ[4][NBZ][MD1*MQ1])
{
   const int tidz = MFEM_THREAD_ID(z);
   double (*B)[MD1] = (double (*)[MD1])(s_BG+0);
   double (*G)[MD1] = (double (*)[MD1])(s_BG+1);

   double (*Xx)[MD1]  = (double (*)[MD1])(s_X[0] + tidz);
   double (*Xy)[MD1]  = (double (*)[MD1])(s_X[1] + tidz);

   double (*XxB)[MQ1] = (double (*)[MQ1])(s_DQ[0] + tidz);
   double (*XxG)[MQ1] = (double (*)[MQ1])(s_DQ[1] + tidz);
   double (*XyB)[MQ1] = (double (*)[MQ1])(s_DQ[2] + tidz);
   double (*XyG)[MQ1] = (double (*)[MQ1])(s_DQ[3] + tidz);
   MFEM_FOREACH_THREAD(dy,y,D1D)
   {
      MFEM_FOREACH_THREAD(qx,x,Q1D)
      {
         double u[2] = {0.0, 0.0};
         double v[2] = {0.0, 0.0};
         for (int dx = 0; dx < D1D; ++dx)
         {
            const double xx = Xx[dy][dx];
            const double xy = Xy[dy][dx];
            u[0] += B[qx][dx] * xx;
            v[0] += G[qx][dx] * xx;
            u[1] += B[qx][dx] * xy;
            v[1] += G[qx][dx] * xy;
         }
         XxB[dy][qx] = u[0];
         XxG[dy][qx] = v[0];
         XyB[dy][qx] = u[1];
         XyG[dy][qx] = v[1];
      }
   }
   MFEM_SYNC_THREAD;
}

/// 2D Grad, stage 2/2
template<int MD1, int MQ1, int NBZ>
MFEM_HOST_DEVICE inline void GradY(const int D1D, const int Q1D,
                                   const double s_BG[2][MQ1*MD1],
                                   const double s_DQ[4][NBZ][MD1*MQ1],
                                   double s_QQ[4][NBZ][MQ1*MQ1])
{
   const int tidz = MFEM_THREAD_ID(z);
   double (*B)[MD1] = (double (*)[MD1])(s_BG+0);
   double (*G)[MD1] = (double (*)[MD1])(s_BG+1);

   double (*XxB)[MQ1] = (double (*)[MQ1])(s_DQ[0] + tidz);
   double (*XxG)[MQ1] = (double (*)[MQ1])(s_DQ[1] + tidz);
   double (*XyB)[MQ1] = (double (*)[MQ1])(s_DQ[2] + tidz);
   double (*XyG)[MQ1] = (double (*)[MQ1])(s_DQ[3] + tidz);

   double (*Xx0)[MQ1] = (double (*)[MQ1])(s_QQ[0] + tidz);
   double (*Xx1)[MQ1] = (double (*)[MQ1])(s_QQ[1] + tidz);
   double (*Xy0)[MQ1] = (double (*)[MQ1])(s_QQ[2] + tidz);
   double (*Xy1)[MQ1] = (double (*)[MQ1])(s_QQ[3] + tidz);

   MFEM_FOREACH_THREAD(qy,y,Q1D)
   {
      MFEM_FOREACH_THREAD(qx,x,Q1D)
      {
         double u[2] = {0.0, 0.0};
         double v[2] = {0.0, 0.0};
         for (int dy = 0; dy < D1D; ++dy)
         {
            u[0] += XxG[dy][qx] * B[qy][dy];
            v[0] += XxB[dy][qx] * G[qy][dy];
            u[1] += XyG[dy][qx] * B[qy][dy];
            v[1] += XyB[dy][qx] * G[qy][dy];
         }
         Xx0[qy][qx] = u[0];
         Xx1[qy][qx] = v[0];
         Xy0[qy][qx] = u[1];
         Xy1[qy][qx] = v[1];
      }
   }
   MFEM_SYNC_THREAD;
}

/// Load 2D GradXY(X) to Jpr
template<int MQ1, int NBZ>
MFEM_HOST_DEVICE inline
const double *PullGradXY(const int qx, const int qy,
                         const double s_QQ[4][NBZ][MQ1*MQ1],
                         double *Jpr)
{
   const int tidz = MFEM_THREAD_ID(z);
   double (*Xx0)[MQ1] = (double (*)[MQ1])(s_QQ[0] + tidz);
   double (*Xx1)[MQ1] = (double (*)[MQ1])(s_QQ[1] + tidz);
   double (*Xy0)[MQ1] = (double (*)[MQ1])(s_QQ[2] + tidz);
   double (*Xy1)[MQ1] = (double (*)[MQ1])(s_QQ[3] + tidz);
   Jpr[0] = Xx0[qy][qx];
   Jpr[1] = Xy0[qy][qx];
   Jpr[2] = Xx1[qy][qx];
   Jpr[3] = Xy1[qy][qx];
   return Jpr;
}

/// Push 2D GradXY(X) from A
template<int MQ1, int NBZ>
MFEM_HOST_DEVICE inline
void PushGradXY(const int qx, const int qy,
                const double *A,
                double s_QQ[4][NBZ][MQ1*MQ1])
{
   const int tidz = MFEM_THREAD_ID(z);
   double (*Xx0)[MQ1] = (double (*)[MQ1])(s_QQ[0] + tidz);
   double (*Xx1)[MQ1] = (double (*)[MQ1])(s_QQ[1] + tidz);
   double (*Xy0)[MQ1] = (double (*)[MQ1])(s_QQ[2] + tidz);
   double (*Xy1)[MQ1] = (double (*)[MQ1])(s_QQ[3] + tidz);

   Xx0[qy][qx] = A[0];
   Xy0[qy][qx] = A[2];
   Xx1[qy][qx] = A[1];
   Xy1[qy][qx] = A[3];
}

/// 2D GradT, stage 1/2
template<int MD1, int MQ1, int NBZ>
MFEM_HOST_DEVICE inline void GradYt(const int D1D, const int Q1D,
                                    const double sBG[2][MQ1*MD1],
                                    const double GQ[4][NBZ][MQ1*MQ1],
                                    double GD[4][NBZ][MD1*MQ1])
{
   const int tidz = MFEM_THREAD_ID(z);

   double (*Bt)[MQ1] = (double (*)[MQ1]) (sBG+0);
   double (*Gt)[MQ1] = (double (*)[MQ1]) (sBG+1);

   double (*DQxB)[MQ1] = (double (*)[MQ1])(GD[0] + tidz);
   double (*DQxG)[MQ1] = (double (*)[MQ1])(GD[1] + tidz);
   double (*DQyB)[MQ1] = (double (*)[MQ1])(GD[2] + tidz);
   double (*DQyG)[MQ1] = (double (*)[MQ1])(GD[3] + tidz);

   double (*QQx0)[MQ1] = (double (*)[MQ1])(GQ[0] + tidz);
   double (*QQx1)[MQ1] = (double (*)[MQ1])(GQ[1] + tidz);
   double (*QQy0)[MQ1] = (double (*)[MQ1])(GQ[2] + tidz);
   double (*QQy1)[MQ1] = (double (*)[MQ1])(GQ[3] + tidz);

   MFEM_FOREACH_THREAD(qy,y,Q1D)
   {
      MFEM_FOREACH_THREAD(dx,x,D1D)
      {
         double u[2] = {0.0, 0.0};
         double v[2] = {0.0, 0.0};
         for (int qx = 0; qx < Q1D; ++qx)
         {
            u[0] += Gt[dx][qx] * QQx0[qy][qx];
            u[1] += Gt[dx][qx] * QQy0[qy][qx];

            v[0] += Bt[dx][qx] * QQx1[qy][qx];
            v[1] += Bt[dx][qx] * QQy1[qy][qx];
         }
         DQxB[dx][qy] = u[0];
         DQyB[dx][qy] = u[1];

         DQxG[dx][qy] = v[0];
         DQyG[dx][qy] = v[1];
      }
   }
   MFEM_SYNC_THREAD;
}

/// 2D GradT, stage 2/2
template<int MD1, int MQ1, int NBZ>
MFEM_HOST_DEVICE inline void GradXt(const int D1D, const int Q1D,
                                    const double sBG[2][MQ1*MD1],
                                    const double GD[4][NBZ][MD1*MQ1],
                                    mfem::DeviceTensor<4, double> Y,
                                    const int e)
{
   const int tidz = MFEM_THREAD_ID(z);

   double (*Bt)[MQ1] = (double (*)[MQ1]) (sBG+0);
   double (*Gt)[MQ1] = (double (*)[MQ1]) (sBG+1);

   double (*DQxB)[MQ1] = (double (*)[MQ1])(GD[0] + tidz);
   double (*DQxG)[MQ1] = (double (*)[MQ1])(GD[1] + tidz);
   double (*DQyB)[MQ1] = (double (*)[MQ1])(GD[2] + tidz);
   double (*DQyG)[MQ1] = (double (*)[MQ1])(GD[3] + tidz);

   MFEM_FOREACH_THREAD(dy,y,D1D)
   {
      MFEM_FOREACH_THREAD(dx,x,D1D)
      {
         double u[2] = {0.0, 0.0};
         double v[2] = {0.0, 0.0};
         for (int qy = 0; qy < Q1D; ++qy)
         {
            u[0] += DQxB[dx][qy] * Bt[dy][qy];
            u[1] += DQyB[dx][qy] * Bt[dy][qy];

            v[0] += DQxG[dx][qy] * Gt[dy][qy];
            v[1] += DQyG[dx][qy] * Gt[dy][qy];
         }
         Y(dx,dy,0,e) += u[0] + v[0];
         Y(dx,dy,1,e) += u[1] + v[1];
      }
   }
}

/// Load 3D input vector into shared memory
template<int MD1>
MFEM_HOST_DEVICE inline void LoadX(const int e,
                                   const int D1D,
                                   const DeviceTensor<5, const double> X,
                                   double sm[3][MD1*MD1*MD1])
{
   double (*Xx)[MD1][MD1] = (double (*)[MD1][MD1])(sm+0);
   double (*Xy)[MD1][MD1] = (double (*)[MD1][MD1])(sm+1);
   double (*Xz)[MD1][MD1] = (double (*)[MD1][MD1])(sm+2);
   MFEM_FOREACH_THREAD(dz,z,D1D)
   {
      MFEM_FOREACH_THREAD(dy,y,D1D)
      {
         MFEM_FOREACH_THREAD(dx,x,D1D)
         {
            Xx[dz][dy][dx] = X(dx,dy,dz,0,e);
            Xy[dz][dy][dx] = X(dx,dy,dz,1,e);
            Xz[dz][dy][dx] = X(dx,dy,dz,2,e);
         }
      }
   }
}

/// 3D Grad, stage 1/3
template<int MD1, int MQ1>
MFEM_HOST_DEVICE inline void GradX(const int D1D, const int Q1D,
                                   const double s_BG[2][MQ1*MD1],
                                   const double s_DDD[3][MD1*MD1*MD1],
                                   double s_DDQ[9][MD1*MD1*MQ1])
{
   double (*B)[MD1] = (double (*)[MD1])(s_BG+0);
   double (*G)[MD1] = (double (*)[MD1])(s_BG+1);

   double (*Xx)[MD1][MD1] = (double (*)[MD1][MD1])(s_DDD+0);
   double (*Xy)[MD1][MD1] = (double (*)[MD1][MD1])(s_DDD+1);
   double (*Xz)[MD1][MD1] = (double (*)[MD1][MD1])(s_DDD+2);

   double (*XxB)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ+0);
   double (*XxG)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ+1);
   double (*XyB)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ+2);
   double (*XyG)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ+3);
   double (*XzB)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ+4);
   double (*XzG)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ+5);

   MFEM_FOREACH_THREAD(dz,z,D1D)
   {
      MFEM_FOREACH_THREAD(dy,y,D1D)
      {
         MFEM_FOREACH_THREAD(qx,x,Q1D)
         {
            double u[3] = {0.0, 0.0, 0.0};
            double v[3] = {0.0, 0.0, 0.0};
            for (int dx = 0; dx < D1D; ++dx)
            {
               const double xx = Xx[dz][dy][dx];
               const double xy = Xy[dz][dy][dx];
               const double xz = Xz[dz][dy][dx];
               const double Bx = B[qx][dx];
               const double Gx = G[qx][dx];
               u[0] += Bx * xx;
               u[1] += Bx * xy;
               u[2] += Bx * xz;

               v[0] += Gx * xx;
               v[1] += Gx * xy;
               v[2] += Gx * xz;
            }
            XxB[dz][dy][qx] = u[0];
            XyB[dz][dy][qx] = u[1];
            XzB[dz][dy][qx] = u[2];

            XxG[dz][dy][qx] = v[0];
            XyG[dz][dy][qx] = v[1];
            XzG[dz][dy][qx] = v[2];
         }
      }
   }
   MFEM_SYNC_THREAD;
}

/// 3D Grad, stage 2/3
template<int MD1, int MQ1>
MFEM_HOST_DEVICE inline void GradY(const int D1D, const int Q1D,
                                   const double s_BG[2][MQ1*MD1],
                                   const double s_DDQ[9][MD1*MD1*MQ1],
                                   double s_DQQ[9][MD1*MQ1*MQ1])
{
   double (*B)[MD1] = (double (*)[MD1])(s_BG+0);
   double (*G)[MD1] = (double (*)[MD1])(s_BG+1);

   double (*XxB)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ+0);
   double (*XxG)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ+1);
   double (*XyB)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ+2);
   double (*XyG)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ+3);
   double (*XzB)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ+4);
   double (*XzG)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ+5);

   double (*XxBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+0);
   double (*XxBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+1);
   double (*XxGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+2);
   double (*XyBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+3);
   double (*XyBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+4);
   double (*XyGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+5);
   double (*XzBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+6);
   double (*XzBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+7);
   double (*XzGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+8);

   MFEM_FOREACH_THREAD(dz,z,D1D)
   {
      MFEM_FOREACH_THREAD(qy,y,Q1D)
      {
         MFEM_FOREACH_THREAD(qx,x,Q1D)
         {
            double u[3] = {0.0, 0.0, 0.0};
            double v[3] = {0.0, 0.0, 0.0};
            double w[3] = {0.0, 0.0, 0.0};
            for (int dy = 0; dy < D1D; ++dy)
            {
               const double By = B[qy][dy];
               const double Gy = G[qy][dy];

               u[0] += XxB[dz][dy][qx] * By;
               u[1] += XyB[dz][dy][qx] * By;
               u[2] += XzB[dz][dy][qx] * By;

               v[0] += XxG[dz][dy][qx] * By;
               v[1] += XyG[dz][dy][qx] * By;
               v[2] += XzG[dz][dy][qx] * By;

               w[0] += XxB[dz][dy][qx] * Gy;
               w[1] += XyB[dz][dy][qx] * Gy;
               w[2] += XzB[dz][dy][qx] * Gy;
            }
            XxBB[dz][qy][qx] = u[0];
            XyBB[dz][qy][qx] = u[1];
            XzBB[dz][qy][qx] = u[2];

            XxBG[dz][qy][qx] = v[0];
            XyBG[dz][qy][qx] = v[1];
            XzBG[dz][qy][qx] = v[2];

            XxGB[dz][qy][qx] = w[0];
            XyGB[dz][qy][qx] = w[1];
            XzGB[dz][qy][qx] = w[2];
         }
      }
   }
   MFEM_SYNC_THREAD;
}

/// 3D Grad, stage 3/3
template<int MD1, int MQ1>
MFEM_HOST_DEVICE inline void GradZ(const int D1D, const int Q1D,
                                   const double s_BG[2][MQ1*MD1],
                                   const double s_DQQ[9][MD1*MQ1*MQ1],
                                   double s_QQQ[9][MQ1*MQ1*MQ1])
{
   double (*B)[MD1] = (double (*)[MD1])(s_BG+0);
   double (*G)[MD1] = (double (*)[MD1])(s_BG+1);

   double (*XxBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+0);
   double (*XxBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+1);
   double (*XxGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+2);
   double (*XyBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+3);
   double (*XyBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+4);
   double (*XyGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+5);
   double (*XzBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+6);
   double (*XzBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+7);
   double (*XzGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ+8);

   double (*XxBBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+0);
   double (*XxBGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+1);
   double (*XxGBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+2);
   double (*XyBBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+3);
   double (*XyBGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+4);
   double (*XyGBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+5);
   double (*XzBBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+6);
   double (*XzBGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+7);
   double (*XzGBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+8);

   MFEM_FOREACH_THREAD(qz,z,Q1D)
   {
      MFEM_FOREACH_THREAD(qy,y,Q1D)
      {
         MFEM_FOREACH_THREAD(qx,x,Q1D)
         {
            double u[3] = {0.0, 0.0, 0.0};
            double v[3] = {0.0, 0.0, 0.0};
            double w[3] = {0.0, 0.0, 0.0};
            for (int dz = 0; dz < D1D; ++dz)
            {
               const double Bz = B[qz][dz];
               const double Gz = G[qz][dz];

               u[0] += XxBG[dz][qy][qx] * Bz;
               u[1] += XyBG[dz][qy][qx] * Bz;
               u[2] += XzBG[dz][qy][qx] * Bz;

               v[0] += XxGB[dz][qy][qx] * Bz;
               v[1] += XyGB[dz][qy][qx] * Bz;
               v[2] += XzGB[dz][qy][qx] * Bz;

               w[0] += XxBB[dz][qy][qx] * Gz;
               w[1] += XyBB[dz][qy][qx] * Gz;
               w[2] += XzBB[dz][qy][qx] * Gz;
            }
            XxBBG[qz][qy][qx] = u[0];
            XyBBG[qz][qy][qx] = u[1];
            XzBBG[qz][qy][qx] = u[2];

            XxBGB[qz][qy][qx] = v[0];
            XyBGB[qz][qy][qx] = v[1];
            XzBGB[qz][qy][qx] = v[2];

            XxGBB[qz][qy][qx] = w[0];
            XyGBB[qz][qy][qx] = w[1];
            XzGBB[qz][qy][qx] = w[2];
         }
      }
   }
   MFEM_SYNC_THREAD;
}

/// Pull 3D GradXYZ(X) into Jpr
template<int MQ1>
MFEM_HOST_DEVICE inline
const double *PullGradXYZ(const int qx, const int qy, const int qz,
                          const double s_QQQ[9][MQ1*MQ1*MQ1],
                          double *Jpr)
{
   double (*XxBBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+0);
   double (*XxBGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+1);
   double (*XxGBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+2);
   double (*XyBBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+3);
   double (*XyBGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+4);
   double (*XyGBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+5);
   double (*XzBBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+6);
   double (*XzBGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+7);
   double (*XzGBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+8);

   Jpr[0] = XxBBG[qz][qy][qx];
   Jpr[3] = XxBGB[qz][qy][qx];
   Jpr[6] = XxGBB[qz][qy][qx];
   Jpr[1] = XyBBG[qz][qy][qx];
   Jpr[4] = XyBGB[qz][qy][qx];
   Jpr[7] = XyGBB[qz][qy][qx];
   Jpr[2] = XzBBG[qz][qy][qx];
   Jpr[5] = XzBGB[qz][qy][qx];
   Jpr[8] = XzGBB[qz][qy][qx];

   return Jpr;
}

/// Push 3D GradXYZ(X) to QQQ
template<int MQ1>
MFEM_HOST_DEVICE inline
void PushGradXYZ(const int qx, const int qy, const int qz,
                 const double *A,
                 double s_QQQ[9][MQ1*MQ1*MQ1])
{
   double (*XxBBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+0);
   double (*XxBGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+1);
   double (*XxGBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+2);
   double (*XyBBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+3);
   double (*XyBGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+4);
   double (*XyGBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+5);
   double (*XzBBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+6);
   double (*XzBGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+7);
   double (*XzGBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+8);

   XxBBG[qz][qy][qx] = A[0];
   XxBGB[qz][qy][qx] = A[1];
   XxGBB[qz][qy][qx] = A[2];

   XyBBG[qz][qy][qx] = A[3];
   XyBGB[qz][qy][qx] = A[4];
   XyGBB[qz][qy][qx] = A[5];

   XzBBG[qz][qy][qx] = A[6];
   XzBGB[qz][qy][qx] = A[7];
   XzGBB[qz][qy][qx] = A[8];
}

/// 3D GradT, stage 1/3
template<int MD1, int MQ1>
MFEM_HOST_DEVICE inline void GradZt(const int D1D, const int Q1D,
                                    const double s_BG[2][MQ1*MD1],
                                    const double s_QQQ[9][MQ1*MQ1*MQ1],
                                    double s_DQQ[9][MD1*MQ1*MQ1])
{

   double (*Bt)[MQ1] = (double (*)[MQ1])(s_BG[0]);
   double (*Gt)[MQ1] = (double (*)[MQ1])(s_BG[1]);

   double (*XxBBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+0);
   double (*XxBGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+1);
   double (*XxGBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+2);
   double (*XyBBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+3);
   double (*XyBGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+4);
   double (*XyGBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+5);
   double (*XzBBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+6);
   double (*XzBGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+7);
   double (*XzGBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_QQQ+8);

   double (*XxBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[0]);
   double (*XxBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[1]);
   double (*XxGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[2]);
   double (*XyBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[3]);
   double (*XyBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[4]);
   double (*XyGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[5]);
   double (*XzBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[6]);
   double (*XzBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[7]);
   double (*XzGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[8]);

   MFEM_FOREACH_THREAD(qz,z,Q1D)
   {
      MFEM_FOREACH_THREAD(qy,y,Q1D)
      {
         MFEM_FOREACH_THREAD(dx,x,D1D)
         {
            double u[3] = {0.0, 0.0, 0.0};
            double v[3] = {0.0, 0.0, 0.0};
            double w[3] = {0.0, 0.0, 0.0};
            for (int qx = 0; qx < Q1D; ++qx)
            {
               const double Btx = Bt[dx][qx];
               const double Gtx = Gt[dx][qx];

               u[0] += XxBBG[qz][qy][qx] * Gtx;
               v[0] += XxBGB[qz][qy][qx] * Btx;
               w[0] += XxGBB[qz][qy][qx] * Btx;

               u[1] += XyBBG[qz][qy][qx] * Gtx;
               v[1] += XyBGB[qz][qy][qx] * Btx;
               w[1] += XyGBB[qz][qy][qx] * Btx;

               u[2] += XzBBG[qz][qy][qx] * Gtx;
               v[2] += XzBGB[qz][qy][qx] * Btx;
               w[2] += XzGBB[qz][qy][qx] * Btx;
            }
            XxBB[dx][qy][qz] = u[0];
            XxBG[dx][qy][qz] = v[0];
            XxGB[dx][qy][qz] = w[0];

            XyBB[dx][qy][qz] = u[1];
            XyBG[dx][qy][qz] = v[1];
            XyGB[dx][qy][qz] = w[1];

            XzBB[dx][qy][qz] = u[2];
            XzBG[dx][qy][qz] = v[2];
            XzGB[dx][qy][qz] = w[2];
         }
      }
   }
   MFEM_SYNC_THREAD;
}

/// 3D GradT, stage 2/3
template<int MD1, int MQ1>
MFEM_HOST_DEVICE inline void GradYt(const int D1D, const int Q1D,
                                    const double s_BG[2][MQ1*MD1],
                                    const double s_DQQ[9][MD1*MQ1*MQ1],
                                    double s_DDQ[9][MD1*MD1*MQ1])
{
   double (*Bt)[MQ1] = (double (*)[MQ1])(s_BG[0]);
   double (*Gt)[MQ1] = (double (*)[MQ1])(s_BG[1]);

   double (*XxBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[0]);
   double (*XxBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[1]);
   double (*XxGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[2]);
   double (*XyBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[3]);
   double (*XyBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[4]);
   double (*XyGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[5]);
   double (*XzBB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[6]);
   double (*XzBG)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[7]);
   double (*XzGB)[MQ1][MQ1] = (double (*)[MQ1][MQ1])(s_DQQ[8]);

   double (*XxB)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[0]);
   double (*XxG)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[1]);
   double (*XyB)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[2]);
   double (*XyG)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[3]);
   double (*XzB)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[4]);
   double (*XzG)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[5]);
   double (*XxC)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[6]);
   double (*XyC)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[7]);
   double (*XzC)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[8]);

   MFEM_FOREACH_THREAD(qz,z,Q1D)
   {
      MFEM_FOREACH_THREAD(dy,y,D1D)
      {
         MFEM_FOREACH_THREAD(dx,x,D1D)
         {
            double u[3] = {0.0, 0.0, 0.0};
            double v[3] = {0.0, 0.0, 0.0};
            double w[3] = {0.0, 0.0, 0.0};
            for (int qy = 0; qy < Q1D; ++qy)
            {
               const double Bty = Bt[dy][qy];
               const double Gty = Gt[dy][qy];

               u[0] += XxBB[dx][qy][qz] * Bty;
               v[0] += XxBG[dx][qy][qz] * Gty;
               w[0] += XxGB[dx][qy][qz] * Bty;

               u[1] += XyBB[dx][qy][qz] * Bty;
               v[1] += XyBG[dx][qy][qz] * Gty;
               w[1] += XyGB[dx][qy][qz] * Bty;

               u[2] += XzBB[dx][qy][qz] * Bty;
               v[2] += XzBG[dx][qy][qz] * Gty;
               w[2] += XzGB[dx][qy][qz] * Bty;

            }
            XxB[dx][dy][qz] = u[0];
            XxC[dx][dy][qz] = v[0];
            XxG[dx][dy][qz] = w[0];

            XyB[dx][dy][qz] = u[1];
            XyC[dx][dy][qz] = v[1];
            XyG[dx][dy][qz] = w[1];

            XzB[dx][dy][qz] = u[2];
            XzC[dx][dy][qz] = v[2];
            XzG[dx][dy][qz] = w[2];
         }
      }
   }
   MFEM_SYNC_THREAD;
}

/// 3D GradT, stage 3/3
template<int MD1, int MQ1>
MFEM_HOST_DEVICE inline void GradXt(const int D1D, const int Q1D,
                                    const double s_BG[2][MQ1*MD1],
                                    const double s_DDQ[9][MD1*MD1*MQ1],
                                    DeviceTensor<5, double> Y,
                                    const int e)
{
   double (*Bt)[MQ1] = (double (*)[MQ1])(s_BG[0]);
   double (*Gt)[MQ1] = (double (*)[MQ1])(s_BG[1]);

   double (*XxB)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[0]);
   double (*XxG)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[1]);
   double (*XyB)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[2]);
   double (*XyG)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[3]);
   double (*XzB)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[4]);
   double (*XzG)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[5]);
   double (*XxC)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[6]);
   double (*XyC)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[7]);
   double (*XzC)[MD1][MQ1] = (double (*)[MD1][MQ1])(s_DDQ[8]);

   MFEM_FOREACH_THREAD(dz,z,D1D)
   {
      MFEM_FOREACH_THREAD(dy,y,D1D)
      {
         MFEM_FOREACH_THREAD(dx,x,D1D)
         {
            double u[3] = {0.0, 0.0, 0.0};
            double v[3] = {0.0, 0.0, 0.0};
            double w[3] = {0.0, 0.0, 0.0};
            for (int qz = 0; qz < Q1D; ++qz)
            {
               const double Btz = Bt[dz][qz];
               const double Gtz = Gt[dz][qz];

               u[0] += XxB[dx][dy][qz] * Btz;
               v[0] += XxC[dx][dy][qz] * Btz;
               w[0] += XxG[dx][dy][qz] * Gtz;

               u[1] += XyB[dx][dy][qz] * Btz;
               v[1] += XyC[dx][dy][qz] * Btz;
               w[1] += XyG[dx][dy][qz] * Gtz;

               u[2] += XzB[dx][dy][qz] * Btz;
               v[2] += XzC[dx][dy][qz] * Btz;
               w[2] += XzG[dx][dy][qz] * Gtz;
            }
            Y(dx,dy,dz,0,e) += u[0] + v[0] + w[0];
            Y(dx,dy,dz,1,e) += u[1] + v[1] + w[1];
            Y(dx,dy,dz,2,e) += u[2] + v[2] + w[2];
         }
      }
   }
}

} // namespace kernels

} // namespace mfem

#endif // MFEM_TMOP_PA_HPP