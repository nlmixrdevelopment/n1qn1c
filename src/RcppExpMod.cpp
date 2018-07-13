#include "evaluate.h"
using namespace arma;

Rcpp::EvalBase *fev = NULL;                  // pointer to abstract base class
Rcpp::EvalBase *gev = NULL;                  // pointer to abstract base class

typedef void (*S2_fp) (int *, int *, double *, double *, double *, int *, float *, double *);
extern "C" void n1qn1_ (S2_fp simul, int n[], double x[], double f[], double g[], double var[], double eps[],
                        int mode[], int niter[], int nsim[], int imp[], int lp[], double zm[], int izs[], float rzs[], double dzs[]);

unsigned int n1qn1_calls = 0, n1qn1_grads = 0;
int n1qn1_fprint = 0;
static void fwrap(int *ind, int *n, double *x, double *f, double *g, int *ti, float *tr, double *td)
{
  int i;
  Rcpp::NumericVector par(*n), ret(*n);
  for (i = 0; i < *n; i++) par[i] = x[i];
        
  if (*ind==2 || *ind==4) {
    n1qn1_calls++;
    ret = fev->eval(par);
    if (n1qn1_fprint){
      Rprintf("%3d:%#14.8g:", n1qn1_calls, ret[0]);
      for (i = 0; i < *n; i++) Rprintf(" %#8g", x[i]);
      Rprintf("\n");
    }
    *f = ret[0];
  }
  if (*ind==3 || *ind==4) {
    n1qn1_grads++;
    ret = gev->eval(par);
    std::copy(&ret[0],&ret[0]+*n,&g[0]);
    // for (i = 0; i < *n; i++) g[i] = ret[i];
  }
}

uvec lowerTri(mat H, bool diag = false){
  unsigned int d = H.n_rows;
  mat o(d, d, fill::ones);
  if (!diag){
    return find(trimatl(o,-1));
  } else {
    return find(trimatl(o));
  }
}


RcppExport SEXP
n1qn1_wrap(
           SEXP fSEXP, SEXP gSEXP, SEXP rhoSEXP, SEXP xSEXP, SEXP epsSEXP, 
           SEXP nSEXP, SEXP modeSEXP, SEXP niterSEXP, SEXP nsimSEXP, SEXP impSEXP,
           SEXP nzmSEXP, SEXP zmSEXP, SEXP fprint_sexp) {
  BEGIN_RCPP
    n1qn1_calls=0;
  n1qn1_grads=0;
  n1qn1_fprint = INTEGER(fprint_sexp)[0];
  if (TYPEOF(fSEXP) == EXTPTRSXP){
    fev = new Rcpp::EvalCompiled(fSEXP, rhoSEXP); // xptr
  } else {
    fev = new Rcpp::EvalStandard(fSEXP, rhoSEXP); // Standard evaulation
  }
  if (TYPEOF(gSEXP) == EXTPTRSXP){
    gev = new Rcpp::EvalCompiled(gSEXP, rhoSEXP); // xptr
  } else {
    gev = new Rcpp::EvalStandard(gSEXP, rhoSEXP); // Standard evaulation
  }
  
  int n, mode, niter, nsim, imp, lp=6, nzm;
  n = INTEGER(nSEXP)[0];
  mode = INTEGER(modeSEXP)[0];
  niter = INTEGER(niterSEXP)[0];
  nsim = INTEGER(nsimSEXP)[0];
  imp = INTEGER(impSEXP)[0];
  nzm = INTEGER(nzmSEXP)[0];

  double f, eps;
  double *x = new double[n];
  double *g = new double[n];
  double *var = new double[n];
  double *zm = new double[nzm];

  int izs[1]; float rzs[1]; double dzs[1];
  std::copy(&(REAL(xSEXP)[0]),&(REAL(xSEXP)[0])+n, &x[0]);
  std::copy(&(REAL(zmSEXP)[0]),&(REAL(zmSEXP)[0])+nzm, &zm[0]);
  eps = REAL(epsSEXP)[0];
  std::fill(&var[0], &var[0]+n, 0.1);
  
  n1qn1_(fwrap,&n,x,&f,g,var,&eps,
         &mode,&niter,&nsim,&imp,&lp,zm,izs,rzs,dzs);
        
  Rcpp::NumericVector par(n);
  std::copy(&x[0],&x[0]+n,&par[0]);
  // for (i=0; i<n; i++) par[i] = x[i];
  Rcpp::NumericVector hess(nzm);
  // On input this is hessian
  // On output this is H = LDL'
  // Triangular matrix is paramterized by column instead of row.
  mat L = eye(n,n);
  mat D = mat(n,n,fill::zeros);
  mat H = mat(n,n);
  vec zmV(n*(n+1)/2);
  std::copy(&zm[0], &zm[0]+n*(n+1)/2, zmV.begin());
  H.elem(lowerTri(H,true)) = zmV;
  if (n == 1) L(0,0) = 1;
  L.elem(lowerTri(H,false)) = H.elem(lowerTri(H,0));
  D.diag() = H.diag();
  H = L*D*L.t();
  // Hessian -> c.hess
  vec hessV = H.elem(lowerTri(H,true));
  std::copy(hessV.begin(),hessV.end(),hess.begin());
  
  delete[] x;
  delete[] g;
  delete[] var;
  delete[] zm;

  return Rcpp::List::create(Rcpp::Named("value") = f,
                            Rcpp::Named("par") = par,
                            // Rcpp::Named("L") = L,
                            // Rcpp::Named("D") = D,
                            Rcpp::Named("H") = H,
                            // Rcpp::Named("zm")=zms,
                            Rcpp::Named("c.hess") = hess,
			    Rcpp::Named("n.fn") = n1qn1_calls,
			    Rcpp::Named("n.gr") = n1qn1_grads);
  END_RCPP
 }
