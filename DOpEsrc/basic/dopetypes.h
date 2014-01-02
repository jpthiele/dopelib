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

#ifndef _DOPETYPES_H_
#define _DOPETYPES_H_

#include <string>

#include "dopeexception.h"

namespace DOpE
{
  namespace DOpEtypes
  {
    //TODO typedef for dealii-boundary-type
    // (see changes after version 7.1 in deal.ii).

    /**
     * This enum describes the different mesh refinement types, see
     * dealii::GridRefinement for more detailded explanation.
     *
     * global           Global refinement
     * fixed_fraction   Local refinement using fixed fraction strategy
     * fixed_number     Local refinement using fixed number of elements strategy
     * optimized        Local refinement using optimized strategy
     * finest_of_both   In the case that one has two grids, refine such that 
     *                  elements are refined if, on the other mesh, the element 
     *                  has been refined. 
     */
    enum RefinementType
    {
      global, fixed_fraction, fixed_number, optimized, finest_of_both
    };


    /**
     * This enum describes which terms of the error identity
     * should get computed:
     *
     * primal_only    Only the primal-residual-term.
     * dual_only      Only the dual-residual-term.
     * mixed          Compute both, scale them by 0.5
     *                 and build the sum.
     * mixed_control  As with mixed but includes control errors
     */
    enum EETerms
    {
      primal_only, dual_only, mixed, mixed_control
    };

    /**
     * This enum describes how we compute the weights in
     * the DWR-method, see for instance
     *
     * Bangerth, Rannacher: Adaptive Finite Element Methods
     * for Differential Equations
     *
     * for the explanation of the different states.
     */
    enum WeightComputation
    {
      element_diameter, higher_order_interpolation, higher_order_computation, constant/*Not implemented!*/
    };

    /**
     * This enum describes how we evaluate the residual
     * in the DWR-method.
     *
     * strong_residual    We use the strong form of the residual.
     */
    enum ResidualEvaluation
    {
      strong_residual, smoothness_and_influence_factors
    };

    /**
     * An enum describing the type of the control.
     *
     * undefined      self explanatory
     * stationary     the control is not time dependent
     * initial        the control acts in the initial conditions
     * nonstationary  the control is timedependent
     */
    enum ControlType
    {
      undefined,
      stationary,
      initial,
      nonstationary
    };
    
    /**
     * An enum that describes the storage behavior of 
     * all Control-, State-, and ConstraintVectors
     *
     * fullmem          Store all data in the main memory (RAM)
     * store_on_disc    Stores all unused timesteps on the harddisc
     * only_recent      Only keep a copy of the most recent timestep
     *                  (Only useful for pure forward runs and 
     *                   Pseudo-Timestepping methods)
     */
    enum VectorStorageType
    {
      fullmem,
      store_on_disc,
      only_recent
    };

  }//End of namespace DOpEtypes


  /**
   * Transfers DOpEtypes::VectorStorageType to Human readable values
   */
  template<typename C>
    static std::string DOpEtypesToString(const C& t)
    {
      throw DOpEException("Not implemented!","DOpEtypesToString");
    }

    template <>
    std::string DOpEtypesToString(const DOpEtypes::VectorStorageType& t)
    {
      if( DOpEtypes::VectorStorageType::fullmem == t )
      {
	return "fullmem";
      }
      else if( DOpEtypes::VectorStorageType::store_on_disc == t )
      {
	return "store_on_disc";
      }
      else if( DOpEtypes::VectorStorageType::only_recent == t )
      {
	return "only_recent";
      }
      else
      {
	std::stringstream out;
	out<<"Unknown DOpEtypes::VectorStorageType"<< std::endl;
	out<<"Code given is "<< t<<std::endl;
	throw DOpEException(out.str(),"DOpEtypesToString<DOpEtypes::VectorStorageType");
      }
    }

}//End of Namespace DOpE

#endif /* DOPETYPES_H_ */
