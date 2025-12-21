#include "../include/RTS.h"
#include <unistd.h>

namespace RTS_web {
    csprng rng_websocket;
    gmp_randstate_t state_gmp_websocket;
    std::atomic<int> serialNumber{0};

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
        initState(state_gmp_websocket);
        Params pp;
        pp.n = n;
        pp.tm = tm;
        BIG q;
        BIG_rcopy(q, CURVE_Order);
        pp.q = BIG_to_mpz(q);
        ECP2_generator(&pp.P2);
        pp.g = rand_mpz(state_gmp_websocket);
        alpha = rand_mpz(state_gmp_websocket);
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
            u = rand_mpz(state_gmp_websocket);
            c1 = pow_mpz(pp.g, u, pp.q);
            beta_u = pow_mpz(pp.beta, u, pp.q);
            beta_u = (beta_u * fij) % pp.q;
            uav.c1.push_back(c1);
            uav.c2.push_back(beta_u);
        }
        uav.ID = id;
        // fetch_add(1) = serialNumber++
        uav.serialNumber = serialNumber.fetch_add(1);
        return uav;
    }

    vector<UAV> KeyGen(Params &params, mpz_class alpha, UAV_h &uavH) {
        vector<mpz_class> b, d;
        int tm = params.tm;
        for (int i = 0; i < tm - 1; ++i) { // 阈值 tm 需要 tm-1 次多项式
            b.push_back(rand_mpz(state_gmp_websocket));
            d.push_back(rand_mpz(state_gmp_websocket));
        }
        params.PK = getPK(b);
        vector<UAV> UAVs;
        for (int j = 0; j < params.n; ++j) {
            UAVs.push_back(getUAV(params, d, b, rand_mpz(state_gmp_websocket)));
        }
        uavH.alpha = alpha;
        uavH.ID = rand_mpz(state_gmp_websocket);
        return UAVs;
    }

    parSig Sign(Params pp, UAV uav, int t, mpz_class M, const std::string &bitmap,
                const vector<mpz_class> &registeredIDs) {

        // 1. Reconstruct the full signer set S locally from the bitmap
        vector<mpz_class> S;
        for (size_t i = 0; i < bitmap.size() * 8; ++i) {
            int byteIdx = i / 8;
            int bitIdx = i % 8;
            // Check if the bit is set
            if ((static_cast<unsigned char>(bitmap[byteIdx]) >> bitIdx) & 1) {
                S.push_back(registeredIDs[i]);
            }
        }

        // 2. Compute basic signature components (cj, sj)
        mpz_class cj = 1, sj = 1;
        for (int i = 0; i < t - 1; ++i) {
            cj = (cj * uav.c1[i]) % pp.q;
            sj = (sj * uav.c2[i]) % pp.q;
        }

        // 3. Compute Lagrange coefficient for this UAV based on set S
        mpz_class Pi_0 = getPi_0(pp, S, t, uav.ID);

        // 4. Generate the signature point on the Elliptic Curve
        sj = (sj * Pi_0) % pp.q;
        ECP Hm = hashToPoint(M, pp.q);
        ECP sigma;
        ECP_copy(&sigma, &Hm);
        ECP_mul(sigma, sj); // sigma = Hm ^ sj

        // 5. Package result
        parSig res;
        res.cj = cj;
        ECP_copy(&res.sig, &sigma);
        res.index = static_cast<short>(uav.serialNumber);

        return res;
    }


    vector<parSig> collectSig(Params pp, vector<UAV> UAVs, int t, mpz_class M, const std::string &bitmap,
                              const vector<mpz_class> &registeredIDs) {
        vector<parSig> sigmas;

        // Iterate through all UAVs to check if they are selected
        for (int i = 0; i < UAVs.size(); ++i) {
            int idx = UAVs[i].serialNumber;
            int byteIdx = idx / 8;
            int bitIdx = idx % 8;
            bool isSelected = false;

            // Check against the bitmap
            if (byteIdx < bitmap.size()) {
                if ((static_cast<unsigned char>(bitmap[byteIdx]) >> bitIdx) & 1) {
                    isSelected = true;
                }
            }

            // If selected, generate signature and add to list
            if (isSelected) {
                parSig sigma = Sign(pp, UAVs[i], t, M, bitmap, registeredIDs);
                sigmas.push_back(sigma);
            }
        }
        return sigmas;
    }

    bool compareParSig(const parSig &a, const parSig &b) {
        return a.index < b.index;
    }

    Sigma AggSig(vector<parSig> parSigs, Params pp, UAV_h uavH, mpz_class PK_v) {
        initState(state_gmp_websocket);
        Sigma sigma;
        std::sort(parSigs.begin(), parSigs.end(), compareParSig);
        mpz_class rk = pow_mpz(PK_v, uavH.alpha, pp.q);
        mpz_class lambda_q = pp.q - 1;
        vector<mpz_class> factors = getFactors();
        rk = hashToCoprime(rk, lambda_q, factors);
        rk = invert_mpz(rk, lambda_q);
        rk = (uavH.alpha * rk) % lambda_q;

        mpz_class e = rand_mpz(state_gmp_websocket);
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
            sigma.indices.push_back(parSigs[i].index);
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

    int Verify(Sigma sigma, mpz_class sk_v, Params pp, mpz_class M,
               const vector<mpz_class> &globalIDs,
               const vector<ECP2> &globalPKs) {

        int t = sigma.indices.size();

        // Containers for the actual participants involved in this signature
        vector<mpz_class> activeIDs;
        vector<ECP2> activePKs;
        activeIDs.reserve(t);
        activePKs.reserve(t);

        // 1. Reconstruct active participants based on indices
        for (short idx: sigma.indices) {
            // Safety check for bounds
            if (idx < 0 || idx >= globalIDs.size()) {
                cout << "[Verify] Error: Invalid index in signature." << endl;
                return 0;
            }
            activeIDs.push_back(globalIDs[idx]);
            activePKs.push_back(globalPKs[idx]);
        }

        // 2. Compute Lagrange interpolation coefficients for the active set
        vector<mpz_class> Pis = getPi_0s(pp, activeIDs, t);

        // 3. Compute unblinding factors (remove the mask applied by the aggregator)
        mpz_class temp, hash;
        temp = pow_mpz(pp.beta, sk_v, pp.q);
        hash = hashToCoprime(temp, pp.q - 1, getFactors());
        vector<mpz_class> k;
        for (int i = 0; i < t; ++i) {
            temp = pow_mpz(sigma.aux[i], hash, pp.q);
            temp = invert_mpz(temp, pp.q); // Invert to remove the factor
            k.push_back(temp);
        }
        ECP s;
        ECP_inf(&s);   // Initialize accumulator s = 0
        ECP Hm = hashToPoint(M, pp.q);
        FP12 left, right;
        ECP ecpLeft;
        // 4. Unblind and Aggregate partial signatures
        for (int i = 0; i < t; ++i) {
            ECP_mul(sigma.sig[i], k[i]);
            // Single signature verification for debugging
            left = e(sigma.sig[i], pp.P2);
            ECP_copy(&ecpLeft, &Hm);
            ECP_mul(ecpLeft, Pis[i]);
            right = e(ecpLeft, activePKs[i]);
            if (!FP12_equals(&left, &right)) cout << "Part " << i << " failed!" << endl;
            // Aggregate: s = s + sigma_i
            ECP_add(&s, &sigma.sig[i]);
        }

        // 5. Check if e(s, P2) == e(Hm, PK_agg)
        left = e(s, pp.P2);
        right = e(Hm, pp.PK[t - 2]); // PK[t-2] corresponds to the threshold public key

        return FP12_equals(&left, &right);
    }
}