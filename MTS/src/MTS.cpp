#include "../include/MTS.h"
#include <cassert>

gmp_randstate_t state;
#define N 129
#define T 128

const mpz_class q("0x73EDA753299D7D483339D80809A1D80553BDA402FFFE5BFEFFFFFFFF00000001");
vector<int> w(N, 1);
int W = accumulate(w.begin(), w.end(), 0);
CRS crs;
PubParam ppt;


// ----------------------------------------------- Setup -----------------------------------------------
void Setup() {
    BIG order;
    BIG_rcopy(order, CURVE_Order);
    ppt.p = BIG_to_mpz(order);
    ECP_generator(&ppt.P1);
    ECP2_generator(&ppt.P2);
}


// ----------------------------------------------- KGen -----------------------------------------------
vector<mpz_class> F(mpz_class s, int Wi) {
    gmp_randstate_t seed;
    gmp_randinit_default(seed);
    gmp_randseed_ui(seed, s.get_si());
    vector<mpz_class> ret;

    for (int i = 0; i < Wi; i++) {
        ret.push_back(rand_mpz(seed));
    }
    return ret;
}

KeyPairs getKeyPairs(mpz_class s, int Wi) {
    vector<mpz_class> sk = F(s, Wi);
    vector<ECP> PK(Wi, ppt.P1);
    for (int i = 0; i < Wi; i++) {
        ECP_mul(PK[i], sk[i]);
    }
    KeyPairs keyPairs;
    keyPairs.sk = sk;
    keyPairs.PK = PK;
    return keyPairs;
}

void clearCRS(){
    crs.sk_star.clear();
    crs.sk_bar.clear();
    crs.pk_bar.clear();
}

void KGen() {
    initState(state);
    clearCRS();
    for (int i = 0; i < N; i++) {
        KeyPair kp;
        kp.sk = rand_mpz(state);
        crs.sk_star.push_back(kp.sk);
        KeyPairs kps = getKeyPairs(kp.sk, w[i]);
        for (int j = 0; j < w[i]; j++) {
            crs.sk_bar.push_back(kps.sk[j]);
            crs.pk_bar.push_back(kps.PK[j]);
        }
    }
}

// ----------------------------------------------- UGen_on -----------------------------------------------

mpz_class H(ECP g, ECP gx, ECP gr) {
    octet ct = getOctet(96);
    octet temp = getOctet(96);
    ECP_toOctet(&temp, &g, false);
    concatOctet(&ct, &temp);
    ECP_toOctet(&temp, &gx, false);
    concatOctet(&ct, &temp);
    ECP_toOctet(&temp, &gr, false);
    concatOctet(&ct, &temp);
    BIG ret, q;
    BIG_rcopy(q, CURVE_Order);
    hashZp256(ret, &ct, q);

    free(ct.val);
    free(temp.val);

    return BIG_to_mpz(ret);
}

Pai createNIZKPoK(ECP gx, mpz_class x) {
    initState(state);
    mpz_class r = rand_mpz(state);
    ECP gr;
    ECP_generator(&gr);
    ECP_mul(gr, r);
    mpz_class c = H(ppt.P1, gx, gr);
    mpz_class z = (r + c * x) % ppt.p;
    Pai pai;
    pai.gr = gr;
    pai.c = c;
    pai.z = z;
    pai.gx = gx;
    return pai;
}

bool verifyNIZKPoK(ECP gx, Pai pai) {
    bool ret = true;
    mpz_class c1 = H(ppt.P1, gx, pai.gr);
    ret = ret && (c1 == pai.c);
    ECP left, right;
    ECP_generator(&left);
    ECP_mul(left, pai.z);
    ECP_copy(&right, &gx);
    ECP_mul(right, pai.c);
    ECP_add(&right, &pai.gr);
    return ret && (ECP_equals(&left, &right));
}

// 初始化 coffs
void setCoffs() {
    initState(state);
    vector<mpz_class> x;
    for (int i = 0; i < N; i++) {
        mpz_class idx = i;
        x.push_back(idx);
    }
    crs.coffs = getLagrangeCoffs(x, crs.sk_bar, ppt.p);
}

vector<ECP> getEvals0() {
    vector<mpz_class> x;
    vector<ECP> evals0(W - T + 1, ppt.P1);
    for (int i = -(W - T); i <= 0; i++) {
        mpz_class idx = (i + ppt.p) % ppt.p;
        x.push_back(idx);
    }
    for (int i = 0; i <= W - T; i++) {
        mpz_class fx = computePoly(crs.coffs, x[i], ppt.p);
        ECP_mul(evals0[i], fx);//evals0[W-T] = g^f(0)
    }
    return evals0;
}

Rho UGen_on(mpz_class sk, int w) {
    setCoffs();
    vector<ECP> evals0 = getEvals0();
    vector<ECP> evals1(W - T, ppt.P1);
    mpz_class k = rand_mpz(state);
    for (int i = 0; i < W - T; i++) {
        ECP_copy(&evals1[i], &evals0[i]);
        ECP_mul(evals1[i], k);
    }
    vector<Pai> pais;
    KeyPairs kps = getKeyPairs(sk, w);
    for (int i = 0; i < w; i++) {
        Pai pai = createNIZKPoK(kps.PK[i], kps.sk[i]);
        pais.push_back(pai);
    }
    Rho rho;
    rho.evals1 = evals1;
    ECP2_generator(&rho.g2k);
    ECP2_mul(rho.g2k, k);
    rho.pais = pais;
    return rho;
}

// ----------------------------------------------- UGen_off -----------------------------------------------
mpz_class H1(vector<ECP2> Qs, ECP2 g2k) {
    octet ct = getOctet(193);
    octet temp = getOctet(193);
    for (int i = 0; i < Qs.size(); i++) {
        ECP2_toOctet(&temp, &Qs[i], false);
        concatOctet(&ct, &temp);
    }
    ECP2_toOctet(&temp, &g2k, false);
    concatOctet(&ct, &temp);
    BIG ret, q;
    BIG_rcopy(q, CURVE_Order);
    hashZp256(ret, &ct, q);

    free(ct.val);
    free(temp.val);

    return BIG_to_mpz(ret);
}

Params UGen_off(vector<Rho> rho_bar) {
    vector<ECP> evals0 = getEvals0();
    // --------------------------------- 构造Q
    vector<mpz_class> alpha;
    vector<int> Q;
    for (int i = 0; i < N; i++) {
        vector<ECP> evals1_i = rho_bar[i].evals1;
        ECP2 g2k_i = rho_bar[i].g2k;
        vector<Pai> pais_i = rho_bar[i].pais;
        // --------------------------------- 验证NIZKPoK
        bool flag = true;
        for (int j = 0; j < w[i]; j++) {
            flag = flag && verifyNIZKPoK(pais_i[j].gx, pais_i[j]);
        }
        // ---------- 验证e(evals_{1,i},g_2)=e(evals_{0,i},g_2^{k_i}) -------------------
        for (int j = 0; j < W - T; j++) {
            FP12 left, right;
            left = e(evals1_i[j], ppt.P2);
            right = e(evals0[j], g2k_i);
            flag = flag && (FP12_equals(&left, &right));
        }
        if (flag) Q.push_back(i);
    }
    // ------------------- 计算alpha = {H({g2^{ki}}_{i∈Q},g2^ki)}_{i∈Q} ----------------------
    vector<ECP2> g2k;
    for (int i = 0; i < Q.size(); i++) {
        g2k.push_back(rho_bar[i].g2k);
    }
    for (int i = 0; i < Q.size(); i++) {
        mpz_class alpha_i = H1(g2k, rho_bar[i].g2k);
        alpha.push_back(alpha_i);
    }

    // ------------------- 计算eval_1 = Π_{i=1}^Q evals_{1,i}^α_i ----------------------
    ECP zero;
    ECP_inf(&zero);
    vector<ECP> evals1(W - T, zero);
    for (int i = 0; i < W - T; i++) {
        for (int j = 0; j < Q.size(); j++) {
            ECP temp;
            ECP_copy(&temp, &rho_bar[j].evals1[i]);
            ECP_mul(temp, alpha[j]);
            ECP_add(&evals1[i], &temp);
        }
    }
    // --------------------------------- 返回 pp && VK ---------------------------------
    ECP2 VK1;
    ECP2_inf(&VK1);
    for (int i = 0; i < Q.size(); i++) {
        ECP2 temp;
        ECP2_copy(&temp, &rho_bar[i].g2k);
        ECP2_mul(temp, alpha[i]);
        ECP2_add(&VK1, &temp);
    }
    // 返回结果
    Params ret;
    ret.pp.evals0 = evals0;
    ret.pp.evals1 = evals1;
    ret.vk.VK0 = evals0[W - T];
    ret.vk.VK1 = VK1;
    return ret;
}

// ----------------------------------------------- Sign -----------------------------------------------

ECP2 H(mpz_class m) {
    ECP2 ret;
    ECP2_generator(&ret);
//    octet ct = getOctet(48);
//    ct = mpzToOctet(m);
//    ECP2_mapit(&ret, &ct);
    ECP2_mul(ret, m);
    return ret;
}

vector<ECP2> Sign(mpz_class s, mpz_class m, int w) {
    vector<mpz_class> sk = F(s, w);
    vector<ECP2> sigma(w, ppt.P2);
    ECP2 Hm = H(m);
    for (int i = 0; i < w; i++) {
        sigma[i] = Hm;
        ECP2_mul(sigma[i], sk[i]);
    }
    return sigma;
}

// ----------------------------------------------- Aggregate -----------------------------------------------

template <typename CurvePoint>
CurvePoint computeInnerProduct(const vector<CurvePoint>& points, const vector<mpz_class>& scalars) {
    assert(points.size() == scalars.size());
    CurvePoint result;
    if constexpr (is_same_v<CurvePoint, ECP>) {
        ECP_inf(&result);
    } else {
        ECP2_inf(&result);
    }
    for (size_t i = 0; i < points.size(); ++i) {
        CurvePoint temp = points[i];
        if constexpr (is_same_v<CurvePoint, ECP>) {
            ECP_mul(temp, scalars[i]);
            ECP_add(&result, &temp);
        } else {
            ECP2_mul(temp, scalars[i]);
            ECP2_add(&result, &temp);
        }
    }
    return result;
}


Sigma Aggregate(PP pp, vector<vector<ECP2>> sigma_bar, mpz_class m) {
    initState(state);
    // --------------------------------- 计算 <pki,r> 和 <σi,r> ---------------------------------
    vector<int> Q;
    int currIdx = 0;
    for (int i = 0; i < sigma_bar.size(); i++) {
        vector<mpz_class> ri(w[i], 0);
        for (int j = 0; j < w[i]; j++) {
            ri[j] = rand_mpz(state);
        }
        currIdx += w[i];
        vector<ECP> pki(crs.pk_bar.begin() + currIdx - w[i], crs.pk_bar.begin() + currIdx);
        ECP pki_r = computeInnerProduct(pki, ri);
        ECP2 sigmai_r = computeInnerProduct(sigma_bar[i], ri);
        FP12 left = e(pki_r, H(m));
        FP12 right = e(ppt.P1, sigmai_r);
        bool flag = FP12_equals(&left, &right);
        if (flag) Q.push_back(i);
    }
    // --------------------------------- 计算 σ0' 和 σ1'> ---------------------------------
    vector<mpz_class> x;
    for (int i = -(W - T); i <= T; i++) {
        if (i != 0) {
            mpz_class idx = (i + ppt.p) % ppt.p;
            x.push_back(idx);
        }
    }
    vector<mpz_class> lambda = getLagrangeBasis(x, q);
    vector<mpz_class> lambda_pub(lambda.begin(), lambda.begin() + (W - T));
    vector<mpz_class> lambda_par(lambda.begin() + (W - T), lambda.end());

    vector<ECP> B(pp.evals0.begin(), pp.evals0.end() - 1);
    vector<ECP> A = pp.evals1;

    ECP sigma01 = computeInnerProduct(B, lambda_pub);
    ECP sigma11 = computeInnerProduct(A, lambda_pub);

    vector<ECP2> sig;
    for (int i = 0; i < sigma_bar.size(); i++) {
        for (int j = 0; j < w[i]; j++) {
            sig.push_back(sigma_bar[i][j]);
        }
    }
    ECP2 sigma1 = computeInnerProduct(sig, lambda_par);

    Sigma sigma;
    sigma.sigma1 = sigma1;
    sigma.sigma01 = sigma01;
    sigma.sigma11 = sigma11;
    return sigma;
}

bool Verify(VK vk, Sigma sigma, mpz_class m) {
    ECP VK0;
    ECP2 VK1;
    ECP_copy(&VK0, &vk.VK0);
    ECP2_copy(&VK1, &vk.VK1);
    // 计算 vk0 / σ0'   如果N=T时，σ0'=P，此时vk0 / σ0' = vk0
    ECP vk0_sigma1;
    ECP_copy(&vk0_sigma1, &VK0);
    if (ECP_equals(&vk0_sigma1, &ppt.P1)) {
        ECP_copy(&vk0_sigma1, &ppt.P1);
    } else {
        ECP_sub(&vk0_sigma1, &sigma.sigma01);
    }

    FP12 left = e(vk0_sigma1, H(m));
    FP12 right = e(ppt.P1, sigma.sigma1);
    bool ret = FP12_equals(&left, &right);
//    cout << "验证结果" << ret << endl;

    left = e(sigma.sigma11, ppt.P2);
    right = e(sigma.sigma01, VK1);
    return ret && FP12_equals(&left, &right);
}


int findEnd() {
    int sum = 0;
    for (int i = w.size(); i > w.size() - T; --i) {
        sum += w[i - 1];
        if (sum == T) return i - 1; // 找到返回项数
    }
    return -1; // 未找到返回-1
}

int findBegin() {
    int sum = 0;
    for (int i = 0; i < w.size(); ++i) {
        sum += w[i];
        if (sum == T) return i + 1; // 找到返回项数
    }
    return -1; // 未找到返回-1
}


void testMTS() {
    Setup();
    // --------------------------------- 生成 sk_star,sk_bar,pk_bar -------------------
    KGen();
    // --------------------------------- 每个人执行 UGen_on ----------------------------
    mpz_class m = 0x666666_mpz;
    vector<Rho> rho_bar;
    for (int i = 0; i < N; i++) {
        Rho rho = UGen_on(crs.sk_star[i], w[i]);
        rho_bar.push_back(rho);
    }
    // --------------------------------- 执行 UGen_off ---------------------------------
    Params params = UGen_off(rho_bar);
    // --------------------------------- 执行 Sign ---------------------------------
    vector<vector<ECP2>> sigmas;
    int n = findBegin();
    cout << "n=" << n << endl;
    for (int i = 1; i <= n; i++) {
        vector<ECP2> parSig = Sign(crs.sk_star[i], m, w[i]);
        sigmas.push_back(parSig);
    }
    // --------------------------------- 聚合签名 Aggregate ---------------------------------
    // 错误并
    Sigma sigma = Aggregate(params.pp, sigmas, m);
    // --------------------------------- 验证签名 verify ---------------------------------
    bool res = Verify(params.vk, sigma, m);
    cout << "验证结果" << res << endl;
}






#include "benchmark/benchmark.h"

static void MTS_Setup(benchmark::State &state1) {
    for (auto _: state1) {
        Setup();
    }
}

static void MTS_KeyGen(benchmark::State &state) {
    Setup();
    for (auto _: state) {
        KGen();
    }
}

static void MTS_UGen_on(benchmark::State &state) {
    Setup();
    KGen();
    mpz_class m = 0x666666_mpz;
    vector<Rho> rho_bar;
    for (auto _: state) {
        UGen_on(crs.sk_star[T-1], w[T-1]);
    }
}


static void MTS_UGen_off(benchmark::State &state1) {
    Setup();
    KGen();
    mpz_class m = 0x666666_mpz;
    vector<Rho> rho_bar;
    for (int i = 0; i < N; i++) {
        Rho rho = UGen_on(crs.sk_star[i], w[i]);
        rho_bar.push_back(rho);
    }
    for (auto _: state1) {
        Params params = UGen_off(rho_bar);
    }
}

static void MTS_Sign(benchmark::State &state1) {
    Setup();
    KGen();
    mpz_class m = 0x666666_mpz;
    vector<Rho> rho_bar;
    for (int i = 0; i < N; i++) {
        Rho rho = UGen_on(crs.sk_star[i], w[i]);
        rho_bar.push_back(rho);
    }
    Params params = UGen_off(rho_bar);
    vector<vector<ECP2>> sigmas;
    for (auto _: state1) {
        Sign(crs.sk_star[0], m, w[0]);
    }
}


void MTS_Aggregate(benchmark::State &state1) {
    Setup();
    KGen();
    mpz_class m = 0x666666_mpz;
    vector<Rho> rho_bar;
    for (int i = 0; i < N; i++) {
        Rho rho = UGen_on(crs.sk_star[i], w[i]);
        rho_bar.push_back(rho);
    }
    Params params = UGen_off(rho_bar);
    vector<vector<ECP2>> sigmas;
    int n = findBegin();
    for (int i = 1; i <= n; i++) {
        vector<ECP2> parSig = Sign(crs.sk_star[i], m, w[i]);
        sigmas.push_back(parSig);
    }

    for (auto _: state1) {
        Aggregate(params.pp, sigmas, m);
    }
}


void MTS_Ver(benchmark::State &state1) {
    Setup();
    KGen();
    mpz_class m = 0x666666_mpz;
    vector<Rho> rho_bar;
    for (int i = 0; i < N; i++) {
        Rho rho = UGen_on(crs.sk_star[i], w[i]);
        rho_bar.push_back(rho);
    }
    Params params = UGen_off(rho_bar);
    vector<vector<ECP2>> sigmas;
    int n = findBegin();
    for (int i = 1; i <= n; i++) {
        vector<ECP2> parSig = Sign(crs.sk_star[i], m, w[i]);
        sigmas.push_back(parSig);
    }
    // 错误并
    Sigma sigma = Aggregate(params.pp, sigmas, m);
    for (auto _: state1) {
        Verify(params.vk, sigma, m);
    }
}


// 注册基准测试
BENCHMARK(MTS_Setup)->MinTime(1.0);
BENCHMARK(MTS_KeyGen);
BENCHMARK(MTS_UGen_on);
BENCHMARK(MTS_UGen_off);
BENCHMARK(MTS_Sign)->Iterations(888);
BENCHMARK(MTS_Aggregate);
BENCHMARK(MTS_Ver)->Iterations(88);


// 基准测试的入口
BENCHMARK_MAIN();


//int main() {
//    testMTS();
//    return 0;
//}