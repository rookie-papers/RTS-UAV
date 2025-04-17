#include "Tools.h"



typedef struct {
    mpz_class p;
    ECP P1;
    ECP2 P2;
} PubParam;

typedef struct {
    mpz_class sk;
    ECP PK;
} KeyPair;

typedef struct {
    vector<mpz_class> sk;
    vector<ECP> PK;
} KeyPairs;

typedef struct {
    ECP gr;
    mpz_class c;
    mpz_class z;
    ECP gx;
} Pai;


typedef struct {
    vector<ECP> evals1;
    ECP2 g2k;
    vector<Pai> pais;
} Rho;


typedef struct {
    vector<ECP> evals0;
    vector<ECP> evals1;
} PP;

typedef struct {
    ECP VK0;
    ECP2 VK1;
} VK;

typedef struct {
    PP pp;
    VK vk;
} Params;

typedef struct {
    ECP2 sigma1;
    ECP sigma01;
    ECP sigma11;
} Sigma;

typedef struct {
    vector<mpz_class> sk_star;
    vector<mpz_class> sk_bar;
    vector<ECP> pk_bar;
    vector<mpz_class> coffs;
} CRS;


void Setup();
vector<mpz_class> F(mpz_class s, int Wi);
KeyPairs getKeyPairs(mpz_class s, int Wi);
void KGen();
mpz_class H(ECP g, ECP gx, ECP gr);
Pai createNIZKPoK(ECP gx, mpz_class x);
bool verifyNIZKPoK(ECP gx, Pai pai);
void setCoffs();
vector<ECP> getEvals0();
Rho UGen_on(mpz_class sk, int w);
mpz_class H1(vector<ECP2> Qs, ECP2 g2k);
Params UGen_off(vector<Rho> rho_bar);
ECP2 H(mpz_class m);
vector<ECP2> Sign(mpz_class s, mpz_class m, int w);
template <typename CurvePoint>
CurvePoint computeInnerProduct(const vector<CurvePoint>& points, const vector<mpz_class>& scalars);
Sigma Aggregate(PP pp, vector<vector<ECP2>> sigma_bar, mpz_class m);
bool Verify(VK vk, Sigma sigma, mpz_class m);
int findEnd();
int findBegin();

