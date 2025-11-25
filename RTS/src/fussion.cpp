#include "../include/fussion.h"
#include <unistd.h>

csprng rng;
gmp_randstate_t state_gmp;

#define DEBUG 1

vector<mpz_class> getFactors() {
    vector<mpz_class> factors;
    int addends[] = {2, 3, 11, 19, 10177, 125527, 859267, 906349, 2508409, 2529403, 52437899, 254760293};
    mpz_class factor;
    for (int i = 0; i < 12; ++i) {
        factor = addends[i];
        factors.push_back(factor);
    }
    return factors;
}

bool isCoprime(mpz_class x, vector<mpz_class> factors) {
    for (int i = 0; i < factors.size(); ++i)
        if (x % factors[i] == 0)
            return false;
    return true;
}

mpz_class hashToCoprime(mpz_class beHashed, mpz_class module, vector<mpz_class> factors) {
    mpz_class hash = hashToZp256(beHashed, module);
    mpz_class inc = 254760293;
    while (!isCoprime(hash, factors)) {
        hash += inc;
    }
    return hash;
}

Params Setup(mpz_class &alpha, int n, int tm) {
    initState(state_gmp);
    Params pp;
    pp.n = n;
    pp.tm = tm;
    BIG q;
    BIG_rcopy(q, CURVE_Order);
    pp.q = BIG_to_mpz(q);
    ECP2_generator(&pp.P2);
    pp.g = rand_mpz(state_gmp);
    alpha = rand_mpz(state_gmp);
    pp.beta = pow_mpz(pp.g, alpha, pp.q);
    return pp;
}

vector<ECP2> getPK(vector<mpz_class> b) {
    vector<ECP2> PK;
    ECP2 A;
    ECP2_generator(&A);
    int i = 0;
    do {
        ECP2_mul(A, b[i]);
        PK.push_back(A);
    } while (++i < b.size());
    return PK;
}

UAV getUAV(Params pp, vector<mpz_class> d, vector<mpz_class> b, mpz_class id) {

    UAV uav;
    mpz_class fij;
    ECP2 temp;
    for (int i = 0; i < b.size(); ++i) {
        fij = ((d[i] * id) + b[i]) % pp.q;
        if (i == 0) {
            ECP2_generator(&temp);
            ECP2_mul(temp, fij);
            uav.PK.push_back(temp);
        } else {
            ECP2_copy(&temp, &uav.PK[i - 1]);
            ECP2_mul(temp, fij);
            uav.PK.push_back(temp);
        }
        // ElGamal
        mpz_class c1, u, beta_u;
        u = rand_mpz(state_gmp);
        c1 = pow_mpz(pp.g, u, pp.q);
        beta_u = pow_mpz(pp.beta, u, pp.q);
        beta_u = (beta_u * fij) % pp.q;
        uav.c1.push_back(c1);
        uav.c2.push_back(beta_u);
    }
    uav.ID = id;
    return uav;
}

vector<UAV> KeyGen(Params &params, mpz_class alpha, UAV_h &uavH) {
    vector<mpz_class> b, d;
    int tm = params.tm;
    for (int i = 0; i < tm - 1; ++i) { // 阈值 tm 需要 tm-1 次多项式
        b.push_back(rand_mpz(state_gmp));
        d.push_back(rand_mpz(state_gmp));
    }
    params.PK = getPK(b);
    vector<UAV> UAVs;
    for (int j = 0; j < params.n; ++j) {
        UAVs.push_back(getUAV(params, d, b, rand_mpz(state_gmp)));
    }
    uavH.alpha = alpha;
    uavH.ID = rand_mpz(state_gmp);
    return UAVs;
}

parSig Sign(Params pp, UAV uav, int t, mpz_class M, vector<mpz_class> S) {
    mpz_class cj = 1, sj = 1;
    for (int i = 0; i < t - 1; ++i) {
        cj = (cj * uav.c1[i]) % pp.q;
        sj = (sj * uav.c2[i]) % pp.q;
    }
    mpz_class Pi_0 = getPi_0(pp, S, t,uav.ID);
    sj = (sj * Pi_0) % pp.q;
    ECP Hm = hashToPoint(M, pp.q);
    ECP sigma;
    ECP_copy(&sigma, &Hm);
    ECP_mul(sigma, sj);

    parSig res;
    res.ID = uav.ID;
    res.cj = cj;
    ECP_copy(&res.sig, &sigma);

    return res;
}


vector<parSig> collectSig(Params pp, vector<UAV> UAVs, int t, mpz_class M, vector<mpz_class> S) {
    vector<parSig> sigmas;
    parSig sigma;
    for (int i = 0; i < t; ++i) {
        sigma = Sign(pp, UAVs[i], t, M, S);
        sigmas.push_back(sigma);
    }
    return sigmas;
}

Sigma AggSig(vector<parSig> parSigs, Params pp, UAV_h uavH, mpz_class PK_v) {
    initState(state_gmp);
    Sigma sigma;
    mpz_class rk = pow_mpz(PK_v, uavH.alpha, pp.q);
    mpz_class lambda_q = pp.q - 1;
    vector<mpz_class> factors = getFactors();
    rk = hashToCoprime(rk, lambda_q, factors);
    rk = invert_mpz(rk, lambda_q);
    rk = (uavH.alpha * rk) % lambda_q;

    mpz_class e = rand_mpz(state_gmp);
    mpz_class ge = pow_mpz(pp.g, e, pp.q);
    mpz_class beta_e = pow_mpz(pp.beta, e, pp.q);
    mpz_class aux_i;
    ECP sig_i;
    for (int i = 0; i < parSigs.size(); ++i) {
        aux_i = (parSigs[i].cj * ge) % pp.q;
        aux_i = pow_mpz(aux_i, rk, pp.q);
        sigma.aux.push_back(aux_i);

        ECP_copy(&sig_i, &parSigs[i].sig);
        ECP_mul(sig_i, beta_e);
        sigma.sig.push_back(sig_i);
        sigma.IDs.push_back(parSigs[i].ID);
    }

    return sigma;
}

vector<mpz_class> getPi_0s(Params pp, vector<mpz_class> ID, int t) {
    vector<mpz_class> Pis;
    mpz_class prodX = 1;
    for (int i = 0; i < t; ++i) {
        prodX = (prodX * ID[i]) % pp.q;
    }
    mpz_class denominator, numerator;
    for (int i = 0; i < t; i++) {
        denominator = 1;
        for (int j = 0; j < t; j++) {
            if (j != i) {
                mpz_class temp = ((ID[j] - ID[i]) + pp.q) % pp.q;
                denominator = (denominator * temp) % pp.q;
            }
        }
        numerator = (prodX * invert_mpz(ID[i], pp.q)) % pp.q;
        mpz_class inv_den = invert_mpz(denominator, pp.q);
        mpz_class Pi_0 = (numerator * inv_den) % pp.q;
        Pis.push_back(Pi_0);
    }
    return Pis;
}

mpz_class getPi_0(Params pp, vector<mpz_class> ID, int t, mpz_class myID) {
    mpz_class prodX = 1;
    for (int i = 0; i < t; ++i) {
        prodX = (prodX * ID[i]) % pp.q;
    }
    mpz_class denominator, numerator;
    denominator = 1;
    for (int j = 0; j < t; j++) {
        if (ID[j] != myID) {
            mpz_class temp = ((ID[j] - myID) + pp.q) % pp.q;
            denominator = (denominator * temp) % pp.q;
        }
    }
    numerator = (prodX * invert_mpz(myID, pp.q)) % pp.q;
    mpz_class inv_den = invert_mpz(denominator, pp.q);
    mpz_class Pi_0 = (numerator * inv_den) % pp.q;
    return Pi_0;
}

vector<mpz_class> getPi_0(Params pp, Sigma UAVs, int t) {
    vector<mpz_class> Pis;
    for (int i = 0; i < t; i++) {
        mpz_class numerator = 1;
        mpz_class denominator = 1;
        for (int j = 0; j < t; j++) {
            if (j != i) {
                mpz_class temp_numerator = (-UAVs.IDs[j] + pp.q) % pp.q;
                numerator = (numerator * temp_numerator) % pp.q;
                mpz_class temp_denominator = (UAVs.IDs[i] - UAVs.IDs[j] + pp.q) % pp.q;
                denominator = (denominator * temp_denominator) % pp.q;
            }
        }
        mpz_class inv_den = invert_mpz(denominator, pp.q);
        mpz_class Pi_0 = (numerator * inv_den) % pp.q;
        Pis.push_back(Pi_0);
    }
    return Pis;
}

int Verify(Sigma sigma, mpz_class sk_v, Params pp, mpz_class M, int t,vector<ECP2> PKs) {
    vector<mpz_class> Pis = getPi_0s(pp, sigma.IDs, t);
    mpz_class temp, hash;
    temp = pow_mpz(pp.beta, sk_v, pp.q);
    hash = hashToCoprime(temp, pp.q - 1, getFactors());
    vector<mpz_class> k;
    for (int i = 0; i < t; ++i) {
        temp = pow_mpz(sigma.aux[i], hash, pp.q);
        temp = invert_mpz(temp, pp.q);
        k.push_back(temp);
    }
    ECP s;
    ECP_inf(&s);   // s = 0
    ECP Hm = hashToPoint(M, pp.q);
    FP12 left, right;
    ECP ecpLeft;
    for (int i = 0; i < t; ++i) {
        ECP_mul(sigma.sig[i], k[i]);
        left = e(sigma.sig[i], pp.P2);
        ECP_copy(&ecpLeft, &Hm);
        ECP_mul(ecpLeft, Pis[i]);
        right = e(ecpLeft, PKs[i]);
//        cout << "No." << i << " partial signature pass ?: " << FP12_equals(&left, &right) << endl;
        ECP_add(&s, &sigma.sig[i]);
    }

    left = e(s, pp.P2);
    right = e(Hm, pp.PK[t - 2]);
    return FP12_equals(&left, &right);
}

int fussion() {
    initState(state_gmp);
    initRNG(&rng);
    // init params
    mpz_class alpha, M = 123456789;
    int n = 6, tm = 6;
    // setup
    Params pp = Setup(alpha, n, tm);
    UAV_h uavH;
    // KeyGen
    vector<UAV> UAVs = KeyGen(pp, alpha, uavH);
    // collect signatures
    int t = tm / 2 + 1;
    vector<mpz_class> S;
    vector<ECP2> PKs;
    for (int i = 0; i < t; ++i) {
        S.push_back(UAVs[i].ID);
        PKs.push_back(UAVs[i].PK[t-2]);
    }
    vector<parSig> sigmas = collectSig(pp, UAVs, t, M, S);
    // AggSig
    mpz_class sk_v = rand_mpz(state_gmp);
    mpz_class PK_v = pow_mpz(pp.g, sk_v, pp.q);

    Sigma sig = AggSig(sigmas, pp, uavH, PK_v);
    // Verify
    int pass = Verify(sig, sk_v, pp, M, t,PKs);

    return pass;
}

void test() {
    int sum = 0;
    for (int i = 0; i < 100; i++) {
        if (fussion() != 1) {
            cout << "本次验证失败 sum=" << sum++ << "  &&  i=" << i << endl;
            sleep(1);
        }
    }
    cout << "失败次数：" << sum << endl;
}


//int main() {
//    cout << "verify RTS success ?: " << fussion() << endl;
//    return 0;
//}

