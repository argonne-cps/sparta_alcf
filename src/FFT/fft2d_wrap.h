/* ----------------------------------------------------------------------
   SPARTA - Stochastic PArallel Rarefied-gas Time-accurate Analyzer
   http://sparta.github.io
   Steve Plimpton, sjplimp@gmail.com, Michael Gallis, magalli@sandia.gov
   Sandia National Laboratories

   Copyright (2014) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level SPARTA directory.
------------------------------------------------------------------------- */

#ifndef SPARTA_FFT2D_WRAP_H
#define SPARTA_FFT2D_WRAP_H

#include "fft2d.h"
#include "pointers.h"

namespace SPARTA_NS {

class FFT2d : protected Pointers {
 public:
  enum { FORWARD = 1, BACKWARD = -1 };

  FFT2d(class SPARTA *, MPI_Comm, int, int, int, int, int, int, int, int,
        int, int, int, int, int *, int);
  ~FFT2d() override;
  void compute(FFT_SCALAR *, FFT_SCALAR *, int);
  void timing1d(FFT_SCALAR *, int, int);

 private:
  struct fft_plan_2d *plan;
};

}    // namespace SPARTA_NS

#endif

/* ERROR/WARNING messages:

E: Could not create 3d FFT plan

The FFT setup for the PPPM solver failed, typically due
to lack of memory.  This is an unusual error.  Check the
size of the FFT grid you are requesting.

*/
