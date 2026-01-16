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

// Helper: Generate a small random weight (approx. 64 bits) for batching
mpz_class getSmallRandomDelta() {
    unsigned long long r = (unsigned long long)rand() << 32 | rand();
    return mpz_class(to_string(r));
}

int BatchVerify(Sigma sigma, mpz_class sk_v, Params pp, mpz_class M, int t, vector<ECP2> PKs) {
    // 1. Pre-compute Lagrange coefficients and decryption keys
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

    // 2. Initialize accumulators
    ECP s; ECP_inf(&s);             // Standard aggregated signature
    ECP S_batch; ECP_inf(&S_batch);    // Batch accumulator for signatures
    ECP2 PK_batch; ECP2_inf(&PK_batch); // Batch accumulator for public keys
    ECP Hm = hashToPoint(M, pp.q);

    // 3. Main Loop: Process partial signatures
    for (int i = 0; i < t; ++i) {
        // 1. Compute transformed partial signature: point = k[i] * sigma[i] and Aggregate into signature 's'
        ECP point;
        ECP_copy(&point, &sigma.sig[i]);
        ECP_mul(point, k[i]);
        ECP_add(&s, &point);

        // 2. Generate random weight delta to prevent cancellation attacks
        mpz_class delta = getSmallRandomDelta();
        // S_batch += delta * point
        ECP temp_batch_point;
        ECP_copy(&temp_batch_point, &point);
        ECP_mul(temp_batch_point, delta);
        ECP_add(&S_batch, &temp_batch_point);
        // PK_batch += (delta * Pis[i]) * PK[i]
        mpz_class w_R = (delta * Pis[i]) % pp.q;
        ECP2 temp_PK;
        ECP2_copy(&temp_PK, &PKs[i]);
        ECP2_mul(temp_PK, w_R);
        ECP2_add(&PK_batch, &temp_PK);
    }

    FP12 left, right;
    int pass = 1;

    // 4. Batch Verification: Check validity of all partial signatures in two pairing
    // e(Sum(delta * sig), P2) == e(H(m), Sum(delta * lambda * PK))
    left = e(S_batch, pp.P2);
    right = e(Hm, PK_batch);
    pass = pass && FP12_equals(&left, &right);

    // 5. Final Verification: Check validity of the aggregated threshold signature
    // e(s, P2) == e(H(m), GroupPK)
    left = e(s, pp.P2);
    right = e(Hm, pp.PK[t - 2]);

    return pass && FP12_equals(&left, &right);
}

void SwarmSplitting(Params &pp, UAV_h &oldHead, vector<UAV> &subSwarm) {

    // Step 1: Distributing the Transformation Key (stk)
    mpz_class phi_q = pp.q - 1;
    mpz_class ti = rand_mpz(state_gmp);
    ti = ti % phi_q;
    mpz_class g_ti = pow_mpz(pp.g, ti, pp.q);

    // Calculate Hash h = H(g^t) , For simulation, we generate a random h or hash g_ti.
    // Here we use rand for simulation cost, ensuring it simulates H(g^t).
    mpz_class h = rand_mpz(state_gmp);
    h = h % phi_q;

    // Calculate Schnorr-based stk = t + alpha * (h + 1)
    mpz_class h_plus_1 = (h + 1) % phi_q;
    mpz_class term = (oldHead.alpha * h_plus_1) % phi_q;
    mpz_class stk = (ti + term) % phi_q;

    // Simulate ElGamal Encryption/Decryption Cost
    mpz_class subHead_sk = rand_mpz(state_gmp);
    mpz_class subHead_pk = pow_mpz(pp.g, subHead_sk, pp.q);

    // [Encryption]: Encrypt stk and g_ti
    mpz_class r = rand_mpz(state_gmp);
    mpz_class C1 = pow_mpz(pp.g, r, pp.q);
    mpz_class K = pow_mpz(subHead_pk, r, pp.q);
    mpz_class C2_stk = (K * stk) % pp.q;
    mpz_class C2_gti = (K * g_ti) % pp.q;

    // [Decryption]: Decrypt to recover stk and g_ti
    mpz_class S = pow_mpz(C1, subHead_sk, pp.q);
    mpz_class S_inv = invert_mpz(S, pp.q);
    mpz_class recovered_stk = (C2_stk * S_inv) % pp.q;
    mpz_class recovered_g_ti = (C2_gti * S_inv) % pp.q;

    // Verification: Verify if g^stk == g^ti * beta^(h+1)
    mpz_class lhs = pow_mpz(pp.g, recovered_stk, pp.q);

    // RHS = g^ti * beta^(h+1)
    mpz_class beta_pow = pow_mpz(pp.beta, h_plus_1, pp.q);
    mpz_class rhs = (recovered_g_ti * beta_pow) % pp.q;

    if (lhs != rhs) {
        cout << "Error: Transformation Key Verification Failed!" << endl;
    }

    // Step 2: Updating the Share Reconstruction Keys
    int sub_swarm_size = subSwarm.size();
    for (int j = 0; j < sub_swarm_size; ++j) {
        int num_shares = std::min((int)subSwarm[j].c1.size(), sub_swarm_size);
        for (int i = 0; i < num_shares; ++i) {
            // Calculate update factor: (c1_old)^stk ; Note: c1 is c_ij, c2 is s_ij
            mpz_class update_factor = pow_mpz(subSwarm[j].c1[i], stk, pp.q);
            subSwarm[j].c2[i] = (subSwarm[j].c2[i] * update_factor) % pp.q;
        }
    }
}

void SwarmSplittingOptimized(Params &pp, UAV_h &oldHead, vector<UAV> &subSwarm) {

    // Step 1: Distributing the Transformation Key (stk)
    mpz_class phi_q = pp.q - 1;
    mpz_class ti = rand_mpz(state_gmp);
    ti = ti % phi_q;
    mpz_class g_ti = pow_mpz(pp.g, ti, pp.q);

    // Calculate Hash h = H(g^t)
    mpz_class h = rand_mpz(state_gmp);
    h = h % phi_q;

    // Calculate Schnorr-based stk = t + alpha * (h + 1)
    mpz_class h_plus_1 = (h + 1) % phi_q;
    mpz_class term = (oldHead.alpha * h_plus_1) % phi_q;
    mpz_class stk = (ti + term) % phi_q;

    // Simulate ElGamal Encryption/Decryption Cost
    mpz_class subHead_sk = rand_mpz(state_gmp);
    mpz_class subHead_pk = pow_mpz(pp.g, subHead_sk, pp.q);

    // [Encryption]
    mpz_class r = rand_mpz(state_gmp);
    mpz_class C1 = pow_mpz(pp.g, r, pp.q);
    mpz_class K = pow_mpz(subHead_pk, r, pp.q);
    mpz_class C2_stk = (K * stk) % pp.q;
    mpz_class C2_gti = (K * g_ti) % pp.q;

    // [Decryption]
    mpz_class S = pow_mpz(C1, subHead_sk, pp.q);
    mpz_class S_inv = invert_mpz(S, pp.q);
    mpz_class recovered_stk = (C2_stk * S_inv) % pp.q;
    mpz_class recovered_g_ti = (C2_gti * S_inv) % pp.q;

    // Verification: Verify if g^stk == g^ti * beta^(h+1)
    mpz_class lhs = pow_mpz(pp.g, recovered_stk, pp.q);

    mpz_class beta_pow = pow_mpz(pp.beta, h_plus_1, pp.q);
    mpz_class rhs = (recovered_g_ti * beta_pow) % pp.q;

    if (lhs != rhs) {
        cout << "Error: Optimized Verification Failed!" << endl;
    }

    // Step 2: Updating the Share Reconstruction Keys (Optimized)
    int sub_swarm_size = subSwarm.size();
    int needed_log_shares = (int)ceil(log2((double)sub_swarm_size + 1));

    for (int j = 0; j < sub_swarm_size; ++j) {
        int actual_shares = std::min((int)subSwarm[j].c1.size(), needed_log_shares);
        for (int i = 0; i < actual_shares; ++i) {
            mpz_class update_factor = pow_mpz(subSwarm[j].c1[i], stk, pp.q);
            subSwarm[j].c2[i] = (subSwarm[j].c2[i] * update_factor) % pp.q;
        }
    }
}

int fussion() {
    initState(state_gmp);
    initRNG(&rng);
    // init params
    mpz_class alpha, M = 123456789;
    int n = 128, tm = 128;
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
    auto start = std::chrono::high_resolution_clock::now();
//    int pass = Verify(sig, sk_v, pp, M, t,PKs);
    int pass = BatchVerify(sig, sk_v, pp, M, t,PKs);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    cout << "signature verify result: " << pass << endl
         << "Verify Cost ( t = " << t << "): "
         << duration.count() << " ms" << endl;

    vector<UAV> subSwarm;
    int subSwarmSize = n;
    for(int i = 0; i < subSwarmSize; ++i) {
        subSwarm.push_back(UAVs[i]);
    }
    start = std::chrono::high_resolution_clock::now();
    SwarmSplitting(pp, uavH, subSwarm);
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;

    cout << "Swarm Splitting Cost (" << subSwarmSize << " UAVs): "
         << duration.count() << " ms" << endl;

    start = std::chrono::high_resolution_clock::now();
    SwarmSplittingOptimized(pp, uavH, subSwarm);
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;

    cout << "Optimized Swarm Splitting Cost (" << subSwarmSize << " UAVs): "
         << duration.count() << " ms" << endl;

    return pass;
}

void test() {
    int sum = 0;
    for (int i = 0; i < 100; i++) {
        if (fussion() != 1) {
            cout << "This verification has failed, sum=" << sum++ << "  &&  i=" << i << endl;
            sleep(1);
        }
    }
    cout << "feat counts：" << sum << endl;
}


//int main() {
//    fussion();
//    return 0;
//}

