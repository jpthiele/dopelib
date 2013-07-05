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

#ifndef _LOCALPDE_
#define _LOCALPDE_

#include "pdeinterface.h"
#include "celldatacontainer.h"
#include "facedatacontainer.h"


using namespace std;
using namespace dealii;
using namespace DOpE;

template<template<int, int> class DH,typename VECTOR,  int dealdim>
  class LocalPDE : public PDEInterface<CellDataContainer,FaceDataContainer,DH, VECTOR,dealdim>
  {
  public:

     static void declare_params(ParameterReader &param_reader)
    {
      param_reader.SetSubsection("Local PDE parameters");

      param_reader.declare_entry("m_biot", "1.0",
				 Patterns::Double(0));
      param_reader.declare_entry("alpha_biot", "1.0",
				 Patterns::Double(0));
      param_reader.declare_entry("viscosity_biot", "1.0",
				 Patterns::Double(0));
      param_reader.declare_entry("K_biot", "1.0",
				 Patterns::Double(0));
      param_reader.declare_entry("density_biot", "1.0",
				 Patterns::Double(0));
      param_reader.declare_entry("volume_source", "1.0",
				 Patterns::Double(0));

     param_reader.declare_entry("density_structure", "1.0",
				 Patterns::Double(0));
     param_reader.declare_entry("mu", "1.0",
				Patterns::Double(0));
     param_reader.declare_entry("poisson_ratio_nu", "0.4",
				Patterns::Double(0));
    }

  LocalPDE(ParameterReader &param_reader) : _state_block_components(3,0)
      {
	_alpha = 1.e-3;
	_state_block_components[2]= 1;

	param_reader.SetSubsection("Local PDE parameters");
	M_biot = param_reader.get_double ("m_biot");
	c_biot = 1.0/M_biot;

	alpha_biot = param_reader.get_double ("alpha_biot");
	viscosity_biot = param_reader.get_double ("viscosity_biot");
	K_biot = param_reader.get_double ("K_biot");
	density_biot = param_reader.get_double ("density_biot");
	volume_source = param_reader.get_double ("volume_source");

	density_structure = param_reader.get_double ("density_structure");
	lame_coefficient_mu = param_reader.get_double ("mu");
	poisson_ratio_nu = param_reader.get_double ("poisson_ratio_nu");

	lame_coefficient_lambda =  (2 * poisson_ratio_nu * lame_coefficient_mu)/
	  (1.0 - 2 * poisson_ratio_nu);

	traction_x_biot = 0.0;
	traction_y_biot = -1.0e+7;



      }

     bool HasFaces() const
     {
       return true;
     }

     // The part of CellEquation scaled by scale contains all "normal" terms which
     // can be treated by full "theta" time-discretization
     void CellEquation(const CellDataContainer<DH, VECTOR, dealdim>& cdc,
				   dealii::Vector<double> &local_cell_vector,
				   double scale, double scale_ico)
    {
      assert(this->_problem_type == "state");

      const DOpEWrapper::FEValues<dealdim> & state_fe_values = cdc.GetFEValuesState();
      unsigned int n_dofs_per_cell = cdc.GetNDoFsPerCell();
      unsigned int n_q_points = cdc.GetNQPoints();
      unsigned int material_id = cdc.GetMaterialId();
      double cell_diameter = cdc.GetCellDiameter();

      // old Newton step solution values and gradients
      _uvalues.resize(n_q_points,Vector<double>(3));
      _ugrads.resize(n_q_points,vector<Tensor<1,2> >(3));

      cdc.GetValuesState("last_newton_solution",_uvalues);
      cdc.GetGradsState("last_newton_solution",_ugrads);

      // old timestep solution values and gradients
      _last_timestep_uvalues.resize(n_q_points,Vector<double>(3));
      _last_timestep_ugrads.resize(n_q_points,vector<Tensor<1,2> >(3));

      cdc.GetValuesState("last_time_solution",_last_timestep_uvalues);
      cdc.GetGradsState("last_time_solution",_last_timestep_ugrads);

      const FEValuesExtractors::Vector displacements (0);
      const FEValuesExtractors::Scalar pressure (2);

      Tensor<2,dealdim> Identity;
      Identity[0][0] = 1.0;
      Identity[0][1] = 0.0;
      Identity[1][0] = 0.0;
      Identity[1][1] = 1.0;


      // pay-zone
      if (material_id == 1)
	{
	  for (unsigned int q=0;q<n_q_points;q++)
	    {
	      	Tensor<2,dealdim> fluid_pressure;
		fluid_pressure.clear();
		fluid_pressure[0][0] = _uvalues[q](2);
		fluid_pressure[1][1] = _uvalues[q](2);

		Tensor<1,dealdim> grad_p;     
		grad_p[0] =  _ugrads[q][2][0];
		grad_p[1] =  _ugrads[q][2][1];

		Tensor<1,dealdim> u;
		u[0] = _uvalues[q](0);
		u[1] = _uvalues[q](1);

		double p = _uvalues[q](2);
		
		Tensor<2,dealdim> grad_u;      
		grad_u[0][0] =  _ugrads[q][0][0];
		grad_u[0][1] =  _ugrads[q][0][1];
		grad_u[1][0] =  _ugrads[q][1][0];
		grad_u[1][1] =  _ugrads[q][1][1];

		const double divergence_u = _ugrads[q][0][0] +  _ugrads[q][1][1];
		const double old_timestep_divergence_u = 
		  _last_timestep_ugrads[q][0][0] +  _last_timestep_ugrads[q][1][1];
		
		double q_biot = volume_source;
		
		Tensor<2,dealdim> E = 0.5 * (grad_u + transpose(grad_u));
		
		Tensor<2,dealdim> sigma_s = 2.0 * lame_coefficient_mu * E  
		  + lame_coefficient_lambda * trace(E) * Identity;
		
	      for (unsigned int i=0;i<n_dofs_per_cell;i++)
		{
		  // Fluid, explicit
		  const Tensor<2,dealdim> phi_i_grads_u = state_fe_values[displacements].gradient (i, q);
                  const double phi_i_p = state_fe_values[pressure].value(i,q);
		  const Tensor<1,dealdim> phi_i_grads_p = state_fe_values[pressure].gradient (i, q); 

		  local_cell_vector(i) += scale_ico * 
		    (scalar_product(sigma_s,phi_i_grads_u) 
		     + alpha_biot * scalar_product(-fluid_pressure, phi_i_grads_u)
		     )
		    * state_fe_values.JxW(q);


		  local_cell_vector(i) += scale * 
		    (1.0/viscosity_biot  
		     * K_biot * grad_p * phi_i_grads_p
		     - q_biot * phi_i_p
		     // Right hand side values
//		     - density_biot * 1.0/viscosity_biot 
//		     * K_biot * gravity * phi_i_grads_p
		     )
		    * state_fe_values.JxW(q);



		}
	    }
	}
      // non pay zone
      else if (material_id == 0)
	{
	  for (unsigned int q=0;q<n_q_points;q++)
	    {
	      double p = _uvalues[q](2);

	      Tensor<1,dealdim> grad_p;     
	      grad_p[0] =  _ugrads[q][2][0];
	      grad_p[1] =  _ugrads[q][2][1];

	      Tensor<2,dealdim> grad_u;      
	      grad_u[0][0] =  _ugrads[q][0][0];
	      grad_u[0][1] =  _ugrads[q][0][1];
	      grad_u[1][0] =  _ugrads[q][1][0];
	      grad_u[1][1] =  _ugrads[q][1][1];

	      Tensor<2,dealdim> E = 0.5 * (grad_u + transpose(grad_u));
	      Tensor<2,dealdim> sigma_s = 2.0 * lame_coefficient_mu * E  
		+ lame_coefficient_lambda * trace(E) * Identity;

	      for (unsigned int i = 0; i < n_dofs_per_cell; i++)
                {
                  // Structure, STVK, explicit
                  const Tensor<2, dealdim> phi_i_grads_u =
                      state_fe_values[displacements].gradient(i, q);

		  const Tensor<1,dealdim> phi_i_grads_p = 
		    state_fe_values[pressure].gradient (i, q);

		  const double phi_i_p = state_fe_values[pressure].value(i,q); 

                  local_cell_vector(i) += scale_ico * 
		    scalar_product(sigma_s,phi_i_grads_u) *
		    state_fe_values.JxW(q);

		}
	    }
	}
    }

    void CellMatrix(const CellDataContainer<DH, VECTOR, dealdim>& cdc,
		    FullMatrix<double> &local_entry_matrix, double scale, double scale_ico)
    {
      const DOpEWrapper::FEValues<dealdim> & state_fe_values = cdc.GetFEValuesState();
      unsigned int n_dofs_per_cell = cdc.GetNDoFsPerCell();
      unsigned int n_q_points = cdc.GetNQPoints();
      unsigned int material_id = cdc.GetMaterialId();
      double cell_diameter = cdc.GetCellDiameter();
      
      // old Newton step solution values and gradients
      _uvalues.resize(n_q_points,Vector<double>(3));
      _ugrads.resize(n_q_points,vector<Tensor<1,2> >(3));

      cdc.GetValuesState("last_newton_solution",_uvalues);
      cdc.GetGradsState("last_newton_solution",_ugrads);

      // old timestep solution values and gradients
      _last_timestep_uvalues.resize(n_q_points,Vector<double>(3));
      _last_timestep_ugrads.resize(n_q_points,vector<Tensor<1,2> >(3));

      cdc.GetValuesState("last_time_solution",_last_timestep_uvalues);
      cdc.GetGradsState("last_time_solution",_last_timestep_ugrads);

      const FEValuesExtractors::Vector displacements (0);
      const FEValuesExtractors::Scalar pressure (2);

      std::vector<Tensor<1,2> >     phi_i_u (n_dofs_per_cell);
      std::vector<Tensor<2,2> >     phi_i_grads_u (n_dofs_per_cell);
      std::vector<double>           phi_i_p (n_dofs_per_cell);
      std::vector<Tensor<1,2> >     phi_i_grads_p (n_dofs_per_cell);

      Tensor<2,dealdim> Identity;
      Identity[0][0] = 1.0;
      Identity[0][1] = 0.0;
      Identity[1][0] = 0.0;
      Identity[1][1] = 1.0;


      // pay zone
      if (material_id == 1)
	{
	  for(unsigned int q = 0; q < n_q_points; q++)
	    {
	      for(unsigned int k = 0; k < n_dofs_per_cell; k++)
		{
		  phi_i_p[k]       = state_fe_values[pressure].value (k, q);
		  phi_i_grads_p[k] = state_fe_values[pressure].gradient (k, q);
		  phi_i_u[k]       = state_fe_values[displacements].value (k, q);
		  phi_i_grads_u[k] = state_fe_values[displacements].gradient (k, q);
		}

	      Tensor<2,dealdim> grad_u;      
	      grad_u[0][0] =  _ugrads[q][0][0];
	      grad_u[0][1] =  _ugrads[q][0][1];
	      grad_u[1][0] =  _ugrads[q][1][0];
	      grad_u[1][1] =  _ugrads[q][1][1];



	      for(unsigned int i = 0; i < n_dofs_per_cell; i++)
		{
		  Tensor<2,dealdim> pI_LinP;
		  pI_LinP.clear();
		  pI_LinP[0][0] = phi_i_p[i];    
		  pI_LinP[1][1] = phi_i_p[i];
		  
		  double divergence_u_LinU = phi_i_grads_u[i][0][0] + phi_i_grads_u[i][1][1];
		  
		  Tensor<2,dealdim> E = 0.5 * (phi_i_grads_u[i] + transpose(phi_i_grads_u[i]));
		  
		  Tensor<2,dealdim> sigma_s = 2.0 * lame_coefficient_mu * E + 
		    lame_coefficient_lambda * trace(E) * Identity;

	 

		  for(unsigned int j=0; j<n_dofs_per_cell; j++)
		    {
                      local_entry_matrix(j, i) += scale_ico
			* (scalar_product(sigma_s,phi_i_grads_u[j])
			   +  alpha_biot * scalar_product(-pI_LinP, phi_i_grads_u[j])) 
			* state_fe_values.JxW(q);
                          

                      local_entry_matrix(j, i) += scale 
			* (1.0/viscosity_biot * K_biot 
			   * phi_i_grads_p[i] * phi_i_grads_p[j])
                          * state_fe_values.JxW(q);



		    }
		}
	    }
	}  // end material_id = 1
      else if (material_id == 0)
	{
	  for(unsigned int q = 0; q  < n_q_points; q++)
	    {
	      for(unsigned int k = 0; k < n_dofs_per_cell; k++)
		{
		  phi_i_u[k]       = state_fe_values[displacements].value (k, q);
		  phi_i_grads_u[k] = state_fe_values[displacements].gradient (k, q);
		  phi_i_p[k]       = state_fe_values[pressure].value (k, q);
		  phi_i_grads_p[k] = state_fe_values[pressure].gradient (k, q);
		}


	      for(unsigned int i=0; i<n_dofs_per_cell; i++)
		{
		  Tensor<2,dealdim> E = 0.5 * (phi_i_grads_u[i] + transpose(phi_i_grads_u[i]));
		  
		  Tensor<2,dealdim> sigma_s = 2.0 * lame_coefficient_mu * E + 
		    lame_coefficient_lambda * trace(E) * Identity;

		  for(unsigned int j=0; j<n_dofs_per_cell; j++)
		    {
		      local_entry_matrix(j, i) += scale_ico
			* scalar_product(sigma_s,phi_i_grads_u[j])
			* state_fe_values.JxW(q);

		    }
		}
	    }
	}  // end material_id = 0
    }


    void CellRightHandSide(const CellDataContainer<DH, VECTOR, dealdim>& /*cdc*/,
                           dealii::Vector<double> &local_cell_vector __attribute__((unused)),
                           double scale __attribute__((unused)))
    {
      assert(this->_problem_type == "state");
    }


    void CellTimeEquation (const CellDataContainer<DH, VECTOR, dealdim>& /*cdc*/,
                           dealii::Vector<double> &local_cell_vector __attribute__((unused)),
				   double scale __attribute__((unused)))
    {
      assert(this->_problem_type == "state");
    }

  void CellTimeEquationExplicit (const CellDataContainer<DH, VECTOR, dealdim>& cdc,
                           dealii::Vector<double> &local_cell_vector,
				   double scale)
    {
      assert(this->_problem_type == "state");

      const DOpEWrapper::FEValues<dealdim> & state_fe_values = cdc.GetFEValuesState();
      unsigned int n_dofs_per_cell = cdc.GetNDoFsPerCell();
      unsigned int n_q_points = cdc.GetNQPoints();
      unsigned int material_id = cdc.GetMaterialId(); 

      // old Newton step solution values and gradients
      _uvalues.resize(n_q_points,Vector<double>(3));
      _ugrads.resize(n_q_points,vector<Tensor<1,2> >(3));

      cdc.GetValuesState("last_newton_solution",_uvalues);
      cdc.GetGradsState("last_newton_solution",_ugrads);

      // old timestep solution values and gradients
      _last_timestep_uvalues.resize(n_q_points,Vector<double>(3));
      _last_timestep_ugrads.resize(n_q_points,vector<Tensor<1,2> >(3));

      cdc.GetValuesState("last_time_solution",_last_timestep_uvalues);
      cdc.GetGradsState("last_time_solution",_last_timestep_ugrads);

      const FEValuesExtractors::Vector displacements (0);
      const FEValuesExtractors::Scalar pressure (2);

      if (material_id == 1)
	{
	  for (unsigned int q=0;q <n_q_points;q++)
	    {

	      const double divergence_u = _ugrads[q][0][0] +  _ugrads[q][1][1];
	      const double old_timestep_divergence_u = 
		_last_timestep_ugrads[q][0][0] +  _last_timestep_ugrads[q][1][1];

	      for (unsigned int i=0;i<n_dofs_per_cell;i++)
		{
		  // Fluid, CellTimeEquation, explicit
		  const double phi_i_p = state_fe_values[pressure].value(i,q);

		  local_cell_vector(i) +=  scale * 
		    (c_biot * (_uvalues[q](2) 
			       - _last_timestep_uvalues[q](2)) * phi_i_p +
		     alpha_biot * (divergence_u - old_timestep_divergence_u) * phi_i_p
		     ) * state_fe_values.JxW(q);


		}
	    }
	}
     }


  void CellTimeMatrix(const CellDataContainer<DH, VECTOR, dealdim>& /*cdc*/,
			FullMatrix<double> &/*local_entry_matrix*/)
    {
      assert(this->_problem_type == "state");
    }

    void CellTimeMatrixExplicit(const CellDataContainer<DH, VECTOR, dealdim>& cdc,
				FullMatrix<double> &local_entry_matrix)
    {
      assert(this->_problem_type == "state");

      const DOpEWrapper::FEValues<dealdim> & state_fe_values = cdc.GetFEValuesState();
      unsigned int n_dofs_per_cell = cdc.GetNDoFsPerCell();
      unsigned int n_q_points = cdc.GetNQPoints();
      unsigned int material_id = cdc.GetMaterialId(); 
      //double cell_diameter = cdc.GetCellDiameter();


      // old Newton step solution values and gradients
      _uvalues.resize(n_q_points,Vector<double>(3));
      _ugrads.resize(n_q_points,vector<Tensor<1,2> >(3));

      cdc.GetValuesState("last_newton_solution",_uvalues);
      cdc.GetGradsState("last_newton_solution",_ugrads);

      // old timestep solution values and gradients
      _last_timestep_uvalues.resize(n_q_points,Vector<double>(3));
      _last_timestep_ugrads.resize(n_q_points,vector<Tensor<1,2> >(3));

      cdc.GetValuesState("last_time_solution",_last_timestep_uvalues);
      cdc.GetGradsState("last_time_solution",_last_timestep_ugrads);

      const FEValuesExtractors::Vector displacements (0);
      const FEValuesExtractors::Scalar pressure (2);
 
      std::vector<double>   phi_i_p (n_dofs_per_cell);
      std::vector<Tensor<2,2> >     phi_i_grads_u (n_dofs_per_cell);

      if (material_id == 1)
	{
	  for(unsigned int q = 0; q  < n_q_points; q++)
	    {
	      for(unsigned int k = 0; k < n_dofs_per_cell; k++)
		{
		  phi_i_p[k]       = state_fe_values[pressure].value (k, q);
		  phi_i_grads_u[k] = state_fe_values[displacements].gradient (k, q);
		}


	      for(unsigned int i = 0; i < n_dofs_per_cell; i++)
		{
		  double divergence_u_LinU = phi_i_grads_u[i][0][0] + phi_i_grads_u[i][1][1];
		  for(unsigned int j = 0; j < n_dofs_per_cell; j++)
		    {
		      local_entry_matrix(j,i) += 
			(c_biot * phi_i_p[i] * phi_i_p[j] + 
			 alpha_biot * divergence_u_LinU * phi_i_p[j] 
			 ) * state_fe_values.JxW(q);

		    }
		}
	    }
	}
 

    }


    // Values for boundary integrals
    void BoundaryEquation (const FaceDataContainer<DH, VECTOR, dealdim>& fdc,
			   dealii::Vector<double> &local_cell_vector,
			   double scale, double scale_ico)
    {

      assert(this->_problem_type == "state");

      const auto & state_fe_face_values = fdc.GetFEFaceValuesState();
      unsigned int n_dofs_per_cell = fdc.GetNDoFsPerCell();
      unsigned int n_q_points = fdc.GetNQPoints();
      unsigned int color = fdc.GetBoundaryIndicator();

      // Top boundary 
      if (color == 3)
	{
	  // old Newton step face_solution values and gradients
	  _ufacevalues.resize(n_q_points,Vector<double>(3));
	  _ufacegrads.resize(n_q_points,vector<Tensor<1,2> >(3));

	  fdc.GetFaceValuesState("last_newton_solution",_ufacevalues);
	  fdc.GetFaceGradsState("last_newton_solution",_ufacegrads);

	  const FEValuesExtractors::Vector displacements (0);

	  for (unsigned int q=0;q<n_q_points;q++)
	    {
	      Tensor<1,dealdim> neumann_value;
	      neumann_value[0] = traction_x_biot;
	      neumann_value[1] = traction_y_biot;

	      for (unsigned int i=0;i<n_dofs_per_cell;i++)
		{
		  const Tensor<1,2> phi_i_u = state_fe_face_values[displacements].value (i, q);

		  local_cell_vector(i) -= 1.0 * scale_ico * neumann_value * phi_i_u  * state_fe_face_values.JxW(q);
		}
	    }
	}

    }

    void BoundaryMatrix (const FaceDataContainer<DH, VECTOR, dealdim>& fdc,
			 dealii::FullMatrix<double> &local_entry_matrix,
			 double scale, double scale_ico)
    {
      assert(this->_problem_type == "state");

    }

    void BoundaryRightHandSide (const FaceDataContainer<DH, VECTOR, dealdim>& /*fdc*/,
				dealii::Vector<double> &/*local_cell_vector*/,
				double /*scale*/)
     {
       	assert(this->_problem_type == "state");
     }



    // Values for boundary integrals
    void FaceEquation (const FaceDataContainer<DH, VECTOR, dealdim>& fdc,
			   dealii::Vector<double> &local_cell_vector,
			   double scale, double scale_ico)
    {

      assert(this->_problem_type == "state");

      const auto & state_fe_face_values = fdc.GetFEFaceValuesState();
      unsigned int n_dofs_per_cell = fdc.GetNDoFsPerCell();
      unsigned int n_q_points = fdc.GetNQPoints();
      unsigned int material_id = fdc.GetMaterialId();
      unsigned int material_id_neighbor = fdc.GetNbrMaterialId();
      bool at_boundary = fdc.GetIsAtBoundary();

      // Top boundary 
      if (material_id == 1)
          if ((material_id != material_id_neighbor) && (!at_boundary))
	    {
	  // old Newton step face_solution values and gradients
	  _ufacevalues.resize(n_q_points,Vector<double>(3));
	  _ufacegrads.resize(n_q_points,vector<Tensor<1,2> >(3));

	  fdc.GetFaceValuesState("last_newton_solution",_ufacevalues);
	  fdc.GetFaceGradsState("last_newton_solution",_ufacegrads);

	  // old timestep solution values and gradients
	  _last_timestep_ufacevalues.resize(n_q_points,Vector<double>(3));
	  _last_timestep_ufacegrads.resize(n_q_points,vector<Tensor<1,2> >(3));
	  
	  fdc.GetFaceValuesState("last_time_solution",_last_timestep_ufacevalues);
	  fdc.GetFaceGradsState("last_time_solution",_last_timestep_ufacegrads);
	  

	  const FEValuesExtractors::Vector displacements (0);

	  for (unsigned int q=0;q<n_q_points;q++)
	    {
	      double fluid_pressure = _last_timestep_ufacevalues[q](2);


	      for (unsigned int i=0;i<n_dofs_per_cell;i++)
		{
		  const Tensor<1,2> phi_i_u = state_fe_face_values[displacements].value (i, q);

		  local_cell_vector(i) += 1.0 * scale_ico * 
		    alpha_biot * fluid_pressure * state_fe_face_values.normal_vector(q) * phi_i_u 

* state_fe_face_values.JxW(q);
		}
	    }
	}



    }

    void FaceMatrix (const FaceDataContainer<DH, VECTOR, dealdim>& fdc,
			 dealii::FullMatrix<double> &local_entry_matrix,
			 double scale, double scale_ico)
    {
      assert(this->_problem_type == "state");


    }

    void FaceRightHandSide (const FaceDataContainer<DH, VECTOR, dealdim>& /*fdc*/,
				dealii::Vector<double> &/*local_cell_vector*/,
				double /*scale*/)
     {
       	assert(this->_problem_type == "state");
     }





    UpdateFlags GetUpdateFlags() const
    {
      if((this->_problem_type == "adjoint") || (this->_problem_type == "state")
	 || (this->_problem_type == "tangent")|| (this->_problem_type == "adjoint_hessian")||(this->_problem_type == "hessian"))
	return update_values | update_gradients  | update_quadrature_points;
      else if((this->_problem_type == "gradient"))
	return update_values | update_quadrature_points;
      else
	throw DOpEException("Unknown Problem Type "+this->_problem_type ,"LocalPDE::GetUpdateFlags");
    }

    UpdateFlags GetFaceUpdateFlags() const
    {
      if((this->_problem_type == "adjoint") || (this->_problem_type == "state")
	 || (this->_problem_type == "tangent")|| (this->_problem_type == "adjoint_hessian")||(this->_problem_type == "hessian"))
	return update_values | update_gradients  | update_normal_vectors | update_quadrature_points;
      else if((this->_problem_type == "gradient"))
	return update_values | update_quadrature_points;
      else
	throw DOpEException("Unknown Problem Type "+this->_problem_type ,"LocalPDE::GetUpdateFlags");
    }




    unsigned int GetControlNBlocks() const
    { return 1;}

    unsigned int GetStateNBlocks() const
    {
      return 2;
    }

    std::vector<unsigned int>& GetControlBlockComponent(){ return _block_components; }
    const std::vector<unsigned int>& GetControlBlockComponent() const{ return _block_components; }
    std::vector<unsigned int>& GetStateBlockComponent(){ return _state_block_components; }
    const std::vector<unsigned int>& GetStateBlockComponent() const{ return _state_block_components; }

  private:
    vector<double> _qvalues;
    vector<double> _dqvalues;
    vector<double> _funcgradvalues;
    vector<Vector<double> > _fvalues;
    vector<Vector<double> > _uvalues;

    vector<vector<Tensor<1,dealdim> > > _ugrads;
    vector<double> _zvalues;
    vector<Tensor<1,dealdim> > _zgrads;
    vector<double> _duvalues;
    vector<Tensor<1,dealdim> > _dugrads;
    vector<double> _dzvalues;
    vector<Tensor<1,dealdim> > _dzgrads;

    //last timestep solution values
    vector<Vector<double> > _last_timestep_uvalues;
    vector<vector<Tensor<1,dealdim> > > _last_timestep_ugrads;

    // face values
    vector<double> _qfacevalues;
    vector<Vector<double> > _ffacevalues;
    vector<Vector<double> > _ufacevalues;
    vector<vector<Tensor<1,dealdim> > > _ufacegrads;

    vector<Vector<double> > _last_timestep_ufacevalues;
    vector<vector<Tensor<1,dealdim> > > _last_timestep_ufacegrads;


    vector<unsigned int> _state_block_components;
    vector<unsigned int> _block_components;
    double _alpha, _cell_diameter;

    // material variables
    double density_structure, 
      lame_coefficient_mu, poisson_ratio_nu, lame_coefficient_lambda;

    double M_biot, c_biot, alpha_biot, viscosity_biot, K_biot, density_biot;

    double traction_x_biot, traction_y_biot;

    double volume_source;

  };
#endif
