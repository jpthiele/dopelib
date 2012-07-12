#ifndef _LOCALFunctionalS_
#define _LOCALFunctionalS_

#include "pdeinterface.h"
//#include "functionalinterface.h"

using namespace std;
using namespace dealii;
using namespace DOpE;


/****************************************************************************************/

template<typename VECTOR, int dopedim, int dealdim>
  class LocalPointFunctionalPressure : public FunctionalInterface<CellDataContainer,FaceDataContainer,dealii::DoFHandler<dealdim>, VECTOR, dopedim,dealdim>
  {
  public:
    
  double PointValue(const DOpEWrapper::DoFHandler<dopedim, dealii::DoFHandler<dealdim> > & /*control_dof_handler*/,
		    const DOpEWrapper::DoFHandler<dealdim, dealii::DoFHandler<dealdim> > & state_dof_handler,
		      const std::map<std::string, const dealii::Vector<double>* > &/*param_values*/,
		    const std::map<std::string, const VECTOR* > &domain_values)
  {
    Point<2> p1(0.15,0.2);
    Point<2> p2(0.25,0.2);
    
    typename map<string, const VECTOR* >::const_iterator it = domain_values.find("state");
    Vector<double> tmp_vector(5);
    
    
    VectorTools::point_value (state_dof_handler, *(it->second), p1, tmp_vector); 
    double p1_value = tmp_vector(4);
    tmp_vector = 0;
    VectorTools::point_value (state_dof_handler, *(it->second), p2, tmp_vector);  
    double p2_value = tmp_vector(4);
    
    // pressure analysis
    return (p1_value - p2_value);
    
  }
  
  string GetType() const
  {
    return "point";  
  }
  string GetName() const
  {
    return "Pressure difference";  
  }
  
  };

/****************************************************************************************/
template<typename VECTOR, int dopedim, int dealdim>
  class LocalPointFunctionalDeflectionX : public FunctionalInterface<CellDataContainer,FaceDataContainer,dealii::DoFHandler<dealdim>, VECTOR, dopedim,dealdim>
  {
  public:
    
    double PointValue(const DOpEWrapper::DoFHandler<dopedim, dealii::DoFHandler<dealdim> > & /*control_dof_handler*/,
			      const DOpEWrapper::DoFHandler<dealdim, dealii::DoFHandler<dealdim> > &state_dof_handler,
		      const std::map<std::string, const dealii::Vector<double>* > &/*param_values*/,
			      const std::map<std::string, const VECTOR* > &domain_values)		
  {
      Point<2> p1(0.6,0.2);
       
    typename map<string, const VECTOR* >::const_iterator it = domain_values.find("state");
    Vector<double> tmp_vector(5);
    
    
    VectorTools::point_value (state_dof_handler, *(it->second), p1, tmp_vector); 
    double x = tmp_vector(2);
  
    // Deflection X
    return x;
    
  }
  
  string GetType() const
  {
    return "point";  
  }
  string GetName() const
  {
    return "Deflection_X";  
  }
  
  };

/****************************************************************************************/
template<typename VECTOR, int dopedim, int dealdim>
  class LocalPointFunctionalDeflectionY : public FunctionalInterface<CellDataContainer,FaceDataContainer,dealii::DoFHandler<dealdim>, VECTOR, dopedim,dealdim>
  {
  public:
    
   double PointValue(const DOpEWrapper::DoFHandler<dopedim, dealii::DoFHandler<dealdim> > & /*control_dof_handler*/,
		    const DOpEWrapper::DoFHandler<dealdim, dealii::DoFHandler<dealdim> > &state_dof_handler,
		      const std::map<std::string, const dealii::Vector<double>* > &/*param_values*/,
		    const std::map<std::string, const VECTOR* > &domain_values)      		
  {
    Point<2> p1(0.6,0.2);
     
    typename map<string, const VECTOR* >::const_iterator it = domain_values.find("state");
    Vector<double> tmp_vector(5);
        
    VectorTools::point_value (state_dof_handler, *(it->second), p1, tmp_vector); 
    double y = tmp_vector(3);
  
    // Delfection Y
    return y;
    
  }
  
  string GetType() const
  {
    return "point";  
  }
  string GetName() const
  {
    return "Deflection_Y";   
  }
  
  };

// drag 
/****************************************************************************************/
template<typename VECTOR, int dopedim, int dealdim>
  class LocalBoundaryFaceFunctionalDrag : public FunctionalInterface<CellDataContainer,FaceDataContainer,dealii::DoFHandler<dealdim>, VECTOR, dopedim,dealdim>
  {
  public:    
     static void declare_params(ParameterReader &param_reader)
    {
      param_reader.SetSubsection("Local PDE parameters");
      param_reader.declare_entry("density_fluid", "0.0",
				 Patterns::Double(0));
      param_reader.declare_entry("viscosity", "0.0",
				 Patterns::Double(0));
      param_reader.declare_entry("mu", "0.0",
				 Patterns::Double(0));
      param_reader.declare_entry("poisson_ratio_nu", "0.0",
				 Patterns::Double(0));
      
    }

     LocalBoundaryFaceFunctionalDrag (ParameterReader &param_reader) 
       {
	 param_reader.SetSubsection("Local PDE parameters");
	 _density_fluid = param_reader.get_double ("density_fluid");
	 _viscosity = param_reader.get_double ("viscosity");
	 _lame_coefficient_mu = param_reader.get_double ("mu");
	 _poisson_ratio_nu = param_reader.get_double ("poisson_ratio_nu");
	 
	 _lame_coefficient_lambda =  (2 * _poisson_ratio_nu * _lame_coefficient_mu)/
	   (1.0 - 2 * _poisson_ratio_nu);
       }

     bool HasFaces() const
      {
        return true;
      }
 
     string GetType() const
     {
       return "face boundary"; 
       // boundary face
     }

     // compute drag value around cylinder
    double BoundaryValue(const FaceDataContainer<dealii::DoFHandler<dealdim>, VECTOR, dealdim>& fdc) 
     {
       unsigned int color = fdc.GetBoundaryIndicator();
      const auto &state_fe_face_values = fdc.GetFEFaceValuesState();
      unsigned int n_q_points = fdc.GetNQPoints();

       Tensor<1,2> drag_lift_value;
       drag_lift_value.clear();
       if (color == 80)
	 {
	  vector<Vector<double> > _ufacevalues;
	  vector<vector<Tensor<1,dealdim> > > _ufacegrads;
	  
	  _ufacevalues.resize(n_q_points,Vector<double>(5));
	  _ufacegrads.resize(n_q_points,vector<Tensor<1,2> >(5));    
	  
	  fdc.GetFaceValuesState("state",_ufacevalues);
	  fdc.GetFaceGradsState("state",_ufacegrads);   
	  
	  for (unsigned int q_point=0;q_point<n_q_points;q_point++)
	    { 
	      
	       Tensor<2,2> pI;
	       pI[0][0] = _ufacevalues[q_point](4);
	       pI[1][1] = _ufacevalues[q_point](4);
	      
	       Tensor<1,2> v;
	       v.clear();
	       v[0] = _ufacevalues[q_point](0);
	       v[1] = _ufacevalues[q_point](1);
	    	      
	       Tensor<2,2> grad_v;
	       grad_v[0][0] = _ufacegrads[q_point][0][0];
	       grad_v[0][1] = _ufacegrads[q_point][0][1];
	       grad_v[1][0] = _ufacegrads[q_point][1][0];
	       grad_v[1][1] = _ufacegrads[q_point][1][1];
	      
	       Tensor<2,2> F;
	       F[0][0] = 1.0 +  _ufacegrads[q_point][2][0];
	       F[0][1] = _ufacegrads[q_point][2][1];
	       F[1][0] = _ufacegrads[q_point][3][0];
	       F[1][1] = 1.0 + _ufacegrads[q_point][3][1];
	      
	       Tensor<2,2> F_Inverse;
	       F_Inverse = invert (F);
	       
	       Tensor<2,2> F_T;
	       F_T = transpose (F);
	       
	       Tensor<2,2> F_Inverse_T;
	       F_Inverse_T = transpose (F_Inverse);
	      
	       double J;
	       J = determinant (F);
	      
	      
	       // constitutive stress tensors for fluid 
	       Tensor<2,2> cauchy_stress_fluid;
	       cauchy_stress_fluid = J * (-pI + _density_fluid * _viscosity *
					  (grad_v * F_Inverse + F_Inverse_T * transpose(grad_v))) * F_Inverse_T;

	      
	      
	      drag_lift_value -=  1.0 * cauchy_stress_fluid
		* state_fe_face_values.normal_vector(q_point) *
		state_fe_face_values.JxW(q_point);
	    }	  	 
	 }  
         return drag_lift_value[0]; 
     }

     
     double FaceValue(const FaceDataContainer<dealii::DoFHandler<dealdim>, VECTOR, dealdim>& fdc)  
     {
        const auto & state_fe_face_values = fdc.GetFEFaceValuesState();
      unsigned int n_dofs_per_cell = fdc.GetNDoFsPerCell();
      unsigned int n_q_points = fdc.GetNQPoints();
      unsigned int color = fdc.GetBoundaryIndicator();
      unsigned int material_id = fdc.GetMaterialId();
      unsigned int material_id_neighbor = fdc.GetNbrMaterialId();
      bool at_boundary = fdc.GetIsAtBoundary();
      

    
       Tensor<1,2> drag_lift_value;
       drag_lift_value.clear();
       if (material_id == 0)
	 { 	   
	   if (material_id != material_id_neighbor)	       
	     {
	       vector<Vector<double> > _ufacevalues;
	      vector<vector<Tensor<1,dealdim> > > _ufacegrads;

	      _ufacevalues.resize(n_q_points,Vector<double>(5));
	      _ufacegrads.resize(n_q_points,vector<Tensor<1,2> >(5));

	      fdc.GetFaceValuesState("state",_ufacevalues);
	      fdc.GetFaceGradsState("state",_ufacegrads);
 
	      
	       for (unsigned int q_point=0;q_point<n_q_points;q_point++)
		 { 
		   
		    Tensor<2,2> pI;
		    pI[0][0] = _ufacevalues[q_point](4);
		    pI[1][1] = _ufacevalues[q_point](4);
		    
		    Tensor<1,2> v;
		    v.clear();
		    v[0] = _ufacevalues[q_point](0);
		    v[1] = _ufacevalues[q_point](1);
		    
		    Tensor<2,2> grad_v;
		    grad_v[0][0] = _ufacegrads[q_point][0][0];
		    grad_v[0][1] = _ufacegrads[q_point][0][1];
		    grad_v[1][0] = _ufacegrads[q_point][1][0];
		    grad_v[1][1] = _ufacegrads[q_point][1][1];
		    
		    Tensor<2,2> F;
		    F[0][0] = 1.0 +  _ufacegrads[q_point][2][0];
		    F[0][1] = _ufacegrads[q_point][2][1];
		    F[1][0] = _ufacegrads[q_point][3][0];
		    F[1][1] = 1.0 + _ufacegrads[q_point][3][1];
		    
		    Tensor<2,2> F_Inverse;
		    F_Inverse = invert (F);
		    
		    Tensor<2,2> F_T;
		    F_T = transpose (F);
		    
		    Tensor<2,2> F_Inverse_T;
		    F_Inverse_T = transpose (F_Inverse);
		    
		    double J;
		    J = determinant (F);
		    
		    
		    // constitutive stress tensors for fluid 
		    Tensor<2,2> cauchy_stress_fluid;
		    cauchy_stress_fluid = J * (-pI + _density_fluid * _viscosity *
		    			      (grad_v * F_Inverse + F_Inverse_T * transpose(grad_v))) * F_Inverse_T;
		    
		    
		    
		    drag_lift_value -= 1.0 * cauchy_stress_fluid
		      * state_fe_face_values.normal_vector(q_point) *
		      state_fe_face_values.JxW(q_point);
		 }	  	
	     } 	      
	 }
       return drag_lift_value[0];          
     }
     
     
     UpdateFlags GetFaceUpdateFlags() const
     {
       return update_values | update_quadrature_points | 
	 update_gradients | update_normal_vectors;
     }
     
     string GetName() const
     {
       return "Drag";   
     }
  
 protected:
    inline void GetFaceValues(const DOpEWrapper::FEFaceValues<dealdim>& fe_face_values,
			      const map<string, const VECTOR* >& domain_values, string name,
			      vector<Vector<double> >& values)
    {
      typename map<string, const VECTOR* >::const_iterator it = domain_values.find(name);
      if(it == domain_values.end())
	{
	  throw DOpEException("Did not find " + name,"LocalPDE::GetValues");
	}
      fe_face_values.get_function_values(*(it->second),values);
    }

    inline void GetFaceGrads(const DOpEWrapper::FEFaceValues<dealdim>& fe_face_values,
			     const map<string, const VECTOR* >& domain_values, string name,
			     vector<vector<Tensor<1,dealdim> > >& values)
    {
      typename map<string, const VECTOR* >::const_iterator it = domain_values.find(name);
      if(it == domain_values.end())
	{
	  throw DOpEException("Did not find " + name,"LocalPDE::GetGrads");
	}
      fe_face_values.get_function_grads(*(it->second),values);
    }





  private:  
  double _density_fluid,_viscosity,_lame_coefficient_mu,
    _poisson_ratio_nu, _lame_coefficient_lambda;
    
   


  };


 // lift 
/****************************************************************************************/
template<typename VECTOR, int dopedim, int dealdim>
  class LocalBoundaryFaceFunctionalLift : public FunctionalInterface<CellDataContainer,FaceDataContainer,dealii::DoFHandler<dealdim>, VECTOR, dopedim,dealdim>
  {
  public:    
     static void declare_params(ParameterReader &param_reader)
    {
      param_reader.SetSubsection("Local PDE parameters");
      param_reader.declare_entry("density_fluid", "0.0",
				 Patterns::Double(0));
      param_reader.declare_entry("viscosity", "0.0",
				 Patterns::Double(0));
      param_reader.declare_entry("mu", "0.0",
				 Patterns::Double(0));
      param_reader.declare_entry("poisson_ratio_nu", "0.0",
				 Patterns::Double(0));
      
    }

     LocalBoundaryFaceFunctionalLift (ParameterReader &param_reader) 
       {
	 param_reader.SetSubsection("Local PDE parameters");
	 _density_fluid = param_reader.get_double ("density_fluid");
	 _viscosity = param_reader.get_double ("viscosity");
	 _lame_coefficient_mu = param_reader.get_double ("mu");
	 _poisson_ratio_nu = param_reader.get_double ("poisson_ratio_nu");
	 
	 _lame_coefficient_lambda =  (2 * _poisson_ratio_nu * _lame_coefficient_mu)/
	   (1.0 - 2 * _poisson_ratio_nu);
       }

     bool HasFaces() const
      {
        return true;
      }
 
     string GetType() const
     {
       return "face boundary";  
       // boundary face
     }


     // compute drag value around cylinder
     double BoundaryValue(const FaceDataContainer<dealii::DoFHandler<dealdim>, VECTOR, dealdim>& fdc) 
     {
       unsigned int color = fdc.GetBoundaryIndicator();
      const auto &state_fe_face_values = fdc.GetFEFaceValuesState();
      unsigned int n_q_points = fdc.GetNQPoints();


       Tensor<1,2> drag_lift_value;
       drag_lift_value.clear();
       if (color == 80)
	 {
	  vector<Vector<double> > _ufacevalues;
	  vector<vector<Tensor<1,dealdim> > > _ufacegrads;
	  
	  _ufacevalues.resize(n_q_points,Vector<double>(5));
	  _ufacegrads.resize(n_q_points,vector<Tensor<1,2> >(5));    
	  
	  fdc.GetFaceValuesState("state",_ufacevalues);
	  fdc.GetFaceGradsState("state",_ufacegrads);   
	  
	  for (unsigned int q_point=0;q_point<n_q_points;q_point++)
	    { 
	       Tensor<2,2> pI;
	       pI[0][0] = _ufacevalues[q_point](4);
	       pI[1][1] = _ufacevalues[q_point](4);
	      
	       Tensor<1,2> v;
	       v.clear();
	       v[0] = _ufacevalues[q_point](0);
	       v[1] = _ufacevalues[q_point](1);
	    	      
	       Tensor<2,2> grad_v;
	       grad_v[0][0] = _ufacegrads[q_point][0][0];
	       grad_v[0][1] = _ufacegrads[q_point][0][1];
	       grad_v[1][0] = _ufacegrads[q_point][1][0];
	       grad_v[1][1] = _ufacegrads[q_point][1][1];
	      
	       Tensor<2,2> F;
	       F[0][0] = 1.0 +  _ufacegrads[q_point][2][0];
	       F[0][1] = _ufacegrads[q_point][2][1];
	       F[1][0] = _ufacegrads[q_point][3][0];
	       F[1][1] = 1.0 + _ufacegrads[q_point][3][1];
	      
	       Tensor<2,2> F_Inverse;
	       F_Inverse = invert (F);
	       
	       Tensor<2,2> F_T;
	       F_T = transpose (F);
	       
	       Tensor<2,2> F_Inverse_T;
	       F_Inverse_T = transpose (F_Inverse);
	      
	       double J;
	       J = determinant (F);
	      
	      
	       // constitutive stress tensors for fluid 
	       Tensor<2,2> cauchy_stress_fluid;
	       cauchy_stress_fluid = J * (-pI + _density_fluid * _viscosity *
					  (grad_v * F_Inverse + F_Inverse_T * transpose(grad_v))) * F_Inverse_T;

	      
	      
	      drag_lift_value -= 1.0 * cauchy_stress_fluid
		* state_fe_face_values.normal_vector(q_point) *
		state_fe_face_values.JxW(q_point);
	    }	  

	 }  
       return drag_lift_value[1];  
     }

      
     // compute drag value at interface
     double FaceValue(const FaceDataContainer<dealii::DoFHandler<dealdim>, VECTOR, dealdim>& fdc)
    {
      const auto & state_fe_face_values = fdc.GetFEFaceValuesState();
      unsigned int n_dofs_per_cell = fdc.GetNDoFsPerCell();
      unsigned int n_q_points = fdc.GetNQPoints();
      unsigned int color = fdc.GetBoundaryIndicator();
      unsigned int material_id = fdc.GetMaterialId();
      unsigned int material_id_neighbor = fdc.GetNbrMaterialId();
      bool at_boundary = fdc.GetIsAtBoundary();
    

   Tensor<1,2> drag_lift_value;
   drag_lift_value.clear();
     if (material_id == 0)
       {
 	   
	 if ((material_id != material_id_neighbor) //&& (!at_boundary)
	     )
 	      {
		vector<Vector<double> > _ufacevalues;
 		vector<vector<Tensor<1,dealdim> > > _ufacegrads;
 		
 		_ufacevalues.resize(n_q_points,Vector<double>(5));
 		_ufacegrads.resize(n_q_points,vector<Tensor<1,2> >(5));    
 		
 		fdc.GetFaceValuesState("state",_ufacevalues);
 		fdc.GetFaceGradsState("state",_ufacegrads);   
 		
 		for (unsigned int q_point=0;q_point<n_q_points;q_point++)
 		  { 
 		    Tensor<2,2> pI;
 		    pI[0][0] = _ufacevalues[q_point](4);
 		    pI[1][1] = _ufacevalues[q_point](4);
 		    
 		    Tensor<1,2> v;
 		    v.clear();
 		    v[0] = _ufacevalues[q_point](0);
 		    v[1] = _ufacevalues[q_point](1);
 		    
 		    Tensor<2,2> grad_v;
 		    grad_v[0][0] = _ufacegrads[q_point][0][0];
 		    grad_v[0][1] = _ufacegrads[q_point][0][1];
 		    grad_v[1][0] = _ufacegrads[q_point][1][0];
 		    grad_v[1][1] = _ufacegrads[q_point][1][1];
 		    
 		    Tensor<2,2> F;
 		    F[0][0] = 1.0 +  _ufacegrads[q_point][2][0];
 		    F[0][1] = _ufacegrads[q_point][2][1];
 		    F[1][0] = _ufacegrads[q_point][3][0];
 		    F[1][1] = 1.0 + _ufacegrads[q_point][3][1];
 		    
 		    Tensor<2,2> F_Inverse;
 		    F_Inverse = invert (F);
 		    
 		    Tensor<2,2> F_T;
 		    F_T = transpose (F);
 		    
 		    Tensor<2,2> F_Inverse_T;
 		    F_Inverse_T = transpose (F_Inverse);
 		    
 		    double J;
 		    J = determinant (F);
 		    
 		    
 		    // constitutive stress tensors for fluid 
 		    Tensor<2,2> cauchy_stress_fluid;
 		    cauchy_stress_fluid = J * (-pI + _density_fluid * _viscosity *
 					       (grad_v * F_Inverse + F_Inverse_T * transpose(grad_v))) * F_Inverse_T;
 		    
 		    
 		    
 		    drag_lift_value -=  cauchy_stress_fluid
 		      * state_fe_face_values.normal_vector(q_point) *
 		      state_fe_face_values.JxW(q_point);
 		    
 		  }	  	
 	      } 	      
 	  }
     return drag_lift_value[1];          
   }
      
      

     UpdateFlags GetFaceUpdateFlags() const
     {
       return update_values | update_quadrature_points | 
	 update_gradients | update_normal_vectors;
     }

   
     string GetName() const
     {
       return "Lift";   
     }
  
 protected:
    inline void GetFaceValues(const DOpEWrapper::FEFaceValues<dealdim>& fe_face_values,  
			      const map<string, const VECTOR* >& domain_values, string name, 
			      vector<Vector<double> >& values)
    {
      typename map<string, const VECTOR* >::const_iterator it = domain_values.find(name);
      if(it == domain_values.end())
	{
	  throw DOpEException("Did not find " + name,"LocalPDE::GetValues");
	}
      fe_face_values.get_function_values(*(it->second),values);
    }
    
    inline void GetFaceGrads(const DOpEWrapper::FEFaceValues<dealdim>& fe_face_values,  
			     const map<string, const VECTOR* >& domain_values, string name, 
			     vector<vector<Tensor<1,dealdim> > >& values)
    {
      typename map<string, const VECTOR* >::const_iterator it = domain_values.find(name);
      if(it == domain_values.end())
	{
	  throw DOpEException("Did not find " + name,"LocalPDE::GetGrads");
	}
      fe_face_values.get_function_grads(*(it->second),values);
    }
     


  private:  
  double _density_fluid,_viscosity,_lame_coefficient_mu,
    _poisson_ratio_nu, _lame_coefficient_lambda;
    
   


  };


#endif