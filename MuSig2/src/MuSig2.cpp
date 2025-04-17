#include "../include/MuSig2.h"

gmp_randstate_t MuSig2;
#define V 2
#define N 128

par param_multiSig2_ECC;

mpz_class H_agg(vector<ECP> L, ECP Xi) {
    bool compress = true;
    // L || Xi
    octet hash = getOctet(96);
    octet temp = getOctet(96);
    for (int i = 1; i <= N; ++i) {
        ECP_toOctet(&temp, &L[i], compress);
        concatOctet(&hash, &temp);
    }
    ECP_toOctet(&temp, &Xi, compress);
    concatOctet(&hash, &temp);
    // H(L || Xi)
    BIG order, res;
    BIG_rcopy(order, CURVE_Order);
    hashZp256(res, &hash, order);

    free(hash.val);
    free(temp.val);
    return BIG_to_mpz(res);
}

mpz_class H_non(ECP X_tilde, vector<ECP> R, mpz_class m) {
    octet hash = getOctet(96);
    octet temp = getOctet(96);
    bool compress = true;
    // X_tilde
    ECP_toOctet(&temp, &X_tilde, compress);
    concatOctet(&hash, &temp);
    // X_tilde || (R1,...,R_v)
    for (int i = 1; i <= V; ++i) {
        ECP_toOctet(&temp, &R[i], compress);
        concatOctet(&hash, &temp);
    }
    // X_tilde || (R1,...,R_v) || m
    temp = mpzToOctet(m);
    concatOctet(&hash, &temp);
    // H(X_tilde || (R1,...,R_v) || m)
    BIG hash_b;
    BIG order;
    BIG_rcopy(order, CURVE_Order);
    hashZp256(hash_b, &hash, order);

    free(hash.val);
    free(temp.val);

    return BIG_to_mpz(hash_b);
}

mpz_class H_sig(ECP X_tilde, ECP R, mpz_class m) {
    octet ocSig = getOctet(96);
    octet temp = getOctet(96);
    bool compress = true;
    // X_tilde || R || m
    ECP_toOctet(&temp, &X_tilde, compress);
    concatOctet(&ocSig, &temp);
    ECP_toOctet(&temp, &R, compress);
    concatOctet(&ocSig, &temp);
    temp = mpzToOctet(m);
    concatOctet(&ocSig, &temp);
    // H(X_tilde || R || m)
    BIG hash_c;
    BIG order;
    BIG_rcopy(order, CURVE_Order);
    hashZp256(hash_c, &ocSig, order);

    free(ocSig.val);
    free(temp.val);

    return BIG_to_mpz(hash_c);
}

void Setup() {
    initState(MuSig2);
    BIG order;
    BIG_rcopy(order, CURVE_Order);
    param_multiSig2_ECC.p = BIG_to_mpz(order);
    ECP_generator(&param_multiSig2_ECC.g);
}

keyPair KeyGen() {
    initState(MuSig2);
    keyPair kp;
    kp.sk = rand_mpz(MuSig2);
    ECP_generator(&kp.pk);
    ECP_mul(kp.pk, kp.sk);
    return kp;
}


parSig Sign() {
    initState(MuSig2);
    vector<mpz_class> state(N + 1, 1);
    vector<ECP> out(N + 1, param_multiSig2_ECC.g);
    for (int j = 1; j <= V; ++j) {
        state[j] = rand_mpz(MuSig2);
        ECP_generator(&out[j]);
        ECP_mul(out[j], state[j]);
    }
    parSig sig;
    sig.state = state;
    sig.out = out;
    return sig;
}

vector<ECP> SignAgg(vector<vector<ECP>> out) {
    vector<ECP> R(V + 1);
    for (int j = 1; j <= V; ++j) {
        ECP_inf(&R[j]);
        for (int i = 1; i <= N; ++i) {
            ECP_add(&R[j],&out[i][j]);
        }
    }
    return R;
}

mpz_class KeyAggCoef(vector<ECP> L, ECP Xi) {
    return H_agg(L, Xi);
}


ECP KeyAgg(vector<ECP> L) {
    vector<mpz_class> a(N + 1);

    for (int i = 1; i <= N; ++i) {
        a[i] = KeyAggCoef(L, L[i]);
    }
    ECP X_tilde;
    ECP_copy(&X_tilde, &L[1]);
    ECP_mul(X_tilde, a[1]);
    for (int i = 2; i <= N; ++i) {
        ECP_mul(L[i], a[i]);
        ECP_add(&X_tilde, &L[i]);
    }
    return X_tilde;
}


schnorr Sign1(vector<mpz_class> state, vector<ECP> Rout, mpz_class sk, mpz_class m, vector<ECP> pk) {
    mpz_class xi = sk;
    ECP Xi;
    ECP_generator(&Xi);
    ECP_mul(Xi, xi);

    // pk.push_back(Xi);// 执行了这一步之后，pk编程所有人的公钥集合，也就是L，我们选择直接传pk = L ，
    vector<ECP> L = pk;
    mpz_class ai = KeyAggCoef(L, Xi);
    ECP X_tilde = KeyAgg(L);

    // b = H_non(X_tilde, (R_1,...,R_v), m)
    mpz_class b = H_non(X_tilde, Rout, m);

    // R = Π { Rj^{b^{j-1}} }
    ECP R;
    ECP_inf(&R);
    for (int j = 1; j <= V; ++j) {
        mpz_class exp = pow_mpz(b, j - 1, param_multiSig2_ECC.p);
        ECP_mul(Rout[j], exp);
        ECP_add(&R, &Rout[j]);
    }

    // c = H_sig(X_tilde, R, m)
    mpz_class c = H_sig(X_tilde, R, m);

    // si = (c * ai * xi) + Σ { state[j] * b^{j-1} }
    mpz_class lambda_p = param_multiSig2_ECC.p;

    mpz_class si = (ai * xi) % lambda_p;
    si = (c * si) % lambda_p;
    for (int j = 1; j <= V; ++j) {
        mpz_class exp = pow_mpz(b, j - 1, param_multiSig2_ECC.p);
        mpz_class factor = ((state[j] * exp) % lambda_p);
        si = (si + factor) % lambda_p;
    }

    // 返回签名
    schnorr sigma;
    sigma.s = si;
    sigma.R = R;
    return sigma;
}


mpz_class SignAgg1(vector<mpz_class> out) {
    mpz_class s = 0;
    mpz_class lambda_p = param_multiSig2_ECC.p;
    for (int i = 1; i <= N; ++i) {
        s = (s + out[i]) % lambda_p;
    }
    return s;
}

schnorr Sign11(ECP R, mpz_class s) {
    schnorr sigma;
    sigma.R = R;
    sigma.s = s;
    return sigma;
}

int Ver(ECP pk_tilde, mpz_class m, schnorr sigma) {
    ECP X_tilde = pk_tilde;
    ECP R = sigma.R;
    mpz_class s = sigma.s;

    mpz_class c = H_sig(X_tilde, R, m);

    ECP left;
    ECP_copy(&left,&param_multiSig2_ECC.g);
    ECP_mul(left, s);
    ECP right;
    ECP_copy(&right,&X_tilde);
    ECP_mul(right, c);
    ECP_add(&right,&R);
    return ECP_equals(&left,&right);
}


void MultiSig2() {
    Setup();
    vector<keyPair> keys(N + 1);
    for (int i = 1; i <= N; ++i)
        keys[i] = KeyGen();
    vector<ECP> L(N + 1);
    for (int i = 1; i <= N; ++i)
        L[i] = keys[i].pk;
    vector<vector<mpz_class>> state(N + 1);
    vector<vector<ECP>> out(N + 1);
    for (int i = 1; i <= N; ++i) {
        parSig sig = Sign();
        state[i] = sig.state;
        out[i] = sig.out;
    }
    vector<ECP> Rout = SignAgg(out);

    mpz_class m = 0x1e23b0dbf852148cfb0d8f3d2f4602a172a93666713b92e994432a111f11be18_mpz;
    vector<mpz_class> out1(N + 1, 1);
    schnorr sig1;
    for (int i = 1; i <= N; ++i) {
        sig1 = Sign1(state[i], Rout, keys[i].sk, m, L);
        out1[i] = sig1.s;
    }
    mpz_class s = SignAgg1(out1);

    int res = Ver(KeyAgg(L), m, Sign11(sig1.R, s));
    cout << "verify success : " << res << endl;
}





#include "benchmark/benchmark.h"

static void MultiSig2_Setup(benchmark::State &state) {
    for (auto _: state) {
        Setup();
    }
}

static void MultiSig2_KeyGen(benchmark::State &state) {
    // 初始化
    Setup();
    for (auto _: state) {
        KeyGen();
    }
}

static void MultiSig2_Sign(benchmark::State &state) {
    Setup();
    vector<keyPair> keys(N + 1);
    for (int i = 1; i <= N; ++i)
        keys[i] = KeyGen();
    vector<ECP> L(N + 1);
    for (int i = 1; i <= N; ++i)
        L[i] = keys[i].pk;

    for (auto _: state) {
        Sign();
    }
}


static void MultiSig2_SignAgg(benchmark::State &state1) {
    Setup();
    vector<keyPair> keys(N + 1);
    for (int i = 1; i <= N; ++i)
        keys[i] = KeyGen();
    vector<ECP> L(N + 1);
    for (int i = 1; i <= N; ++i)
        L[i] = keys[i].pk;
    vector<vector<mpz_class>> state(N + 1);
    vector<vector<ECP>> out(N + 1);
    for (int i = 1; i <= N; ++i) {
        parSig sig = Sign();
        state[i] = sig.state;
        out[i] = sig.out;
    }

    for (auto _: state1) {
        SignAgg(out);
    }
}

static void MultiSig2_Sign1(benchmark::State &state1) {
    Setup();
    vector<keyPair> keys(N + 1);
    for (int i = 1; i <= N; ++i)
        keys[i] = KeyGen();
    vector<ECP> L(N + 1);
    for (int i = 1; i <= N; ++i)
        L[i] = keys[i].pk;
    vector<vector<mpz_class>> state(N + 1);
    vector<vector<ECP>> out(N + 1);
    for (int i = 1; i <= N; ++i) {
        parSig sig = Sign();
        state[i] = sig.state;
        out[i] = sig.out;
    }
    vector<ECP> Rout = SignAgg(out);

    mpz_class m = 0x1e23b0dbf852148cfb0d8f3d2f4602a172a93666713b92e994432a111f11be18_mpz;
    for (auto _: state1) {
        Sign1(state[1], Rout, keys[1].sk, m, L);
    }
}


void MultiSig2_SignAgg1(benchmark::State &state1) {
    Setup();
    vector<keyPair> keys(N + 1);
    for (int i = 1; i <= N; ++i)
        keys[i] = KeyGen();
    vector<ECP> L(N + 1);
    for (int i = 1; i <= N; ++i)
        L[i] = keys[i].pk;
    vector<vector<mpz_class>> state(N + 1);
    vector<vector<ECP>> out(N + 1);
    for (int i = 1; i <= N; ++i) {
        parSig sig = Sign();
        state[i] = sig.state;
        out[i] = sig.out;
    }
    vector<ECP> Rout = SignAgg(out);

    mpz_class m = 0x1e23b0dbf852148cfb0d8f3d2f4602a172a93666713b92e994432a111f11be18_mpz;
    vector<mpz_class> out1(N + 1, 1);
    schnorr sig1;
    for (int i = 1; i <= N; ++i) {
        sig1 = Sign1(state[i], Rout, keys[i].sk, m, L);
        out1[i] = sig1.s;
    }
    for (auto _: state1) {
        SignAgg1(out1);
    }
}


void MultiSig2_Ver(benchmark::State &state1) {
    Setup();
    vector<keyPair> keys(N + 1);
    for (int i = 1; i <= N; ++i)
        keys[i] = KeyGen();
    vector<ECP> L(N + 1);
    for (int i = 1; i <= N; ++i)
        L[i] = keys[i].pk;
    vector<vector<mpz_class>> state(N + 1);
    vector<vector<ECP>> out(N + 1);
    for (int i = 1; i <= N; ++i) {
        parSig sig = Sign();
        state[i] = sig.state;
        out[i] = sig.out;
    }
    vector<ECP> Rout = SignAgg(out);

    mpz_class m = 0x1e23b0dbf852148cfb0d8f3d2f4602a172a93666713b92e994432a111f11be18_mpz;
    vector<mpz_class> out1(N + 1, 1);
    schnorr sig1;
    for (int i = 1; i <= N; ++i) {
        sig1 = Sign1(state[i], Rout, keys[i].sk, m, L);
        out1[i] = sig1.s;
    }
    mpz_class s = SignAgg1(out1);

    ECP  X_tilde = KeyAgg(L);
    schnorr sig = Sign11(sig1.R, s);

    for (auto _: state1) {
        Ver(X_tilde, m, sig);
    }
}


// 注册基准测试
BENCHMARK(MultiSig2_Setup)->MinTime(1.0);
BENCHMARK(MultiSig2_KeyGen);
BENCHMARK(MultiSig2_Sign);
BENCHMARK(MultiSig2_SignAgg);
BENCHMARK(MultiSig2_Sign1);
BENCHMARK(MultiSig2_SignAgg1)->Iterations(100);
BENCHMARK(MultiSig2_Ver)->MinTime(1.0);


// 基准测试的入口
BENCHMARK_MAIN();


//int main() {
//    MultiSig2();
//    return 0;
//}
