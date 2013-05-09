/**
 *
 * Copyright (C) 2012 by the DOpElib authors
 *
 * This file is part of DOpElib
 *
 * DOpElib is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later
 * version.
 *
 * DOpElib is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * Please refer to the file LICENSE.TXT included in this distribution
 * for further information on this license.
 *
 **/

/*
 * functionals.h
 *
 *  Created on: Aug 22, 2011
 *      Author: cgoll
 */

#ifndef FUNCTIONALS_H_
#define FUNCTIONALS_H_

#include "functionalinterface.h"

using namespace std;
using namespace dealii;
using namespace DOpE;

/**
 * Flux over the right hand side boundary.
 */

template<
    template<template<int, int> class DH, typename VECTOR, int dealdim> class CDC,
    template<template<int, int> class DH, typename VECTOR, int dealdim> class FDC,
    template<int, int> class DH, typename VECTOR, int dealdim>
  class LocalBoundaryFunctionalMassFlux : public FunctionalInterface<CDC, FDC,
      DH, VECTOR, dealdim>
  {
    public:
      LocalBoundaryFunctionalMassFlux()
      {
        assert(dealdim == 2);
      }

      // compute drag value around cylinder
      double
      BoundaryValue(const FDC<DH, VECTOR, dealdim>& fdc)
      {
        const unsigned int n_q_points = fdc.GetNQPoints();
        const unsigned int color = fdc.GetBoundaryIndicator();

        double mass_flux_stokes = 0;

        if (color == 1)
        {
          vector<Vector<double> > _ufacevalues;

          _ufacevalues.resize(n_q_points, Vector<double>(2));

          fdc.GetFaceValuesState("state", _ufacevalues);

          for (unsigned int q_point = 0; q_point < n_q_points; q_point++)
          {
            Tensor<1, 2> v;
            v.clear();
            v[0] = _ufacevalues[q_point](0);
            v[1] = _ufacevalues[q_point](1);

            mass_flux_stokes += v
                * fdc.GetFEFaceValuesState().normal_vector(q_point)
                * fdc.GetFEFaceValuesState().JxW(q_point);
          }

        }
        return mass_flux_stokes;
      }

      //Achtung, hier kein gradient update
      UpdateFlags
      GetFaceUpdateFlags() const
      {
        return update_values | update_quadrature_points | update_normal_vectors;
      }

      string
      GetType() const
      {
        return "boundary";
      }
      string
      GetName() const
      {
        return "Outflow";
      }

    private:
      int _outflow_fluid_boundary_color;
  };
#endif /* FUNCTIONALS_H_ */
