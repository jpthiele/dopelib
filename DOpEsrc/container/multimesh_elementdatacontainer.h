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

#ifndef MULTIMESH_ELEMENTDATACONTAINER_H_
#define MULTIMESH_ELEMENTDATACONTAINER_H_

#include "spacetimehandler.h"
#include "statespacetimehandler.h"
#include "fevalues_wrapper.h"
#include "dopeexception.h"

#include <dofs/dof_handler.h>
#include <hp/dof_handler.h>

using namespace dealii;

namespace DOpE
{

  /**
   * Dummy Template Class, acts as kind of interface. Through template specialization for
   * DOFHANDLER, we
   * distinguish between the 'classic' and the 'hp' case.
   *
   * The Multimesh_ElementDataContainers, can deal with different meshes for the control
   * and state variable as long as both are given as refinements of a common coarse grid
   * by calculation of the respective values on a common refinement.
   *
   * @template DOFHANDLER The type of the dealii-dofhandler we use in our DoPEWrapper::DoFHandler, at the moment DoFHandler and hp::DoFHandler.
   * @template VECTOR     Type of the vector we use in our computations (i.e. Vector<double> or BlockVector<double>)
   * @template dim        The dimension of the integral we are actually interested in.
   */

  template<template<int, int> class DH, typename VECTOR, int dim>
    class Multimesh_ElementDataContainer
    {
      public:
        Multimesh_ElementDataContainer()
        {
          throw(DOpE::DOpEException(
              "Dummy class, this constructor should never get called.",
              "Multimesh_ElementDataContainer<dealii::DoFHandler , VECTOR, dim>::Multimesh_ElementDataContainer"));
        }
        ;
    };

  /**
   * This two classes hold all the information we need in the integrator to
   * integrate something over a element (could be a functional, a PDE, etc.).
   * Of particular importance: This class holds the FEValues objects.
   *
   * @template VECTOR     Type of the vector we use in our computations (i.e. Vector<double> or BlockVector<double>)
   * @template dim        The dimension of the integral we are actually interested in.
   */

  template<typename VECTOR, int dim>
    class Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>
    {

      public:
        /**
         * Constructor. Initializes the FEValues objects.
         *
         * @template FE                   Type of the finite element in use. Must be compatible with dealii::DofHandler. //TODO Should we fix this?
         * @template SPARSITYPATTERN      The corresponding Sparsitypattern to the class-template VECTOR.
         * @template dopedim              The dimension of the control variable.
         * @template dealdim              The dimension of the state variable.
         *
         * @param quad                    Reference to the quadrature-rule which we use at the moment.
         * @param update_flags            The update flags we need to initialize the FEValues obejcts
         * @param sth                     A reference to the SpaceTimeHandler in use.
         * @param element                    A vector of element iterators through which we gain most of the needed information (like
         *                                material_ids, n_dfos, etc.)
         * @param param_values            A std::map containing parameter data (e.g. non space dependent data). If the control
         *                                is done by parameters, it is contained in this map at the position "control".
         * @param domain_values           A std::map containing domain data (e.g. nodal vectors for FE-Functions). If the control
         *                                is distributed, it is contained in this map at the position "control". The state may always
         *                                be found in this map at the position "state"
         *
         */
        template<template<int, int> class FE, typename SPARSITYPATTERN, int dopedim,
            int dealdim>
          Multimesh_ElementDataContainer(
              const Quadrature<dim>& quad,
              UpdateFlags update_flags,
              SpaceTimeHandler<FE, dealii::DoFHandler, SPARSITYPATTERN,
                  VECTOR, dopedim, dealdim>& sth,
              const typename std::vector<typename dealii::DoFHandler<dim>::cell_iterator>& element,
              const typename std::vector<typename dealii::Triangulation<dim>::cell_iterator>& tria_element,
              const std::map<std::string, const Vector<double>*> &param_values,
              const std::map<std::string, const VECTOR*> &domain_values) :
                 _param_values(param_values),
		   _domain_values(domain_values),
		   _element(element),
		   _tria_element(tria_element),
		   _fine_state_fe_values((sth.GetFESystem("state")), quad,
					 update_flags),
		   _fine_control_fe_values((sth.GetFESystem("control")), quad,
					   update_flags)
          {
            _state_index = sth.GetStateIndex();
            if (_state_index == 1)
              _control_index = 0;
            else
              _control_index = 1;
            _n_q_points_per_element = quad.size();
            _n_dofs_per_element = element[0]->get_fe().dofs_per_cell;
	    _control_prolongation = IdentityMatrix(_element[this->GetControlIndex()]->get_fe().dofs_per_cell);
	    _state_prolongation = IdentityMatrix(_element[this->GetStateIndex()]->get_fe().dofs_per_cell);
          }


        ~Multimesh_ElementDataContainer()
        {
        }
        /*********************************************/
        /*
         * This function reinits the FEValues on the actual element. Should
         * be called prior to any of the get-functions.
         */
        inline void
        ReInit(unsigned int coarse_index,unsigned int fine_index, const FullMatrix<double>& prolongation_matrix);

        /*********************************************/
        /**
         * Get functions to extract data. They all assume that ReInit
         * is executed before calling them. Self explanatory.
         */
        inline unsigned int
        GetNDoFsPerElement() const;
        inline unsigned int
        GetNQPoints() const;
        inline unsigned int
        GetMaterialId() const;
        inline unsigned int
        GetNbrMaterialId(unsigned int face) const;
        inline bool
        GetIsAtBoundary() const;
        inline double
        GetElementDiameter() const;
        inline const DOpEWrapper::FEValues<dim>&
        GetFEValuesState() const;
        inline const DOpEWrapper::FEValues<dim>&
        GetFEValuesControl() const;

        /**********************************************/
        /*
         * Looks up the given name in _parameter_data and returns the corresponding value
         * through 'value'.
         */
        void
        GetParamValues(std::string name, Vector<double>& value) const;

        /*********************************************/
        /**
         * Functions to extract values and gradients out of the FEValues
         */

        /*
         * Writes the values of the state variable at the quadrature points into values.
         */
        inline void
        GetValuesState(std::string name, std::vector<double>& values) const;
        /*********************************************/
        /*
         * Same as above for the Vector valued case.
         */
        inline void
        GetValuesState(std::string name, std::vector<Vector<double> >& values) const;

        /*********************************************/
        /*
         * Writes the values of the control variable at the quadrature points into values
         */
        inline void
        GetValuesControl(std::string name, std::vector<double>& values) const;
        /*********************************************/
        /*
         * Same as above for the Vector valued case.
         */
        inline void
        GetValuesControl(std::string name, std::vector<Vector<double> >& values) const;
        /*********************************************/
        /*
         * Writes the values of the state gradient at the quadrature points into values.
         */

        template<int targetdim>
          inline void
              GetGradsState(std::string name, std::vector<Tensor<1, targetdim> >& values) const;

        /*********************************************/
        /*
         * Same as above for the Vector valued case.
         */
        template<int targetdim>
          inline void
          GetGradsState(std::string name,
              std::vector<std::vector<Tensor<1, targetdim> > >& values) const;

        /*********************************************/
        /*
         * Writes the values of the control gradient at the quadrature points into values.
         */
        template<int targetdim>
          inline void
              GetGradsControl(std::string name,
                  std::vector<Tensor<1, targetdim> >& values) const;

        /*********************************************/
        /*
         * Same as above for the Vector valued case.
         */
        template<int targetdim>
          inline void
          GetGradsControl(std::string name,
              std::vector<std::vector<Tensor<1, targetdim> > >& values) const;

      private:
        /*
         * Helper Functions
         */
        unsigned int
        GetStateIndex() const;
        unsigned int
        GetControlIndex() const;
	unsigned int
	  GetCoarseIndex() const { return _coarse_index; }
        unsigned int
	  GetFineIndex() const { return _fine_index; }
        /***********************************************************/
        /**
         * Helper Function. Vector valued case.
         */
        inline void
        GetValues(typename dealii::DoFHandler<dim>::cell_iterator element,
		  const FullMatrix<double>& prolongation,
		  const DOpEWrapper::FEValues<dim>& fe_values, std::string name,
		  std::vector<double>& values) const;
        /***********************************************************/
        /**
         * Helper Function. Vector valued case.
         */
        inline void
        GetValues(typename dealii::DoFHandler<dim>::cell_iterator element,
		  const FullMatrix<double>& prolongation,
		  const DOpEWrapper::FEValues<dim>& fe_values, std::string name,
		  std::vector<Vector<double> >& values) const;
        /***********************************************************/
        /**
         * Helper Function.
         */
        template<int targetdim>
          inline void
          GetGrads(typename dealii::DoFHandler<dim>::cell_iterator element,
		   const FullMatrix<double>& prolongation,
		   const DOpEWrapper::FEValues<dim>& fe_values, std::string name,
		   std::vector<Tensor<1, targetdim> >& values) const;
        /***********************************************************/
        /**
         * Helper Function. Vector valued case.
         */
        template<int targetdim>
          inline void
          GetGrads(typename dealii::DoFHandler<dim>::cell_iterator element,
		   const FullMatrix<double>& prolongation,
		   const DOpEWrapper::FEValues<dim>& fe_values, std::string name,
		   std::vector<std::vector<Tensor<1, targetdim> > >& values) const;
        /***********************************************************/

        inline const std::map<std::string, const VECTOR*> &
        GetDomainValues() const
        {
          return _domain_values;
        }

        /***********************************************************/
        //"global" member data, part of every instantiation
        const std::map<std::string, const Vector<double>*> &_param_values;
        const std::map<std::string, const VECTOR*> &_domain_values;
        unsigned int _state_index;
        unsigned int _control_index;

	const typename std::vector<typename dealii::DoFHandler<dim>::cell_iterator>& _element;
        const typename std::vector<typename dealii::Triangulation<dim>::cell_iterator>& _tria_element;
        DOpEWrapper::FEValues<dim> _fine_state_fe_values;
        DOpEWrapper::FEValues<dim> _fine_control_fe_values;

	FullMatrix<double> _control_prolongation;
	FullMatrix<double> _state_prolongation;
	unsigned int _coarse_index, _fine_index;

        unsigned int _n_q_points_per_element;
        unsigned int _n_dofs_per_element;
    };

  /***********************************************************************/
  /************************IMPLEMENTATION for DoFHandler*********************************/
  /***********************************************************************/

  template<typename VECTOR, int dim>
    void
    DOpE::Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::ReInit(unsigned int coarse_index,unsigned int fine_index, const FullMatrix<double>& prolongation_matrix)
    {
      _coarse_index = coarse_index;
      _fine_index = fine_index;
      assert(this->GetControlIndex() < _element.size());
      if(coarse_index == this->GetStateIndex())
      {
	_state_prolongation = prolongation_matrix;
	_control_prolongation = IdentityMatrix(_element[this->GetControlIndex()]->get_fe().dofs_per_cell);
      }
      else
      {
	if(coarse_index == this->GetControlIndex())
	{
	   _control_prolongation = prolongation_matrix;
	   _state_prolongation = IdentityMatrix(_element[this->GetStateIndex()]->get_fe().dofs_per_cell);
	}
	else
	{
	  _control_prolongation = IdentityMatrix(_element[this->GetControlIndex()]->get_fe().dofs_per_cell);
	  _state_prolongation = IdentityMatrix(_element[this->GetStateIndex()]->get_fe().dofs_per_cell);
	  _fine_index = 0;
	}
      }
      _fine_state_fe_values.reinit(_tria_element[GetFineIndex()]);
      _fine_control_fe_values.reinit(_tria_element[GetFineIndex()]);
    }

  /***********************************************************************/
  template<typename VECTOR, int dim>
    unsigned int
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetNDoFsPerElement() const
    {
      return _n_dofs_per_element;
    }

  /**********************************************/
  template<typename VECTOR, int dim>
    unsigned int
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetNQPoints() const
    {
      return _n_q_points_per_element;
    }

  /**********************************************/
  template<typename VECTOR, int dim>
    unsigned int
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetMaterialId() const
    {
      return _tria_element[GetFineIndex()]->material_id();
    }

  /**********************************************/
  template<typename VECTOR, int dim>
    unsigned int
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetNbrMaterialId(
        unsigned int face) const
    {
      if (_tria_element[GetFineIndex()]->neighbor_index(face) != -1)
        return _tria_element[GetFineIndex()]->neighbor(face)->material_id();
      else
        throw DOpEException("There is no neighbor with number " + face,
            "Multimesh_ElementDataContainer::GetNbrMaterialId");
    }

  /**********************************************/
  template<typename VECTOR, int dim>
    bool
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetIsAtBoundary() const
    {
      return _tria_element[GetFineIndex()]->at_boundary();
    }

  template<typename VECTOR, int dim>
    double
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetElementDiameter() const
    {
      //Note that we return the diameter of the _element[0], which may be the coarse one,
      //But it is the one for which we do the computation
      return _element[0]->diameter();
    }

  /**********************************************/
  template<typename VECTOR, int dim>
    const DOpEWrapper::FEValues<dim>&
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetFEValuesState() const
    {
      return _fine_state_fe_values;
    }

  /**********************************************/
  template<typename VECTOR, int dim>
    const DOpEWrapper::FEValues<dim>&
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetFEValuesControl() const
    {
      return _fine_control_fe_values;
    }

  /**********************************************/

  template<typename VECTOR, int dim>
    void
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetParamValues(
        std::string name, Vector<double>& value) const
    {
      typename std::map<std::string, const Vector<double>*>::const_iterator it =
          _param_values.find(name);
      if (it == _param_values.end())
        {
          throw DOpEException("Did not find " + name,
              "Multimesh_ElementDataContainer::GetParamValues");
        }
      value = *(it->second);
    }

  /*********************************************/
  template<typename VECTOR, int dim>
    void
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetValuesState(
        std::string name, std::vector<double>& values) const
    {
      this->GetValues(_element[this->GetStateIndex()],_state_prolongation,this->GetFEValuesState(), name, values);
    }
  /*********************************************/
  template<typename VECTOR, int dim>
    void
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetValuesState(
        std::string name, std::vector<Vector<double> >& values) const
    {
      this->GetValues(_element[this->GetStateIndex()],_state_prolongation,this->GetFEValuesState(), name, values);

    }

  /*********************************************/
  template<typename VECTOR, int dim>
    void
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetValuesControl(
        std::string name, std::vector<double>& values) const
    {
      this->GetValues(_element[this->GetControlIndex()],_control_prolongation,this->GetFEValuesControl(), name, values);
    }

  /*********************************************/
  template<typename VECTOR, int dim>
    void
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetValuesControl(
        std::string name, std::vector<Vector<double> >& values) const
    {
      this->GetValues(_element[this->GetControlIndex()],_control_prolongation,this->GetFEValuesControl(), name, values);
    }

  /*********************************************/
  template<typename VECTOR, int dim>
    template<int targetdim>
      void
      Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetGradsState(
          std::string name, std::vector<Tensor<1, targetdim> >& values) const
      {
        this->GetGrads<targetdim> (_element[this->GetStateIndex()],_state_prolongation,this->GetFEValuesState(), name, values);
      }

  /*********************************************/
  template<typename VECTOR, int dim>
    template<int targetdim>
      void
      Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetGradsState(
          std::string name, std::vector<std::vector<Tensor<1, targetdim> > >& values) const
      {
        this->GetGrads<targetdim> (_element[this->GetStateIndex()],_state_prolongation,this->GetFEValuesState(), name, values);
      }

  /***********************************************************************/

  template<typename VECTOR, int dim>
    template<int targetdim>
      void
      Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetGradsControl(
          std::string name, std::vector<Tensor<1, targetdim> >& values) const
      {
        this->GetGrads<targetdim> (_element[this->GetControlIndex()],_control_prolongation,this->GetFEValuesControl(), name, values);
      }
  /***********************************************************************/

  template<typename VECTOR, int dim>
    template<int targetdim>
      void
      Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetGradsControl(
          std::string name, std::vector<std::vector<Tensor<1, targetdim> > >& values) const
      {
        this->GetGrads<targetdim> (_element[this->GetControlIndex()],_control_prolongation,this->GetFEValuesControl(), name, values);
      }

  /***********************************************************************/

  template<typename VECTOR, int dim>
    unsigned int
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetStateIndex() const
    {
      return _state_index;
    }

  /***********************************************************************/

  template<typename VECTOR, int dim>
    unsigned int
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetControlIndex() const
    {
      return _control_index;
    }

  /***********************************************************************/
  template<typename VECTOR, int dim>
    void
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetValues(
      typename dealii::DoFHandler<dim>::cell_iterator element,
      const FullMatrix<double>& prolongation,
      const DOpEWrapper::FEValues<dim>& fe_values, std::string name,
      std::vector<double>& values) const
    {
       typename std::map<std::string, const VECTOR*>::const_iterator it =
          this->GetDomainValues().find(name);
      if (it == this->GetDomainValues().end())
        {
          throw DOpEException("Did not find " + name,
              "Multimesh_ElementDataContainer::GetValues");
        }

      unsigned int dofs_per_element = element->get_fe().dofs_per_cell;
      //Now we get the values on the real element
      dealii::Vector<double> dof_values(dofs_per_element);
      dealii::Vector<double> dof_values_transformed(dofs_per_element);
      element->get_dof_values (*(it->second), dof_values);
      //Now compute the real values at the nodal points
      prolongation.vmult(dof_values_transformed,dof_values);

      //Copied from deal FEValuesBase<dim,spacedim>::get_function_values
      // see deal.II/source/fe/fe_values.cc
      unsigned int n_quadrature_points = GetNQPoints();
      std::fill_n (values.begin(), n_quadrature_points, 0);
      for (unsigned int shape_func=0; shape_func<dofs_per_element; ++shape_func)
      {
	const double value = dof_values_transformed(shape_func);
	if (value == 0.)
	continue;

	const double *shape_value_ptr = &(fe_values.shape_value(shape_func, 0));
	for (unsigned int point=0; point<n_quadrature_points; ++point)
	  values[point] += value * *shape_value_ptr++;
      }
    }

  /***********************************************************************/
  template<typename VECTOR, int dim>
    void
    Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetValues(
        typename dealii::DoFHandler<dim>::cell_iterator element,
	const FullMatrix<double>& prolongation,
	const DOpEWrapper::FEValues<dim>& fe_values, std::string name,
        std::vector<Vector<double> >& values) const
    {
      typename std::map<std::string, const VECTOR*>::const_iterator it =
          this->GetDomainValues().find(name);
      if (it == this->GetDomainValues().end())
        {
          throw DOpEException("Did not find " + name,
              "Multimesh_ElementDataContainer::GetValues");
        }

      unsigned int dofs_per_element = element->get_fe().dofs_per_cell;
      //Now we get the values on the real element
      dealii::Vector<double> dof_values(dofs_per_element);
      dealii::Vector<double> dof_values_transformed(dofs_per_element);
      element->get_dof_values (*(it->second), dof_values);
      //Now compute the real values at the nodal points
      prolongation.vmult(dof_values_transformed,dof_values);

      //Copied from deal FEValuesBase<dim,spacedim>::get_function_values
      // see deal.II/source/fe/fe_values.cc
      const unsigned int n_components = element->get_fe().n_components();
      unsigned int n_quadrature_points = GetNQPoints();
      for (unsigned i=0;i<values.size();++i)
	std::fill_n (values[i].begin(), values[i].size(), 0);

      for (unsigned int shape_func=0; shape_func<dofs_per_element; ++shape_func)
      {
	const double value = dof_values_transformed(shape_func);
	if (value == 0.)
	  continue;

	if (element->get_fe().is_primitive(shape_func))
	{
	  const unsigned int comp = element->get_fe().system_to_component_index(shape_func).first;
	  for (unsigned int point=0; point<n_quadrature_points; ++point)
	    values[point](comp) += value * fe_values.shape_value(shape_func,point);
	}
	else
	  for (unsigned int c=0; c<n_components; ++c)
	  {
	    if (element->get_fe().get_nonzero_components(shape_func)[c] == false)
	      continue;
	    for (unsigned int point=0; point<n_quadrature_points; ++point)
	      values[point](c) += value * fe_values.shape_value_component(shape_func,point,c);
	  }
      }
    }

  /***********************************************************************/

  template<typename VECTOR, int dim>
    template<int targetdim>
      void
      Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetGrads(
          typename dealii::DoFHandler<dim>::cell_iterator element,
	  const FullMatrix<double>& prolongation,
	  const DOpEWrapper::FEValues<dim>& fe_values, std::string name,
          std::vector<Tensor<1, targetdim> >& values) const
      {
       typename std::map<std::string, const VECTOR*>::const_iterator it =
            this->GetDomainValues().find(name);
        if (it == this->GetDomainValues().end())
          {
            throw DOpEException("Did not find " + name,
                "Multimesh_ElementDataContainerBase::GetGrads");
          }

	unsigned int dofs_per_element = element->get_fe().dofs_per_cell;
	//Now we get the values on the real element
	dealii::Vector<double> dof_values(dofs_per_element);
	dealii::Vector<double> dof_values_transformed(dofs_per_element);
	element->get_dof_values (*(it->second), dof_values);
	//Now compute the real values at the nodal points
	prolongation.vmult(dof_values_transformed,dof_values);

        //Copied from deal FEValuesBase<dim,spacedim>::get_function_gradients
	unsigned int n_quadrature_points = GetNQPoints();
	std::fill_n (values.begin(), n_quadrature_points, Tensor<1,targetdim>());

	for (unsigned int shape_func=0; shape_func<dofs_per_element; ++shape_func)
	{
	  const double value = dof_values_transformed(shape_func);
	  if (value == 0.)
	    continue;

	  const Tensor<1,targetdim> *shape_gradient_ptr
	    = &(fe_values.shape_grad(shape_func,0));
	  for (unsigned int point=0; point<n_quadrature_points; ++point)
	    values[point] += value * *shape_gradient_ptr++;
	}

      }

  /***********************************************************************/

  template<typename VECTOR, int dim>
    template<int targetdim>
      void
      Multimesh_ElementDataContainer<dealii::DoFHandler, VECTOR, dim>::GetGrads(
          typename dealii::DoFHandler<dim>::cell_iterator element,
	  const FullMatrix<double>& prolongation,
	  const DOpEWrapper::FEValues<dim>& fe_values, std::string name,
          std::vector<std::vector<Tensor<1, targetdim> > >& values) const
      {
        typename std::map<std::string, const VECTOR*>::const_iterator it =
	  this->GetDomainValues().find(name);
        if (it == this->GetDomainValues().end())
	{
	  throw DOpEException("Did not find " + name,
			      "Multimesh_ElementDataContainerBase::GetGrads");
	}

	unsigned int dofs_per_element = element->get_fe().dofs_per_cell;
	//Now we get the values on the real element
	dealii::Vector<double> dof_values(dofs_per_element);
	dealii::Vector<double> dof_values_transformed(dofs_per_element);
	element->get_dof_values (*(it->second), dof_values);
	//Now compute the real values at the nodal points
	prolongation.vmult(dof_values_transformed,dof_values);

        //Copied from deal FEValuesBase<dim,spacedim>::get_function_gradients
	const unsigned int n_components = element->get_fe().n_components();
	unsigned int n_quadrature_points = GetNQPoints();
	for (unsigned i=0;i<values.size();++i)
	  std::fill_n (values[i].begin(), values[i].size(), Tensor<1,dim>());

	for (unsigned int shape_func=0; shape_func<dofs_per_element; ++shape_func)
	{
	  const double value = dof_values_transformed(shape_func);
	  if (value == 0.)
	    continue;

	  if (element->get_fe().is_primitive(shape_func))
	  {
	    const unsigned int comp = element->get_fe().system_to_component_index(shape_func).first;
	    for (unsigned int point=0; point<n_quadrature_points; ++point)
	      values[point][comp] += value * fe_values.shape_grad(shape_func,point);
	  }
	  else
	    for (unsigned int c=0; c<n_components; ++c)
	    {
	      if (element->get_fe().get_nonzero_components(shape_func)[c] == false)
		continue;
	      for (unsigned int point=0; point<n_quadrature_points; ++point)
		values[point][c] += value * fe_values.shape_grad_component(shape_func,point,c);
	    }
	}
      }

  /***********************************************************************/
  /************************END*OF*IMPLEMENTATION**************************/
  /***********************************************************************/
}//end of namespace

#endif /* MULTIMESH_ELEMENTDATACONTAINER_H_ */