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

#include "operator.hpp"
#include "../../linalg/vector.hpp"
#include "../fespace.hpp"

namespace mfem
{

void MFEMCeedOperator::Mult(const Vector &x, Vector &y) const
{
   const CeedScalar *x_ptr;
   CeedScalar *y_ptr;
   CeedMemType mem;
   CeedGetPreferredMemType(internal::ceed, &mem);
   if ( Device::Allows(Backend::DEVICE_MASK) && mem==CEED_MEM_DEVICE )
   {
      x_ptr = x.Read();
      y_ptr = y.ReadWrite();
   }
   else
   {
      x_ptr = x.HostRead();
      y_ptr = y.HostReadWrite();
      mem = CEED_MEM_HOST;
   }
   CeedVectorSetArray(u, mem, CEED_USE_POINTER,
                      const_cast<CeedScalar*>(x_ptr));
   CeedVectorSetArray(v, mem, CEED_USE_POINTER, y_ptr);

   CeedOperatorApplyAdd(oper, u, v,
                        CEED_REQUEST_IMMEDIATE);

   CeedVectorTakeArray(u, mem, const_cast<CeedScalar**>(&x_ptr));
   CeedVectorTakeArray(v, mem, &y_ptr);
}

void MFEMCeedOperator::GetDiagonal(Vector &diag) const
{
   CeedScalar *d_ptr;
   CeedMemType mem;
   CeedGetPreferredMemType(internal::ceed, &mem);
   if ( Device::Allows(Backend::DEVICE_MASK) && mem==CEED_MEM_DEVICE )
   {
      d_ptr = diag.ReadWrite();
   }
   else
   {
      d_ptr = diag.HostReadWrite();
      mem = CEED_MEM_HOST;
   }
   CeedVectorSetArray(v, mem, CEED_USE_POINTER, d_ptr);

   CeedOperatorLinearAssembleAddDiagonal(oper, v, CEED_REQUEST_IMMEDIATE);

   CeedVectorTakeArray(v, mem, &d_ptr);
}

} // namespace mfem