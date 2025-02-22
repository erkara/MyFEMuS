/*=========================================================================

 Program: FEMUS
 Module: ElemType
 Authors: Eugenio Aulisa, Giorgio Bornia

 Copyright (c) FEMTTU
 All rights reserved.

 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

//----------------------------------------------------------------------------
// includes :
//----------------------------------------------------------------------------

#include "GaussPoints.hpp"
#include "ElemType.hpp"
#include "FETypeEnum.hpp"
#include "Elem.hpp"
#include "NumericVector.hpp"

using std::cout;
using std::endl;


namespace femus {

  const unsigned elem_type::_fe_old_to_new[QL] = {2, 0, 3};
  const unsigned elem_type::_fe_new_to_old[NFE_FAMS] = {1, -7, 0, 2, -7};

  unsigned elem_type::_refindex = 1;

//   Constructor
  elem_type::elem_type(const char* geom_elem, const char* order_gauss) : _gauss(geom_elem, order_gauss) {
    isMpGDAllocated = false;
  }


  elem_type::~elem_type() {
    delete [] _X;
    delete [] _KVERT_IND;
    delete [] _IND;

    delete [] _prol_val;
    delete [] _prol_ind;
    delete [] _mem_prol_val;
    delete [] _mem_prol_ind;

    delete _pt_basis;

    if(isMpGDAllocated) {
      for(int g = 0; g < GetGaussRule().GetGaussPointsNumber(); g++) {
        delete [] _phi_mapGD[g];
        delete [] _dphidxez_mapGD[g];
      }

      delete [] _phi_mapGD;
      delete [] _dphidxez_mapGD;
    }

  }




//----------------------------------------------------------------------------------------------------
// evaluate shape functions at all quadrature points  TODO DEALLOCATE at destructor TODO FEFamilies TODO change HEX27 connectivity
//-----------------------------------------------------------------------------------------------------

  void elem_type::EvaluateShapeAtQP(const std::string geomel_id_in, const std::string fe_in) {

// if (  (!strcmp(fe_in.c_str(),"disc_linear"))  || (!strcmp(fe_in.c_str(),"quadratic")) ) {  std::cout << "BEWARE, family not supported yet" << std::endl; return; }

// ============== allocate canonical shape ==================================================================
    _phi_mapGD = new double*[GetGaussRule().GetGaussPointsNumber()];// TODO valgrind, remember to DEALLOCATE THESE, e.g. with smart pointers
    _dphidxez_mapGD = new double*[GetGaussRule().GetGaussPointsNumber()];

    for(int g = 0; g < GetGaussRule().GetGaussPointsNumber(); g++) {
      _phi_mapGD[g] = new double[GetNDofs()];
      _dphidxez_mapGD[g] = new double[GetNDofs()*GetDim()];
    }

    isMpGDAllocated = true;
// ============== allocate canonical shape ==================================================================


// HEX 27 CASE ==========================================
// HEX 27 CASE ==========================================
// HEX 27 CASE ==========================================
// from eu connectivity to my (=libmesh) connectivity
    const unsigned from_femus_to_libmesh[27] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16, 17, 18, 19, 12, 13, 14, 15, 24, 20, 21, 22, 23, 25, 26};
//                                          0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26
// from libmesh to eu connectivity
    const unsigned from_libmesh_to_femus[27] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16, 17, 18, 19, 12, 13, 14, 15, 21, 22, 23, 24, 20, 25, 26};

    if((!strcmp(fe_in.c_str(), "biquadratic")) && GetDim() == 3  && (!strcmp(geomel_id_in.c_str(), "hex"))) {
//             std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << "REMEMBER THAT ONLY HEX27 HAS A DIFFERENT CONNECTIVITY MAP"  << std::endl;

      for(int ig = 0; ig < GetGaussRule().GetGaussPointsNumber(); ig++) {

        for(int idof = 0; idof < GetNDofs(); idof++) {
//                 std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << vb << " " << ig << " " << idof << std::endl;
          _phi_mapGD[ig][idof] = GetPhi(ig)[ from_femus_to_libmesh[idof] ];
// 	std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << vb << " " << ig << " " << dof << " phi " << _phi_mapGD[vb][ig][dof] << std::endl;

// derivatives in canonical element
          for(uint idim = 0; idim < GetDim(); idim++) {
            double* dphi_g = (this->*(_DPhiXiEtaZetaPtr[idim]))(ig);      //how to access a pointer to member function
            _dphidxez_mapGD[ig][ idof + idim * GetNDofs()] =  dphi_g[ from_femus_to_libmesh[idof] ];
//           std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << " " << ig << " " << idof << " " << idim << " dphi         " << _dphidxez_mapGD[ig][ idof + idim*GetNDofs()]  << "                                      "  << std::endl;

          }

        }

      }  // end gauss



    }
// HEX 27 CASE ==========================================
// HEX 27 CASE ==========================================
// HEX 27 CASE ==========================================

// ALL THE OTHERS ==========================================
// ALL THE OTHERS ==========================================
// ALL THE OTHERS ==========================================

    else {

      for(int ig = 0; ig < GetGaussRule().GetGaussPointsNumber(); ig++) {

        for(int idof = 0; idof < GetNDofs(); idof++) {
          _phi_mapGD[ig][idof] = GetPhi(ig)[idof];

// derivatives in canonical element
          for(uint idim = 0; idim < GetDim(); idim++) {
            double* dphi_g = (this->*(_DPhiXiEtaZetaPtr[idim]))(ig);   //how to access a pointer to member function
            _dphidxez_mapGD[ig][ idof + idim * GetNDofs()] =  dphi_g[idof];

          }

        }

      }  // end gauss


    } //else HEX27



    return;
  }


//----------------------------------------------------------------------------------------------------
//BEGIN build matrix sparsity pattern size and build prolungator matrix for the LsysPde  Matrix
//-----------------------------------------------------------------------------------------------------

  void elem_type::GetSparsityPatternSize(const LinearEquation& lspdef, const LinearEquation& lspdec, const int& ielc,
                                         NumericVector* NNZ_d, NumericVector* NNZ_o,
                                         const unsigned& index_sol, const unsigned& kkindex_sol) const {
    if(lspdec._msh->GetRefinedElementIndex(ielc)) {  // coarse2fine prolongation
      for(int i = 0; i < _nf; i++) {
        int i0 = _KVERT_IND[i][0]; //id of the subdivision of the fine element
        int i1 = _KVERT_IND[i][1]; //local id node on the subdivision of the fine element
        int irow = lspdef.GetSystemDof(index_sol, kkindex_sol, ielc, i0, i1, lspdec._msh); //  local-id to dof

        int iproc = 0;

        while(irow >= lspdef.KKoffset[lspdef.KKIndex.size() - 1][iproc]) iproc++;

        int ncols = _prol_ind[i + 1] - _prol_ind[i];
        int counter_o = 0;

        for(int k = 0; k < ncols; k++) {
          int j = _prol_ind[i][k];
          int jcolumn = lspdec.GetSystemDof(index_sol, kkindex_sol, j, ielc);

          if(jcolumn < lspdec.KKoffset[0][iproc] || jcolumn >= lspdec.KKoffset[lspdef.KKIndex.size() - 1][iproc]) counter_o++;
        }

        NNZ_d->set(irow, ncols - counter_o);
        NNZ_o->set(irow, counter_o);
      }
    }
    else { // coarse2coarse prolongation
      for(int i = 0; i < _nc; i++) {
        int irow = lspdef.GetSystemDof(index_sol, kkindex_sol, ielc, 0, i, lspdec._msh);

        int iproc = 0;

        while(irow >= lspdef.KKoffset[lspdef.KKIndex.size() - 1][iproc]) iproc++;

        int jcolumn = lspdec.GetSystemDof(index_sol, kkindex_sol, i, ielc);

        if(jcolumn < lspdec.KKoffset[0][iproc] || jcolumn >= lspdec.KKoffset[lspdef.KKIndex.size() - 1][iproc]) {
          NNZ_o->set(irow, 1);
        }
        else {
          NNZ_d->set(irow, 1);
        }
      }
    }
  }


  void elem_type::BuildProlongation(const LinearEquation& lspdef, const LinearEquation& lspdec, const int& ielc, SparseMatrix* Projmat,
                                    const unsigned& index_sol, const unsigned& kkindex_sol) const {

    if(lspdec._msh->GetRefinedElementIndex(ielc)) {  // coarse2fine prolongation
      vector<int> cols(_nc);

      for(int i = 0; i < _nf; i++) {
        int i0 = _KVERT_IND[i][0]; //id of the subdivision of the fine element
        int i1 = _KVERT_IND[i][1]; //local id node on the subdivision of the fine element
        int irow = lspdef.GetSystemDof(index_sol, kkindex_sol, ielc, i0, i1, lspdec._msh);

        int ncols = _prol_ind[i + 1] - _prol_ind[i];
        cols.assign(ncols, 0);

        for(int k = 0; k < ncols; k++) {
          int j = _prol_ind[i][k];
          int jj = lspdec.GetSystemDof(index_sol, kkindex_sol, j, ielc);
          cols[k] = jj;
        }

        Projmat->insert_row(irow, ncols, cols, _prol_val[i]);
      }
    }
    else {
      vector <int> jcol(1);
      double one = 1.;

      for(int i = 0; i < _nc; i++) {
        int irow = lspdef.GetSystemDof(index_sol, kkindex_sol, ielc, 0, i, lspdec._msh);
        jcol[0] = lspdec.GetSystemDof(index_sol, kkindex_sol, i, ielc);
        Projmat->insert_row(irow, 1, jcol, &one);
      }
    }
  }


  void elem_type::BuildRestrictionTranspose(const LinearEquation& lspdef, const LinearEquation& lspdec, const int& ielc, SparseMatrix* Projmat,
      const unsigned& index_sol, const unsigned& kkindex_sol,
      const unsigned& index_pair_sol, const unsigned& kkindex_pair_sol) const {

    if(lspdec._msh->GetRefinedElementIndex(ielc)) {  // coarse2fine prolongation

      //BEGIN project nodeSolidMark
      vector < double > fineNodeSolidMark(_nf, 0);
      vector < bool > coarseNodeSolidMark(_nc, 0);

      if(_SolType == 2) {
        for(unsigned j = 0; j < _nc; j++) {
          int jadd = lspdec._msh->GetSolutionDof(j, ielc, _SolType);
          coarseNodeSolidMark[j] = lspdec._msh->GetSolidMark(jadd);
        }

        for(unsigned i = 0; i < _nf; i++) {
          int ncols = _prol_ind[i + 1] - _prol_ind[i];

          for(int k = 0; k < ncols; k++) {
            int j = _prol_ind[i][k];
            fineNodeSolidMark[i] += _prol_val[i][k] * coarseNodeSolidMark[j];
          }
        }
      }

      //END project nodeSolidMark

      vector <int> cols(_nc);
      vector <double> copy_prol_val;
      copy_prol_val.reserve(_nc);

      for(int i = 0; i < _nf; i++) {
        int i0 = _KVERT_IND[i][0]; // id of the subdivision of the fine element
        int i1 = _KVERT_IND[i][1]; // local id node on the subdivision of the fine element
        int irow = lspdef.GetSystemDof(index_sol, kkindex_sol, ielc, i0, i1, lspdec._msh);
        bool isolidmark = (fineNodeSolidMark[i] > 0.99 && fineNodeSolidMark[i] < 1.01) ? true : false;

        int ncols = _prol_ind[i + 1] - _prol_ind[i];
        cols.assign(ncols, 0);
        copy_prol_val.assign(ncols, 0);

        for(int k = 0; k < ncols; k++) {
          int j = _prol_ind[i][k];
          bool jsolidmark = coarseNodeSolidMark[j];

          if(isolidmark == jsolidmark) {
            int jcolumn = lspdec.GetSystemDof(index_sol, kkindex_sol, j, ielc);
            cols[k] = jcolumn;
            copy_prol_val[k] = _prol_val[i][k];
          }
          else {
            int jcolumn = lspdec.GetSystemDof(index_pair_sol, kkindex_pair_sol, j, ielc);
            cols[k] = jcolumn;
            copy_prol_val[k] = (index_sol != index_pair_sol) ? _prol_val[i][k] : 0.;
          }
        }

        Projmat->insert_row(irow, ncols, cols, &copy_prol_val[0]);
      }
    }
    else {
      vector <int> jcol(1);
      double one = 1.;

      for(int i = 0; i < _nc; i++) {
        int irow = lspdef.GetSystemDof(index_sol, kkindex_sol, ielc, 0, i, lspdec._msh);
        jcol[0] = lspdec.GetSystemDof(index_sol, kkindex_sol, i, ielc);
        Projmat->insert_row(irow, 1, jcol, &one);
      }
    }
  }

//----------------------------------------------------------------------------------------------------
//END build matrix sparsity pattern size and build prolungator matrix for the LsysPde  Matrix
//-----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//BEGIN  build matrix sparsity pattern size and build prolungator matrix for single solution
//-----------------------------------------------------------------------------------------------------

  void elem_type::GetSparsityPatternSize(const Mesh& meshf, const Mesh& meshc, const int& ielc, NumericVector* NNZ_d, NumericVector* NNZ_o) const {

    if(meshc.GetRefinedElementIndex(ielc)) {  // coarse2fine prolongation
      for(int i = 0; i < _nf; i++) {
        int i0 = _KVERT_IND[i][0]; //id of the subdivision of the fine element
        int i1 = _KVERT_IND[i][1]; //local id node on the subdivision of the fine element
        int irow = meshf.GetSolutionDof(ielc, i0, i1, _SolType, &meshc);

        int iproc = meshf.IsdomBisectionSearch(irow, _SolType);
        int ncols = _prol_ind[i + 1] - _prol_ind[i];
        unsigned counter_o = 0;

        for(int k = 0; k < ncols; k++) {
          int j = _prol_ind[i][k];
          int jcolumn = meshc.GetSolutionDof(j, ielc, _SolType);

          if(jcolumn < meshc._dofOffset[_SolType][iproc] || jcolumn >= meshc._dofOffset[_SolType][iproc + 1]) counter_o++;
        }

        NNZ_d->set(irow, ncols - counter_o);
        NNZ_o->set(irow, counter_o);
      }
    }
    else { // coarse2coarse prolongation
      for(int i = 0; i < _nc; i++) {
        int irow = meshf.GetSolutionDof(ielc, 0, i , _SolType, &meshc);

        int iproc = meshf.IsdomBisectionSearch(irow, _SolType);
        int jcolumn = meshc.GetSolutionDof(i, ielc, _SolType);

        if(jcolumn < meshc._dofOffset[_SolType][iproc] || jcolumn >= meshc._dofOffset[_SolType][iproc + 1]) {
          NNZ_o->set(irow, 1);
        }
        else {
          NNZ_d->set(irow, 1);
        }
      }
    }
  }

  void elem_type::BuildProlongation(const Mesh& meshf, const Mesh& meshc, const int& ielc,
                                    SparseMatrix* Projmat) const {
    if(meshc.GetRefinedElementIndex(ielc)) {  // coarse2fine prolongation
      vector<int> jcols(_nc);

      for(int i = 0; i < _nf; i++) {
        int i0 = _KVERT_IND[i][0]; //id of the subdivision of the fine element
        int i1 = _KVERT_IND[i][1]; //local id node on the subdivision of the fine element
        int irow = meshf.GetSolutionDof(ielc, i0, i1, _SolType, &meshc);
        int ncols = _prol_ind[i + 1] - _prol_ind[i];
        jcols.assign(ncols, 0);

        for(int k = 0; k < ncols; k++) {
          int j = _prol_ind[i][k];
          int jcolumn = meshc.GetSolutionDof(j, ielc, _SolType);
          jcols[k] = jcolumn;
        }

        Projmat->insert_row(irow, ncols, jcols, _prol_val[i]);
      }
    }
    else { // coarse2coarse prolongation
      vector <int> jcol(1);
      double one = 1.;

      for(int i = 0; i < _nc; i++) {
        int irow = meshf.GetSolutionDof(ielc, 0, i , _SolType, &meshc);
        jcol[0] = meshc.GetSolutionDof(i, ielc, _SolType);
        Projmat->insert_row(irow, 1, jcol, &one);
      }
    }
  }

//----------------------------------------------------------------------------------------------------
//END  build matrix sparsity pattern size and build prolungator matrix for single solution
//-----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
//BEGIN prolungator for solution printing
//----------------------------------------------------------------------------------------------------

  void elem_type::GetSparsityPatternSize(const Mesh& mesh, const int& iel, NumericVector* NNZ_d, NumericVector* NNZ_o, const unsigned& itype) const {
    bool identity = (_nlag[itype] <= _nc) ? true : false;
    for(int i = 0; i < _nlag[itype]; i++) {
      int irow = mesh.GetSolutionDof(i, iel, itype);
      int iproc = mesh.IsdomBisectionSearch(irow, itype);
      int ncols = (identity) ? 1 : _nc;
      unsigned counter_o = 0;
      unsigned counter = 0;
      for(int k = 0; k < ncols; k++) {
        double phi = (identity) ? 1. : _pt_basis->eval_phi(_pt_basis->GetIND(k), _pt_basis->GetXcoarse(i));
        if(fabs(phi) > 1.0e-14) {
          counter++;
          int kcolumn = (identity) ? mesh.GetSolutionDof(i, iel, _SolType) : mesh.GetSolutionDof(k, iel, _SolType);
          if(kcolumn < mesh._dofOffset[_SolType][iproc] || kcolumn >= mesh._dofOffset[_SolType][iproc + 1]) counter_o++;
        }
      }
      NNZ_d->set(irow, counter - counter_o);
      NNZ_o->set(irow, counter_o);
    }
  }

  void elem_type::BuildProlongation(const Mesh& mesh, const int& iel, SparseMatrix* Projmat, NumericVector* NNZ_d, NumericVector* NNZ_o, const unsigned& itype) const {
    vector<int> cols(_nc);
    vector<double> value(_nc);
    bool identity = (_nlag[itype] <= _nc) ? true : false;
    for(int i = 0; i < _nlag[itype]; i++) {
      int irow = mesh.GetSolutionDof(i, iel, itype);
      int ncols = (identity) ? 1 : _nc;
      unsigned counter = 0;
      cols.resize(_nc);
      for(int k = 0; k < ncols; k++) {
        double phi = (identity) ? 1. : _pt_basis->eval_phi(_pt_basis->GetIND(k), _pt_basis->GetXcoarse(i));
        if(fabs(phi) > 1.0e-14) {
          cols[counter]  = (identity) ? mesh.GetSolutionDof(i, iel, _SolType) : mesh.GetSolutionDof(k, iel, _SolType);
          value[counter] = phi;
          counter++;
        }
      }
      cols.resize(counter);
      int ncols_stored = static_cast <int>(floor((*NNZ_d)(irow) + (*NNZ_o)(irow) + 0.5));
      if(counter == ncols_stored) {
        Projmat->insert_row(irow, counter, cols, &value[0]);
      }
    }
  }

//----------------------------------------------------------------------------------------------------
//END prolungator for solution printing
//----------------------------------------------------------------------------------------------------



  elem_type_1D::elem_type_1D(const char* geom_elem, const char* order, const char* order_gauss) :
    elem_type(geom_elem, order_gauss) {

    basis *linearElement;

    _dim = 1;
    _DPhiXiEtaZetaPtr.resize(_dim);
    _DPhiXiEtaZetaPtr[0] = &elem_type::GetDPhiDXi;

    //************ BEGIN FE and MG SETUP ******************
    if(!strcmp(order, "linear")) 		 _SolType = 0;
    else if(!strcmp(order, "quadratic")) 	 _SolType = 1;
    else if(!strcmp(order, "biquadratic")) _SolType = 2;
    else if(!strcmp(order, "constant"))    _SolType = 3;
    else if(!strcmp(order, "disc_linear")) _SolType = 4;
    else {
      cout << order << " is not a valid option for " << geom_elem << endl;
      exit(0);
    }

    if(!strcmp(geom_elem, "line")) {  //line
      linearElement = new LineLinear;

      if(_SolType == 0) _pt_basis = new LineLinear;
      else if(_SolType == 1) _pt_basis = new LineBiquadratic;
      else if(_SolType == 2) _pt_basis = new LineBiquadratic;
      else if(_SolType == 3) _pt_basis = new line0;
      else if(_SolType == 4) _pt_basis = new linepwLinear;
      else {
        cout << order << " is not a valid option for " << geom_elem << endl;
        exit(0);
      }
    }
    else {
      cout << geom_elem << " is not a valid option" << endl;
      exit(0);
    }

    // get data from basis object
    _nc 	   = _pt_basis->_nc;
    _nf 	   = _pt_basis->_nf;
    _nlag[0] = _pt_basis->_nlag0;
    _nlag[1] = _pt_basis->_nlag1;
    _nlag[2] = _pt_basis->_nlag2;
    _nlag[3] = _pt_basis->_nlag3;


    _IND = new const int * [_nc];

    for(int i = 0; i < _nc; i++) {
      _IND[i] = _pt_basis->GetIND(i);
    }

    _KVERT_IND = new const int * [_nf];
    _X = new const double * [_nf];

    //******************************************************
    if(_SolType <= 2) {
      for(int i = 0; i < _nlag[3]; i++) {
        double xm = 0.;
        for(int k = 0; k <  _nlag[0]; k++) {
          unsigned element = *(linearElement->GetKVERT_IND(i) + 0);
          double xv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(element, k)) + 0);
          unsigned vertex = *(linearElement->GetKVERT_IND(i) + 1);
          xm += linearElement->eval_phi(linearElement->GetIND(k), linearElement->GetXcoarse(vertex)) * xv;
        }
        _pt_basis->SetX(i, 0, xm);

        //std::cout << *(_pt_basis->GetX(i) + 0) << std::endl;
      }
    }

    for(int i = 0; i < _nf; i++) {
      _KVERT_IND[i] = _pt_basis->GetKVERT_IND(i);
      _X[i] = _pt_basis->GetX(i);
    }
    //******************************************************

    // local projection matrix evaluation
    int counter = 0;

    for(int i = 0; i < _nf; i++) {

      double jac[2] = {0, 0};

      if(_SolType == 4 && i / 2 == 1) { //if piece_wise_linear derivatives
        for(int k = 0; k <  _nlag[0]; k++) {
          //coordinates of the coarse vertices with respect the fine elements
          double xv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(i % 2,  k)) + 0);
          jac[1] += linearElement->eval_dphidx(linearElement->GetIND(k), _X[i]) * xv;
        }
      }

      for(int j = 0; j < _nc; j++) {

        double phi = _pt_basis->eval_phi(_IND[j], _X[i]);


	//std::cout << j << " "<< phi << std::endl;

        if(_SolType == 4 && i / 2 == 1) { //if piece_wise_linear derivatives
          phi = jac[j];
        }

        if(fabs(phi) >= 1.0e-14) {
          counter++;
        }
      }
    }

    double* pt_d;
    int* pt_i;

    _prol_val = new double * [_nf + 1];
    _prol_ind = new int * [_nf + 1];
    _mem_prol_val = new double [counter];
    _mem_prol_ind = new int [counter];

    pt_d = _mem_prol_val;
    pt_i = _mem_prol_ind;

    for(int i = 0; i < _nf; i++) {

      double jac[2] = {0, 0};

      if(_SolType == 4 && i / 2 == 1) { //if piece_wise_linear derivatives
        for(int k = 0; k <  _nlag[0]; k++) {
          //coordinates of the coarse vertices with respect the fine elements
          double xv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(i % 2, k)) + 0);
          jac[1] += linearElement->eval_dphidx(linearElement->GetIND(k), _X[i]) * xv;
        }
        //std::cout << jac[0] << " " << jac[1] << std::endl;
      }

      _prol_val[i] = pt_d;
      _prol_ind[i] = pt_i;

      for(int j = 0; j < _nc; j++) {
        double phi = _pt_basis->eval_phi(_IND[j], _X[i]);

        if(_SolType == 4 && i / 2 == 1) { //if piece_wise_linear derivatives
          phi = jac[j];
        }

        if(fabs(phi) >= 1.0e-14) {
          *(pt_d++) = phi;
          *(pt_i++) = j;
        }
      }
    }

    _prol_val[_nf] = pt_d;
    _prol_ind[_nf] = pt_i;

    // shape function and its derivatives evaluated at Gauss'points
    int n_gauss = _gauss.GetGaussPointsNumber();

    _phi = new double*[n_gauss];
    _dphidxi  = new double*[n_gauss];
    _d2phidxi2  = new double*[n_gauss];

    _phi_memory = new double [n_gauss * _nc];
    _dphidxi_memory  = new double [n_gauss * _nc];
    _d2phidxi2_memory  = new double [n_gauss * _nc];

    for(unsigned i = 0; i < n_gauss; i++) {
      _phi[i] = &_phi_memory[i * _nc];
      _dphidxi[i]  = &_dphidxi_memory[i * _nc];
      _d2phidxi2[i]  = &_d2phidxi2_memory[i * _nc];
    }

    const double* ptx[1] = {_gauss.GetGaussWeightsPointer() + n_gauss};

    for(unsigned i = 0; i < n_gauss; i++) {
      double x[1];

      for(unsigned j = 0; j < 1; j++) {
        x[j] = *ptx[j];
        ptx[j]++;
      }

      for(int j = 0; j < _nc; j++) {
        _phi[i][j] = _pt_basis->eval_phi(_IND[j], x);
        _dphidxi[i][j] = _pt_basis->eval_dphidx(_IND[j], x);
        _d2phidxi2[i][j] = _pt_basis->eval_d2phidx2(_IND[j], x);
      }
    }


    if(_SolType < 3) {

      unsigned nFaces = 2;
      _phiFace.resize(nFaces);
      _gradPhiFace.resize(nFaces);
      _hessianPhiFace.resize(nFaces);

       double xv[2]={-1,1};

      for(int iface = 0; iface < nFaces; iface++) {
        _phiFace[iface].resize(1);
        _gradPhiFace[iface].resize(1);
        _hessianPhiFace[iface].resize(1);

	_phiFace[iface][0].resize(_nc);
        _gradPhiFace[iface][0].resize(_nc);
        _hessianPhiFace[iface][0].resize(_nc);

	for(int j = 0; j < _nc; j++) {
         _phiFace[iface][0][j] = _pt_basis->eval_phi(_IND[j], &xv[iface]);

	 //std::cout <<  _phiFace[iface][0][j] << " ";

         _gradPhiFace[iface][0][j].resize(1);
         _gradPhiFace[iface][0][j][0] = _pt_basis->eval_dphidx(_IND[j], &xv[iface]);

	 //std::cout <<  _gradPhiFace[iface][0][j][0] << " ";

          _hessianPhiFace[iface][0][j].resize(1);
          _hessianPhiFace[iface][0][j][0].resize(1);
          _hessianPhiFace[iface][0][j][0][0] = _pt_basis->eval_d2phidx2(_IND[j], &xv[iface]);

	  //std::cout <<  _hessianPhiFace[iface][0][j][0][0] << " ";
        }
        //std::cout << std::endl;
      }

    }







//=====================
    EvaluateShapeAtQP(geom_elem, order);

    delete linearElement;

  }


  elem_type_2D::elem_type_2D(const char* geom_elem, const char* order, const char* order_gauss):
    elem_type(geom_elem, order_gauss) {

    basis *linearElement;

    _dim = 2;
    _DPhiXiEtaZetaPtr.resize(_dim);
    _DPhiXiEtaZetaPtr[0] = &elem_type::GetDPhiDXi;
    _DPhiXiEtaZetaPtr[1] = &elem_type::GetDPhiDEta;

    //************ BEGIN FE and MG SETUP ******************
    if(!strcmp(order, "linear")) 	 _SolType = 0;
    else if(!strcmp(order, "quadratic"))   _SolType = 1;
    else if(!strcmp(order, "biquadratic")) _SolType = 2;
    else if(!strcmp(order, "constant"))    _SolType = 3;
    else if(!strcmp(order, "disc_linear")) _SolType = 4;
    else {
      cout << order << " is not a valid option for " << geom_elem << endl;
      abort();
    }

    if(!strcmp(geom_elem, "quad")) {  //QUAD
      linearElement = new QuadLinear;

      if(_SolType == 0) _pt_basis = new QuadLinear;
      else if(_SolType == 1) _pt_basis = new QuadQuadratic;
      else if(_SolType == 2) _pt_basis = new QuadBiquadratic;
      else if(_SolType == 3) _pt_basis = new quad0;
      else if(_SolType == 4) _pt_basis = new quadpwLinear;
      else {
        cout << order << " is not a valid option for " << geom_elem << endl;
        abort();
      }
    }
    else if(!strcmp(geom_elem, "tri")) {  //TRIANGLE
      linearElement = new TriLinear;

      if(_SolType == 0) _pt_basis = new TriLinear;
      else if(_SolType == 1) _pt_basis = new TriQuadratic;
      else if(_SolType == 2) _pt_basis = new TriBiquadratic;
      else if(_SolType == 3) _pt_basis = new tri0;
      else if(_SolType == 4) _pt_basis = new tripwLinear;
      else {
        cout << order << " is not a valid option for " << geom_elem << endl;
        abort();
      }
    }
    else {
      cout << geom_elem << " is not a valid option" << endl;
      abort();
    }

    // get data from basis object
    _nc 	   = _pt_basis->_nc;
    _nf 	   = _pt_basis->_nf;
    _nlag[0] = _pt_basis->_nlag0;
    _nlag[1] = _pt_basis->_nlag1;
    _nlag[2] = _pt_basis->_nlag2;
    _nlag[3] = _pt_basis->_nlag3;

    _IND = new const int * [_nc];

    for(int i = 0; i < _nc; i++) {
      _IND[i] = _pt_basis->GetIND(i);
    }

    _KVERT_IND = new const int * [_nf];
    _X = new const double * [_nf];



    //***********************************************************
    // construction of coordinates
    if(_SolType <= 2) {
      for(int i = 0; i < _nlag[3]; i++) {
        double xm = 0., ym = 0.;
        for(int k = 0; k <  _nlag[0]; k++) {
          unsigned element = *(linearElement->GetKVERT_IND(i) + 0);
          double xv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(element, k)) + 0);
          double yv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(element, k)) + 1);
          unsigned vertex = *(linearElement->GetKVERT_IND(i) + 1);
          xm += linearElement->eval_phi(linearElement->GetIND(k), linearElement->GetXcoarse(vertex)) * xv;
          ym += linearElement->eval_phi(linearElement->GetIND(k), linearElement->GetXcoarse(vertex)) * yv;
        }
        _pt_basis->SetX(i, 0, xm);
        _pt_basis->SetX(i, 1, ym);

        //std::cout << *(_pt_basis->GetX(i) + 0) << " " << *(_pt_basis->GetX(i) + 1) << std::endl;
      }
    }



    for(int i = 0; i < _nf; i++) {
      _KVERT_IND[i] = _pt_basis->GetKVERT_IND(i);
      _X[i] = _pt_basis->GetX(i);
    }

    //***********************************************************

    // local projection matrix evaluation
    int counter = 0;

    for(int i = 0; i < _nf; i++) {

      double jac[3] = {0, 0, 0};

      if(_SolType == 4 && i / 4 >= 1) { //if piece_wise_linear derivatives
        for(int k = 0; k <  _nlag[0]; k++) {
          //coordinates of the coarse vertices with respect the fine elements
          double xv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(i % 4, k)) + 0);
          double yv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(i % 4, k)) + 1);
          if(i / 4 == 1) {
            jac[1] += linearElement->eval_dphidx(linearElement->GetIND(k), _X[i]) * xv;
            jac[2] += linearElement->eval_dphidx(linearElement->GetIND(k), _X[i]) * yv;
          }
          else if(i / 4 == 2) {
            jac[1] += linearElement->eval_dphidy(linearElement->GetIND(k), _X[i]) * xv;
            jac[2] += linearElement->eval_dphidy(linearElement->GetIND(k), _X[i]) * yv;
          }
        }
      }

      for(int j = 0; j < _nc; j++) {
        double phi = _pt_basis->eval_phi(_IND[j], _X[i]);

        if(_SolType == 4 && i / 4 >= 1) { //if piece_wise_linear derivatives
          phi = jac[j];
        }

        if(fabs(phi) >= 1.0e-14) {
          counter++;
        }
      }
    }

    double* pt_d;
    int* pt_i;

    _prol_val = new double * [_nf + 1];
    _prol_ind = new int * [_nf + 1];
    _mem_prol_val = new double [counter];
    _mem_prol_ind = new int [counter];

    pt_d = _mem_prol_val;
    pt_i = _mem_prol_ind;

    for(int i = 0; i < _nf; i++) {

      double jac[3] = {0, 0, 0};

      if(_SolType == 4 && i / 4 >= 1) { //if piece_wise_linear derivatives
        for(int k = 0; k <  _nlag[0]; k++) {
          //coordinates of the coarse vertices with respect the fine elements
          double xv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(i % 4, k)) + 0);
          double yv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(i % 4, k)) + 1);
          if(i / 4 == 1) {
            jac[1] += linearElement->eval_dphidx(linearElement->GetIND(k), _X[i]) * xv;
            jac[2] += linearElement->eval_dphidx(linearElement->GetIND(k), _X[i]) * yv;
          }
          else if(i / 4 == 2) {
            jac[1] += linearElement->eval_dphidy(linearElement->GetIND(k), _X[i]) * xv;
            jac[2] += linearElement->eval_dphidy(linearElement->GetIND(k), _X[i]) * yv;
          }
        }
        //std::cout << jac[0] << " " << jac[1] << " " << jac[2] << std::endl;
      }

      _prol_val[i] = pt_d;
      _prol_ind[i] = pt_i;

      for(int j = 0; j < _nc; j++) {
        double phi = _pt_basis->eval_phi(_IND[j], _X[i]);

        if(_SolType == 4 && i / 4 >= 1) { //if piece_wise_linear derivatives
          phi = jac[j];
        }

        if(fabs(phi) >= 1.0e-14) {
          *(pt_d++) = phi;
          *(pt_i++) = j;
        }
      }
    }

    _prol_val[_nf] = pt_d;
    _prol_ind[_nf] = pt_i;

    // shape function and its derivatives evaluated at Gauss'points
    int n_gauss = _gauss.GetGaussPointsNumber();

    _phi = new double*[n_gauss];
    _dphidxi  = new double*[n_gauss];
    _dphideta = new double*[n_gauss];

    _d2phidxi2  = new double*[n_gauss];
    _d2phideta2 = new double*[n_gauss];

    _d2phidxideta  = new double*[n_gauss];

    _phi_memory = new double [n_gauss * _nc];
    _dphidxi_memory  = new double [n_gauss * _nc];
    _dphideta_memory = new double [n_gauss * _nc];

    _d2phidxi2_memory  = new double [n_gauss * _nc];
    _d2phideta2_memory = new double [n_gauss * _nc];

    _d2phidxideta_memory  = new double [n_gauss * _nc];

    for(unsigned i = 0; i < n_gauss; i++) {
      _phi[i] = &_phi_memory[i * _nc];
      _dphidxi[i]  = &_dphidxi_memory[i * _nc];
      _dphideta[i] = &_dphideta_memory[i * _nc];

      _d2phidxi2[i]  = &_d2phidxi2_memory[i * _nc];
      _d2phideta2[i] = &_d2phideta2_memory[i * _nc];

      _d2phidxideta[i]  = &_d2phidxideta_memory[i * _nc];

    }

    const double* ptx[2] = {_gauss.GetGaussWeightsPointer() + n_gauss, _gauss.GetGaussWeightsPointer() + 2 * n_gauss};

    for(unsigned i = 0; i < n_gauss; i++) {
      double x[2];

      for(unsigned j = 0; j < 2; j++) {
        x[j] = *ptx[j];
        ptx[j]++;
      }

      for(int j = 0; j < _nc; j++) {
        _phi[i][j] = _pt_basis->eval_phi(_IND[j], x);
        _dphidxi[i][j] = _pt_basis->eval_dphidx(_IND[j], x);
        _dphideta[i][j] = _pt_basis->eval_dphidy(_IND[j], x);
        _d2phidxi2[i][j] = _pt_basis->eval_d2phidx2(_IND[j], x);
        _d2phideta2[i][j] = _pt_basis->eval_d2phidy2(_IND[j], x);
        _d2phidxideta[i][j] = _pt_basis->eval_d2phidxdy(_IND[j], x);
      }

    }

    //std::cout << std::endl;
    if(_SolType < 3) {
      basis *linearLine = new LineLinear;

      Gauss faceGaussPoint = Gauss("line", order_gauss);
      const double* xi = {faceGaussPoint.GetGaussWeightsPointer() + faceGaussPoint.GetGaussPointsNumber()};

      basis *faceBasis = linearLine;

      unsigned nFaces = _pt_basis->faceNumber[2];
      _phiFace.resize(nFaces);
      _gradPhiFace.resize(nFaces);
      _hessianPhiFace.resize(nFaces);

      for(int iface = 0; iface < nFaces; iface++) {
        std::vector< double > xv(faceBasis -> _nc);
        std::vector< double > yv(faceBasis -> _nc);
        for(int jnode = 0; jnode < faceBasis -> _nc; jnode++) {
          unsigned iDof = _pt_basis->GetFaceDof(iface, jnode);
          xv[jnode] = *(_pt_basis->GetXcoarse(iDof) + 0);
          yv[jnode] = *(_pt_basis->GetXcoarse(iDof) + 1);
        }
        unsigned nGaussPts = faceGaussPoint.GetGaussPointsNumber();
        _phiFace[iface].resize(nGaussPts);
        _gradPhiFace[iface].resize(nGaussPts);
        _hessianPhiFace[iface].resize(nGaussPts);
        for(unsigned i = 0; i < nGaussPts; i++) {
          double x[2] = {0., 0.};
          for(int j = 0; j <  faceBasis -> _nc; j++) {
            x[0] += faceBasis->eval_phi(faceBasis->GetIND(j), &xi[i]) * xv[j] ;
            x[1] += faceBasis->eval_phi(faceBasis->GetIND(j), &xi[i]) * yv[j] ;
          }
          _phiFace[iface][i].resize(_nc);
          _gradPhiFace[iface][i].resize(_nc);
          _hessianPhiFace[iface][i].resize(_nc);
          for(int j = 0; j < _nc; j++) {
            _phiFace[iface][i][j] = _pt_basis->eval_phi(_IND[j], x);

            _gradPhiFace[iface][i][j].resize(2);
            _gradPhiFace[iface][i][j][0] = _pt_basis->eval_dphidx(_IND[j], x);
            _gradPhiFace[iface][i][j][1] = _pt_basis->eval_dphidy(_IND[j], x);

            _hessianPhiFace[iface][i][j].resize(2);
            _hessianPhiFace[iface][i][j][0].resize(2);
            _hessianPhiFace[iface][i][j][1].resize(2);
            _hessianPhiFace[iface][i][j][0][0] = _pt_basis->eval_d2phidx2(_IND[j], x);
            _hessianPhiFace[iface][i][j][1][1] = _pt_basis->eval_d2phidy2(_IND[j], x);
            _hessianPhiFace[iface][i][j][0][1] = _pt_basis->eval_d2phidxdy(_IND[j], x);
            _hessianPhiFace[iface][i][j][1][0] = _hessianPhiFace[iface][i][j][1][0];
          }
        }
      }
      delete linearLine;
    }


//
//=====================
    EvaluateShapeAtQP(geom_elem, order);

    //std::cout << std::endl;

    delete linearElement;


  }

  elem_type_3D::elem_type_3D(const char* geom_elem, const char* order, const char* order_gauss) :
    elem_type(geom_elem, order_gauss)  {

    _dim = 3;
    _DPhiXiEtaZetaPtr.resize(_dim);
    _DPhiXiEtaZetaPtr[0] = &elem_type::GetDPhiDXi;
    _DPhiXiEtaZetaPtr[1] = &elem_type::GetDPhiDEta;
    _DPhiXiEtaZetaPtr[2] = &elem_type::GetDPhiDZeta;

    basis *linearElement;

    //************ BEGIN FE and MG SETUP ******************
    if(!strcmp(order, "linear")) 	 _SolType = 0;
    else if(!strcmp(order, "quadratic")) 	 _SolType = 1;
    else if(!strcmp(order, "biquadratic")) _SolType = 2;
    else if(!strcmp(order, "constant"))    _SolType = 3;
    else if(!strcmp(order, "disc_linear")) _SolType = 4;
    else {
      cout << order << " is not a valid option for " << geom_elem << endl;
      exit(0);
    }

    if(!strcmp(geom_elem, "hex")) {  //HEX

      linearElement = new HexLinear;

      if(_SolType == 0) _pt_basis = new HexLinear;
      else if(_SolType == 1) _pt_basis = new HexQuadratic;
      else if(_SolType == 2) _pt_basis = new HexBiquadratic;
      else if(_SolType == 3) _pt_basis = new hex0;
      else if(_SolType == 4) _pt_basis = new hexpwLinear;
      else {
        cout << order << " is not a valid option for " << geom_elem << endl;
        exit(0);
      }
    }
    else if(!strcmp(geom_elem, "wedge")) {  //WEDGE
      linearElement = new WedgeLinear;

      if(_SolType == 0) _pt_basis = new WedgeLinear;
      else if(_SolType == 1) _pt_basis = new WedgeQuadratic;
      else if(_SolType == 2) _pt_basis = new WedgeBiquadratic;
      else if(_SolType == 3) _pt_basis = new wedge0;
      else if(_SolType == 4) _pt_basis = new wedgepwLinear;
      else {
        cout << order << " is not a valid option for " << geom_elem << endl;
        exit(0);
      }
    }
    else if(!strcmp(geom_elem, "tet")) {  //TETRAHEDRA
      linearElement = new TetLinear;

      if(_SolType == 0) _pt_basis = new TetLinear;
      else if(_SolType == 1) _pt_basis = new TetQuadratic;
      else if(_SolType == 2) _pt_basis = new TetBiquadratic;
      else if(_SolType == 3) _pt_basis = new tet0;
      else if(_SolType == 4) _pt_basis = new tetpwLinear;
      else {
        cout << order << " is not a valid option for " << geom_elem << endl;
        exit(0);
      }
    }
    else {
      cout << geom_elem << " is not a valid option" << endl;
      exit(0);
    }

    // get data from basis object
    _nc 	   = _pt_basis->_nc;
    _nf 	   = _pt_basis->_nf;
    _nlag[0] = _pt_basis->_nlag0;
    _nlag[1] = _pt_basis->_nlag1;
    _nlag[2] = _pt_basis->_nlag2;
    _nlag[3] = _pt_basis->_nlag3;


    _IND = new const int * [_nc];

    for(int i = 0; i < _nc; i++) {
      _IND[i] = _pt_basis->GetIND(i);
    }

    _KVERT_IND = new const int * [_nf];
    _X = new const double * [_nf];

    // ****************************************************
    // construction of coordinates
    if(_SolType <= 2) {
      for(int i = 0; i < _nlag[3]; i++) {
        double xm = 0., ym = 0., zm = 0.;
        for(int k = 0; k <  _nlag[0]; k++) {
          unsigned element = *(linearElement->GetKVERT_IND(i) + 0);
          double xv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(element, k)) + 0);
          double yv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(element, k)) + 1);
          double zv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(element, k)) + 2);

          unsigned vertex = *(linearElement->GetKVERT_IND(i) + 1);
          xm += linearElement->eval_phi(linearElement->GetIND(k), linearElement->GetXcoarse(vertex)) * xv;
          ym += linearElement->eval_phi(linearElement->GetIND(k), linearElement->GetXcoarse(vertex)) * yv;
          zm += linearElement->eval_phi(linearElement->GetIND(k), linearElement->GetXcoarse(vertex)) * zv;
        }
        _pt_basis->SetX(i, 0, xm);
        _pt_basis->SetX(i, 1, ym);
        _pt_basis->SetX(i, 2, zm);

        //std::cout << *(_pt_basis->GetX(i) + 0) << " " << *(_pt_basis->GetX(i) + 1) << " " << *(_pt_basis->GetX(i) + 2) << std::endl;
      }
    }

    // ****************************************************

    for(int i = 0; i < _nf; i++) {
      _KVERT_IND[i] = _pt_basis->GetKVERT_IND(i);
      _X[i] = _pt_basis->GetX(i);
    }

    for(int i = 0; i < _nf; i++) {
      _KVERT_IND[i] = _pt_basis->GetKVERT_IND(i);
      _X[i] = _pt_basis->GetX(i);
    }

    // local projection matrix evaluation
    int counter = 0;

    for(int i = 0; i < _nf; i++) {

      double jac[4] = {0, 0, 0, 0};
      if(_SolType == 4 && i / 8 >= 1) { //if piece_wise_linear derivatives
        for(int k = 0; k < _nlag[0]; k++) {
          //coordinates of the coarse vertices with respect to the fine elements
          double xv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(i % 8, k)) + 0);
          double yv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(i % 8, k)) + 1);
          double zv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(i % 8, k)) + 2);
          if(i / 8 == 1) {
            jac[1] += linearElement->eval_dphidx(linearElement->GetIND(k), _X[i]) * xv;
            jac[2] += linearElement->eval_dphidx(linearElement->GetIND(k), _X[i]) * yv;
            jac[3] += linearElement->eval_dphidx(linearElement->GetIND(k), _X[i]) * zv;
          }
          else if(i / 8 == 2) {
            jac[1] += linearElement->eval_dphidy(linearElement->GetIND(k), _X[i]) * xv;
            jac[2] += linearElement->eval_dphidy(linearElement->GetIND(k), _X[i]) * yv;
            jac[3] += linearElement->eval_dphidy(linearElement->GetIND(k), _X[i]) * zv;
          }
          else if(i / 8 == 3) {
            jac[1] += linearElement->eval_dphidz(linearElement->GetIND(k), _X[i]) * xv;
            jac[2] += linearElement->eval_dphidz(linearElement->GetIND(k), _X[i]) * yv;
            jac[3] += linearElement->eval_dphidz(linearElement->GetIND(k), _X[i]) * zv;
          }
        }
      }
      for(int j = 0; j < _nc; j++) {
        double phi = _pt_basis->eval_phi(_IND[j], _X[i]);
        if(_SolType == 4 && i / 8 >= 1) { //if piece_wise_linear
          phi = jac[j];
        }
        if(fabs(phi) >= 1.0e-14) {
          counter++;
        }
      }
    }

    double* pt_d;
    int* pt_i;

    _prol_val = new double * [_nf + 1];
    _prol_ind = new int * [_nf + 1];
    _mem_prol_val = new double [counter];
    _mem_prol_ind = new int [counter];

    pt_d = _mem_prol_val;
    pt_i = _mem_prol_ind;

    for(int i = 0; i < _nf; i++) {

      double jac[4] = {0, 0, 0, 0};
      if(_SolType == 4 && i / 8 >= 1) { //if piece_wise_linear derivatives
        for(int k = 0; k <  _nlag[0]; k++) {
          //coordinates of the coarse vertices with respect to the fine elements
          double xv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(i % 8, k)) + 0);
          double yv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(i % 8, k)) + 1);
          double zv = * (linearElement->GetXcoarse(linearElement->GetFine2CoarseVertexMapping(i % 8, k)) + 2);
          if(i / 8 == 1) {
            jac[1] += linearElement->eval_dphidx(linearElement->GetIND(k), _X[i]) * xv;
            jac[2] += linearElement->eval_dphidx(linearElement->GetIND(k), _X[i]) * yv;
            jac[3] += linearElement->eval_dphidx(linearElement->GetIND(k), _X[i]) * zv;
          }
          else if(i / 8 == 2) {
            jac[1] += linearElement->eval_dphidy(linearElement->GetIND(k), _X[i]) * xv;
            jac[2] += linearElement->eval_dphidy(linearElement->GetIND(k), _X[i]) * yv;
            jac[3] += linearElement->eval_dphidy(linearElement->GetIND(k), _X[i]) * zv;
          }
          else if(i / 8 == 3) {
            jac[1] += linearElement->eval_dphidz(linearElement->GetIND(k), _X[i]) * xv;
            jac[2] += linearElement->eval_dphidz(linearElement->GetIND(k), _X[i]) * yv;
            jac[3] += linearElement->eval_dphidz(linearElement->GetIND(k), _X[i]) * zv;
          }
        }
        //std::cout << jac[0] << " " << jac[1] << " " << jac[2] << " " << jac[3] << std::endl;
      }

      _prol_val[i] = pt_d;
      _prol_ind[i] = pt_i;
      for(int j = 0; j < _nc; j++) {
        double phi = _pt_basis->eval_phi(_IND[j], _X[i]);
        if(_SolType == 4 && i / 8 >= 1) { //if piece_wise_linear derivatives
          phi = jac[j];
        }
        if(fabs(phi) >= 1.0e-14) {
          *(pt_d++) = phi;
          *(pt_i++) = j;
        }
      }
    }

    _prol_val[_nf] = pt_d;
    _prol_ind[_nf] = pt_i;

    // shape function and its derivatives evaluated at Gauss'points
    int n_gauss = _gauss.GetGaussPointsNumber();

    _phi = new double*[n_gauss];
    _dphidxi  = new double*[n_gauss];
    _dphideta = new double*[n_gauss];
    _dphidzeta = new double*[n_gauss];

    _d2phidxi2  = new double*[n_gauss];
    _d2phideta2 = new double*[n_gauss];
    _d2phidzeta2 = new double*[n_gauss];

    _d2phidxideta  = new double*[n_gauss];
    _d2phidetadzeta = new double*[n_gauss];
    _d2phidzetadxi = new double*[n_gauss];

    _phi_memory = new double [n_gauss * _nc];
    _dphidxi_memory  = new double [n_gauss * _nc];
    _dphideta_memory = new double [n_gauss * _nc];
    _dphidzeta_memory = new double [n_gauss * _nc];

    _d2phidxi2_memory  = new double [n_gauss * _nc];
    _d2phideta2_memory = new double [n_gauss * _nc];
    _d2phidzeta2_memory = new double [n_gauss * _nc];

    _d2phidxideta_memory  = new double [n_gauss * _nc];
    _d2phidetadzeta_memory = new double [n_gauss * _nc];
    _d2phidzetadxi_memory = new double [n_gauss * _nc];

    for(unsigned i = 0; i < n_gauss; i++) {
      _phi[i] = &_phi_memory[i * _nc];
      _dphidxi[i]  = &_dphidxi_memory[i * _nc];
      _dphideta[i] = &_dphideta_memory[i * _nc];
      _dphidzeta[i] = &_dphidzeta_memory[i * _nc];

      _d2phidxi2[i]  = &_d2phidxi2_memory[i * _nc];
      _d2phideta2[i] = &_d2phideta2_memory[i * _nc];
      _d2phidzeta2[i] = &_d2phidzeta2_memory[i * _nc];

      _d2phidxideta[i]  = &_d2phidxideta_memory[i * _nc];
      _d2phidetadzeta[i] = &_d2phidetadzeta_memory[i * _nc];
      _d2phidzetadxi[i] = &_d2phidzetadxi_memory[i * _nc];

    }

    const double* ptx[3] = {_gauss.GetGaussWeightsPointer() +   n_gauss,
                            _gauss.GetGaussWeightsPointer() + 2 * n_gauss,
                            _gauss.GetGaussWeightsPointer() + 3 * n_gauss
                           };

    for(unsigned i = 0; i < n_gauss; i++) {
      double x[3];

      for(unsigned j = 0; j < 3; j++) {
        x[j] = *ptx[j];
        ptx[j]++;
      }

      double phisum = 0.;
      double dphidxisum = 0.;
      double dphidetasum = 0.;
      double dphidzetasum = 0.;
      double d2phidxi2sum = 0.;
      double d2phideta2sum = 0.;
      double d2phidzeta2sum = 0.;
      double d2phidxidetasum = 0.;
      double d2phidetadzetasum = 0.;
      double d2phidzetadxisum = 0.;

      for(int j = 0; j < _nc; j++) {
        _phi[i][j] = _pt_basis->eval_phi(_IND[j], x);
        _dphidxi[i][j] = _pt_basis->eval_dphidx(_IND[j], x);
        _dphideta[i][j] = _pt_basis->eval_dphidy(_IND[j], x);
        _dphidzeta[i][j] = _pt_basis->eval_dphidz(_IND[j], x);

        _d2phidxi2[i][j] = _pt_basis->eval_d2phidx2(_IND[j], x);
        _d2phideta2[i][j] = _pt_basis->eval_d2phidy2(_IND[j], x);
        _d2phidzeta2[i][j] = _pt_basis->eval_d2phidz2(_IND[j], x);

        _d2phidxideta[i][j] = _pt_basis->eval_d2phidxdy(_IND[j], x);
        _d2phidetadzeta[i][j] = _pt_basis->eval_d2phidydz(_IND[j], x);
        _d2phidzetadxi[i][j] = _pt_basis->eval_d2phidzdx(_IND[j], x);

        phisum += _phi[i][j];

        dphidxisum += _dphidxi[i][j];
        dphidetasum += _dphideta[i][j];
        dphidzetasum += _dphidzeta[i][j];

        d2phidxi2sum += _d2phidxi2[i][j];
        d2phideta2sum += _d2phideta2[i][j];
        d2phidzeta2sum += _d2phidzeta2[i][j];

        d2phidxidetasum += _d2phidxideta[i][j];
        d2phidetadzetasum +=  _d2phidetadzeta[i][j];
        d2phidzetadxisum += _d2phidzetadxi[i][j];
      }
    }

    //std::cout << std::endl;
    if(_SolType < 3) {
      basis *linearQuad = new QuadLinear;
      basis *linearTri = new TriLinear;

      Gauss quadGaussPoint = Gauss("quad", order_gauss);
      Gauss triGaussPoint = Gauss("tri", order_gauss);

      Gauss *faceGauss[2];
      faceGauss[0] = &quadGaussPoint;
      faceGauss[1] = &triGaussPoint;

      const double* xi[2] = { faceGauss[0]->GetGaussWeightsPointer() + faceGauss[0]->GetGaussPointsNumber(),
                              faceGauss[1]->GetGaussWeightsPointer() + faceGauss[1]->GetGaussPointsNumber()
                            };

      const double* yi[2] = { faceGauss[0]->GetGaussWeightsPointer() + 2 * (faceGauss[0]->GetGaussPointsNumber()),
                              faceGauss[1]->GetGaussWeightsPointer() + 2 * (faceGauss[1]->GetGaussPointsNumber())
                            };

      basis *faceBasis[2];
      faceBasis[0] = linearQuad;
      faceBasis[1] = linearTri;

      unsigned nFaces = _pt_basis->faceNumber[2];
      _phiFace.resize(nFaces);
      _gradPhiFace.resize(nFaces);
      _hessianPhiFace.resize(nFaces);

      for(unsigned type = 0; type < 2; type++) {
        for(int iface = _pt_basis->faceNumber[type]; iface < _pt_basis->faceNumber[type + 1]; iface++) {
          std::vector< double > xv(faceBasis[type] -> _nc);
          std::vector< double > yv(faceBasis[type] -> _nc);
          std::vector< double > zv(faceBasis[type] -> _nc);
          for(int jnode = 0; jnode < faceBasis[type] -> _nc; jnode++) {
            unsigned iDof = _pt_basis->GetFaceDof(iface, jnode);
            xv[jnode] = *(_pt_basis->GetXcoarse(iDof) + 0);
            yv[jnode] = *(_pt_basis->GetXcoarse(iDof) + 1);
            zv[jnode] = *(_pt_basis->GetXcoarse(iDof) + 2);
          }

	  unsigned nGaussPts = faceGauss[type]->GetGaussPointsNumber();
	  _phiFace[iface].resize(nGaussPts);
	  _gradPhiFace[iface].resize(nGaussPts);
	  _hessianPhiFace[iface].resize(nGaussPts);

          for(unsigned i = 0; i < nGaussPts; i++) {
            double x[3] = {0., 0., 0.};
            const double vertex[2] = {xi[type][i], yi[type][i]};
            for(int j = 0; j <  faceBasis[type] -> _nc; j++) {
              x[0] += faceBasis[type]->eval_phi(faceBasis[type]->GetIND(j), vertex) * xv[j] ;
              x[1] += faceBasis[type]->eval_phi(faceBasis[type]->GetIND(j), vertex) * yv[j] ;
              x[2] += faceBasis[type]->eval_phi(faceBasis[type]->GetIND(j), vertex) * zv[j] ;
            }

	    _phiFace[iface][i].resize(_nc);
	    _gradPhiFace[iface][i].resize(_nc);
	    _hessianPhiFace[iface][i].resize(_nc);
            for(int j = 0; j < _nc; j++) {
              _phiFace[iface][i][j] = _pt_basis->eval_phi(_IND[j], x);

	      _gradPhiFace[iface][i][j].resize(3);
              _gradPhiFace[iface][i][j][0] = _pt_basis->eval_dphidx(_IND[j], x);
              _gradPhiFace[iface][i][j][1] = _pt_basis->eval_dphidy(_IND[j], x);
              _gradPhiFace[iface][i][j][2] = _pt_basis->eval_dphidz(_IND[j], x);

	      _hessianPhiFace[iface][i][j].resize(3);
	      _hessianPhiFace[iface][i][j][0].resize(3);
	      _hessianPhiFace[iface][i][j][1].resize(3);
	      _hessianPhiFace[iface][i][j][2].resize(3);

	      _hessianPhiFace[iface][i][j][0][0] = _pt_basis->eval_d2phidx2(_IND[j], x);
              _hessianPhiFace[iface][i][j][0][1] = _pt_basis->eval_d2phidxdy(_IND[j], x);
	      _hessianPhiFace[iface][i][j][0][2] = _pt_basis->eval_d2phidzdx(_IND[j], x);

	      _hessianPhiFace[iface][i][j][1][0] =  _hessianPhiFace[iface][i][j][0][1];
	      _hessianPhiFace[iface][i][j][1][1] = _pt_basis->eval_d2phidy2(_IND[j], x);
              _hessianPhiFace[iface][i][j][1][2] = _pt_basis->eval_d2phidydz(_IND[j], x);

	      _hessianPhiFace[iface][i][j][2][0] = _hessianPhiFace[iface][i][j][0][2];
	      _hessianPhiFace[iface][i][j][2][1] = _hessianPhiFace[iface][i][j][1][2];
	      _hessianPhiFace[iface][i][j][2][2] = _pt_basis->eval_d2phidz2(_IND[j], x);
            }
          }
        }
      }
      delete linearQuad;
      delete linearTri;
    }

//=====================
    EvaluateShapeAtQP(geom_elem, order);

    //std::cout << std::endl;

    delete linearElement;

  }

//---------------------------------------------------------------------------------------------------------

  template <class type>
  void elem_type_1D::Jacobian_type(const vector < vector < type > >& vt, const unsigned& ig, type& Weight,
                                   vector < double >& phi, vector < type >& gradphi, vector < type >& nablaphi) const {

    phi.resize(_nc);
    gradphi.resize(_nc * 1);
    nablaphi.resize(_nc * 1);

    type Jac = 0.;
    type JacI;

    const double* dxi = _dphidxi[ig];

    for(int inode = 0; inode < _nc; inode++, dxi++) {
      Jac += (*dxi) * vt[0][inode];
    }

    Weight = Jac * _gauss.GetGaussWeightsPointer()[ig];

    JacI = 1 / Jac;

    dxi = _dphidxi[ig];
    const double* dxi2 = _d2phidxi2[ig];

    for(int inode = 0; inode < _nc; inode++, dxi++, dxi2++) {
      phi[inode] = _phi[ig][inode];
      gradphi[inode] = (*dxi) * JacI;
      nablaphi[inode] = (*dxi2) * JacI * JacI;
    }

  }

//---------------------------------------------------------------------------------------------------------

  template <class type>
  void elem_type_1D::JacobianSur_type(const vector < vector < type > >& vt, const unsigned& ig, type& Weight,
                                      vector < double >& phi, vector < type >& gradphi, vector < type >& normal) const {

    phi.resize(_nc);
    normal.resize(2);

    type Jac[2][2] = {{0., 0.}, {0., 0.}};
    type JacI[2][2];

    const double* dfeta = _dphidxi[ig];

    for(int inode = 0; inode < _nc; inode++, dfeta++) {
      Jac[0][0] += (*dfeta) * vt[0][inode];
      Jac[1][0] += (*dfeta) * vt[1][inode];
    }

//   normal module
    type modn = sqrt(Jac[0][0] * Jac[0][0] + Jac[1][0] * Jac[1][0]);

    normal[0] =  Jac[1][0] / modn;
    normal[1] = -Jac[0][0] / modn;

    //The derivative of x with respect to eta (dx/deta) has the opposite sign with respect to the normal
    //obtained as cross product between (dx/deta , dy/deta, 0) x (0,0,1)
    //The Jacobian has the structure
    // |dx/deta  -nx|
    // |dy/deta  -ny|
    Jac[0][1] = -normal[0];
    Jac[1][1] = -normal[1];

    //The determinant of that matrix is the area
    type det = (Jac[0][0] * Jac[1][1] - Jac[0][1] * Jac[1][0]);

    JacI[0][0] =  Jac[1][1] / det;
    JacI[0][1] = -Jac[0][1] / det;
    JacI[1][0] = -Jac[1][0] / det;
    JacI[1][1] =  Jac[0][0] / det;

    Weight = det * _gauss.GetGaussWeightsPointer()[ig];

    for(int inode = 0; inode < _nc; inode++) {
      phi[inode] = _phi[ig][inode];
    }

  }

//---------------------------------------------------------------------------------------------------------

  template <class type>
  void elem_type_2D::Jacobian_type(const vector < vector < type > >& vt, const unsigned& ig, type& Weight,
                                   vector < double >& phi, vector < type >& gradphi, vector < type >& nablaphi) const {

    phi.resize(_nc);
    gradphi.resize(_nc * 2);
    nablaphi.resize(_nc * 3);

    type Jac[2][2] = {{0, 0}, {0, 0}};
    type JacI[2][2];
    const double* dxi = _dphidxi[ig];
    const double* deta = _dphideta[ig];

    for(int inode = 0; inode < _nc; inode++, dxi++, deta++) {
      Jac[0][0] += (*dxi) * vt[0][inode];
      Jac[0][1] += (*dxi) * vt[1][inode];
      Jac[1][0] += (*deta) * vt[0][inode];
      Jac[1][1] += (*deta) * vt[1][inode];
    }

    type det = (Jac[0][0] * Jac[1][1] - Jac[0][1] * Jac[1][0]);

    JacI[0][0] = Jac[1][1] / det;
    JacI[0][1] = -Jac[0][1] / det;
    JacI[1][0] = -Jac[1][0] / det;
    JacI[1][1] = Jac[0][0] / det;

    Weight = det * _gauss.GetGaussWeightsPointer()[ig];

    dxi = _dphidxi[ig];
    deta = _dphideta[ig];

    const double* dxi2 = _d2phidxi2[ig];
    const double* deta2 = _d2phideta2[ig];
    const double* dxideta = _d2phidxideta[ig];

    for(int inode = 0; inode < _nc; inode++, dxi++, deta++, dxi2++, deta2++, dxideta++) {

      phi[inode] = _phi[ig][inode];

      gradphi[2 * inode + 0] = (*dxi) * JacI[0][0] + (*deta) * JacI[0][1];
      gradphi[2 * inode + 1] = (*dxi) * JacI[1][0] + (*deta) * JacI[1][1];

      nablaphi[3 * inode + 0] =
        ((*dxi2)   * JacI[0][0] + (*dxideta) * JacI[0][1]) * JacI[0][0] +
        ((*dxideta) * JacI[0][0] + (*deta2)  * JacI[0][1]) * JacI[0][1];
      nablaphi[3 * inode + 1] =
        ((*dxi2)   * JacI[1][0] + (*dxideta) * JacI[1][1]) * JacI[1][0] +
        ((*dxideta) * JacI[1][0] + (*deta2)  * JacI[1][1]) * JacI[1][1];
      nablaphi[3 * inode + 2] =
        ((*dxi2)   * JacI[0][0] + (*dxideta) * JacI[0][1]) * JacI[1][0] +
        ((*dxideta) * JacI[0][0] + (*deta2)  * JacI[0][1]) * JacI[1][1];

    }
  }

//---------------------------------------------------------------------------------------------------------

  template <class type>
  void elem_type_2D::JacobianSur_type(const vector < vector < type > >& vt, const unsigned& ig, type& Weight,
                                      vector < double >& phi, vector < type >& gradphi, vector < type >& normal) const {
    phi.resize(_nc);
    normal.resize(3);

    type Jac[3][3] = {{0., 0., 0.}, {0., 0., 0.}, {0., 0., 0.}};

    const double* dfx = _dphidxi[ig];
    const double* dfy = _dphideta[ig];

    for(int inode = 0; inode < _nc; inode++, dfx++, dfy++) {
      Jac[0][0] += (*dfx) * vt[0][inode];
      Jac[1][0] += (*dfx) * vt[1][inode];
      Jac[2][0] += (*dfx) * vt[2][inode];

      Jac[0][1] += (*dfy) * vt[0][inode];
      Jac[1][1] += (*dfy) * vt[1][inode];
      Jac[2][1] += (*dfy) * vt[2][inode];
    }

    //   normal module
    type nx = Jac[1][0] * Jac[2][1] - Jac[1][1] * Jac[2][0];
    type ny = Jac[0][1] * Jac[2][0] - Jac[2][1] * Jac[0][0];
    type nz = Jac[0][0] * Jac[1][1] - Jac[0][1] * Jac[1][0];
    type invModn = 1. / sqrt(nx * nx + ny * ny + nz * nz);

    normal[0] = (nx) * invModn;
    normal[1] = (ny) * invModn;
    normal[2] = (nz) * invModn;

    Jac[0][2] = normal[0];
    Jac[1][2] = normal[1];
    Jac[2][2] = normal[2];

    //the determinant of the matrix is the area
    type det = (Jac[0][0] * (Jac[1][1] * Jac[2][2] - Jac[1][2] * Jac[2][1]) +
                Jac[0][1] * (Jac[1][2] * Jac[2][0] - Jac[1][0] * Jac[2][2]) +
                Jac[0][2] * (Jac[1][0] * Jac[2][1] - Jac[1][1] * Jac[2][0]));

    Weight = det * _gauss.GetGaussWeightsPointer()[ig];

    for(int inode = 0; inode < _nc; inode++) {
      phi[inode] = _phi[ig][inode];
    }

    //TODO warning the surface gradient is missing!!!!!!!!!!!!!!!
  }


//---------------------------------------------------------------------------------------------------------
  template <class type>
  void elem_type_3D::Jacobian_type(const vector < vector < type > >& vt, const unsigned& ig, type& Weight,
                                   vector < double >& phi, vector < type >& gradphi, vector < type >& nablaphi) const {

    phi.resize(_nc);
    gradphi.resize(_nc * 3);
    nablaphi.resize(_nc * 6);


    type Jac[3][3] = {{0., 0., 0.}, {0., 0., 0.}, {0., 0., 0.}};
    type JacI[3][3];

    const double* dxi = _dphidxi[ig];
    const double* deta = _dphideta[ig];
    const double* dzeta = _dphidzeta[ig];

    for(int inode = 0; inode < _nc; inode++, dxi++, deta++, dzeta++) {
      Jac[0][0] += (*dxi) * vt[0][inode];
      Jac[0][1] += (*dxi) * vt[1][inode];
      Jac[0][2] += (*dxi) * vt[2][inode];
      Jac[1][0] += (*deta) * vt[0][inode];
      Jac[1][1] += (*deta) * vt[1][inode];
      Jac[1][2] += (*deta) * vt[2][inode];
      Jac[2][0] += (*dzeta) * vt[0][inode];
      Jac[2][1] += (*dzeta) * vt[1][inode];
      Jac[2][2] += (*dzeta) * vt[2][inode];
    }

    type det = (Jac[0][0] * (Jac[1][1] * Jac[2][2] - Jac[1][2] * Jac[2][1]) +
                Jac[0][1] * (Jac[1][2] * Jac[2][0] - Jac[1][0] * Jac[2][2]) +
                Jac[0][2] * (Jac[1][0] * Jac[2][1] - Jac[1][1] * Jac[2][0]));

    JacI[0][0] = (-Jac[1][2] * Jac[2][1] + Jac[1][1] * Jac[2][2]) / det;
    JacI[0][1] = (Jac[0][2] * Jac[2][1] - Jac[0][1] * Jac[2][2]) / det;
    JacI[0][2] = (-Jac[0][2] * Jac[1][1] + Jac[0][1] * Jac[1][2]) / det;
    JacI[1][0] = (Jac[1][2] * Jac[2][0] - Jac[1][0] * Jac[2][2]) / det;
    JacI[1][1] = (-Jac[0][2] * Jac[2][0] + Jac[0][0] * Jac[2][2]) / det;
    JacI[1][2] = (Jac[0][2] * Jac[1][0] - Jac[0][0] * Jac[1][2]) / det;
    JacI[2][0] = (-Jac[1][1] * Jac[2][0] + Jac[1][0] * Jac[2][1]) / det;
    JacI[2][1] = (Jac[0][1] * Jac[2][0] - Jac[0][0] * Jac[2][1]) / det;
    JacI[2][2] = (-Jac[0][1] * Jac[1][0] + Jac[0][0] * Jac[1][1]) / det;

    Weight = det * _gauss.GetGaussWeightsPointer()[ig];

    dxi = _dphidxi[ig];
    deta = _dphideta[ig];
    dzeta = _dphidzeta[ig];

    const double* dxi2 = _d2phidxi2[ig];
    const double* deta2 = _d2phideta2[ig];
    const double* dzeta2 = _d2phidzeta2[ig];
    const double* dxideta = _d2phidxideta[ig];
    const double* detadzeta = _d2phidetadzeta[ig];
    const double* dzetadxi = _d2phidzetadxi[ig];

    for(int inode = 0; inode < _nc; inode++, dxi++, deta++, dzeta++, dxi2++, deta2++, dzeta2++, dxideta++, detadzeta++, dzetadxi++) {

      phi[inode] = _phi[ig][inode];

      gradphi[3 * inode + 0] = (*dxi) * JacI[0][0] + (*deta) * JacI[0][1] + (*dzeta) * JacI[0][2];
      gradphi[3 * inode + 1] = (*dxi) * JacI[1][0] + (*deta) * JacI[1][1] + (*dzeta) * JacI[1][2];
      gradphi[3 * inode + 2] = (*dxi) * JacI[2][0] + (*deta) * JacI[2][1] + (*dzeta) * JacI[2][2];

      nablaphi[6 * inode + 0] =
        ((*dxi2)    * JacI[0][0] + (*dxideta)  * JacI[0][1] + (*dzetadxi) * JacI[0][2]) * JacI[0][0] +
        ((*dxideta) * JacI[0][0] + (*deta2)    * JacI[0][1] + (*detadzeta) * JacI[0][2]) * JacI[0][1] +
        ((*dzetadxi) * JacI[0][0] + (*detadzeta) * JacI[0][1] + (*dzeta2)   * JacI[0][2]) * JacI[0][2];
      nablaphi[6 * inode + 1] =
        ((*dxi2)    * JacI[1][0] + (*dxideta)  * JacI[1][1] + (*dzetadxi) * JacI[1][2]) * JacI[1][0] +
        ((*dxideta) * JacI[1][0] + (*deta2)    * JacI[1][1] + (*detadzeta) * JacI[1][2]) * JacI[1][1] +
        ((*dzetadxi) * JacI[1][0] + (*detadzeta) * JacI[1][1] + (*dzeta2)   * JacI[1][2]) * JacI[1][2];
      nablaphi[6 * inode + 2] =
        ((*dxi2)    * JacI[2][0] + (*dxideta)  * JacI[2][1] + (*dzetadxi) * JacI[2][2]) * JacI[2][0] +
        ((*dxideta) * JacI[2][0] + (*deta2)    * JacI[2][1] + (*detadzeta) * JacI[2][2]) * JacI[2][1] +
        ((*dzetadxi) * JacI[2][0] + (*detadzeta) * JacI[2][1] + (*dzeta2)   * JacI[2][2]) * JacI[2][2];
      nablaphi[6 * inode + 3] =
        ((*dxi2)    * JacI[0][0] + (*dxideta)  * JacI[0][1] + (*dzetadxi) * JacI[0][2]) * JacI[1][0] +
        ((*dxideta) * JacI[0][0] + (*deta2)    * JacI[0][1] + (*detadzeta) * JacI[0][2]) * JacI[1][1] +
        ((*dzetadxi) * JacI[0][0] + (*detadzeta) * JacI[0][1] + (*dzeta2)   * JacI[0][2]) * JacI[1][2];
      nablaphi[6 * inode + 4] =
        ((*dxi2)    * JacI[1][0] + (*dxideta)  * JacI[1][1] + (*dzetadxi) * JacI[1][2]) * JacI[2][0] +
        ((*dxideta) * JacI[1][0] + (*deta2)    * JacI[1][1] + (*detadzeta) * JacI[1][2]) * JacI[2][1] +
        ((*dzetadxi) * JacI[1][0] + (*detadzeta) * JacI[1][1] + (*dzeta2)   * JacI[1][2]) * JacI[2][2];
      nablaphi[6 * inode + 5] =
        ((*dxi2)    * JacI[2][0] + (*dxideta)  * JacI[2][1] + (*dzetadxi) * JacI[2][2]) * JacI[0][0] +
        ((*dxideta) * JacI[2][0] + (*deta2)    * JacI[2][1] + (*detadzeta) * JacI[2][2]) * JacI[0][1] +
        ((*dzetadxi) * JacI[2][0] + (*detadzeta) * JacI[2][1] + (*dzeta2)   * JacI[2][2]) * JacI[0][2];
    }

  }

//---------------------------------------------------------------------------------------------------------

} //end namespace femus







