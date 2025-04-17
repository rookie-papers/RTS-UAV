#include "Tools.h"
#include <map>


typedef struct {
    mpz_class p;
    ECP g;
} par;


typedef struct {
    mpz_class sk;
    ECP pk;
} keyPair;


typedef struct {
    vector<mpz_class> state;
    vector<ECP> out;
} parSig;

typedef struct {
    ECP R;
    mpz_class s;
} schnorr;

mpz_class H_agg(vector<ECP> L, ECP Xi);

mpz_class H_non(ECP X_tilde, vector<ECP> R, mpz_class m);

mpz_class H_sig(ECP X_tilde, ECP R, mpz_class m) ;

void Setup();

keyPair KeyGen();

parSig Sign();

vector<ECP> SignAgg(vector<vector<ECP>> out);

mpz_class KeyAggCoef(vector<ECP> L, ECP Xi);

ECP KeyAgg(vector<ECP> L) ;

schnorr Sign1(vector<mpz_class> state, vector<ECP> Rout, mpz_class sk, mpz_class m, vector<ECP> pk) ;

mpz_class SignAgg1(vector<mpz_class> out);

schnorr Sign11(ECP R, mpz_class s);

int Ver(ECP pk_tilde, mpz_class m, schnorr sigma) ;
