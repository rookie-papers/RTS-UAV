#include "benchmark/benchmark.h"
#include "../include/fussion.h"

csprng rng_test;
gmp_randstate_t state_test;

#define N 128
#define TM 128

static void BM_Setup(benchmark::State &state) {
    initState(state_test);
    initRNG(&rng_test);
    mpz_class alpha, M = 123456789;
    for (auto _: state) {
        Params pp = Setup(alpha, N, TM);
    }
}

static void BM_KeyGen(benchmark::State &state) {
    initState(state_test);
    initRNG(&rng_test);
    mpz_class alpha, M = 123456789;
    // setup
    Params pp = Setup(alpha, N, TM);
    UAV_h uavH;
    for (auto _: state) {
        vector<UAV> UAVs = KeyGen(pp, alpha, uavH);
    }
}

static void BM_Sign(benchmark::State &state) {
    initState(state_test);
    initRNG(&rng_test);
    mpz_class alpha, M = 123456789;
    Params pp = Setup(alpha, N, TM);
    UAV_h uavH;
    vector<UAV> UAVs = KeyGen(pp, alpha, uavH);
    int t = TM;
    vector<mpz_class> S;
    for (int i = 0; i < t; ++i) {
        S.push_back(UAVs[i].ID);
    }
    int i = 0;
    for (auto _: state) {
        Sign(pp, UAVs[i], t, M,S);
        i = (++i) % N;
    }
}


static void BM_Tran(benchmark::State &state) {
    initState(state_test);
    initRNG(&rng_test);
    mpz_class alpha, M = 123456789;
    Params pp = Setup(alpha, N, TM);
    UAV_h uavH;
    vector<UAV> UAVs = KeyGen(pp, alpha, uavH);
    int t = TM;
    mpz_class sk_v = rand_mpz(state_test);
    mpz_class PK_v = pow_mpz(pp.g, sk_v, pp.q);
    vector<mpz_class> S;
    vector<ECP2> PKs;
    for (int i = 0; i < t; ++i) {
        S.push_back(UAVs[i].ID);
        PKs.push_back(UAVs[i].PK[t-2]);
    }
    vector<parSig> sigmas = collectSig(pp, UAVs, t, M, S);
    for (auto _: state) {
        Sigma sig = AggSig(sigmas, pp, uavH, PK_v);
    }
}

static void BM_Verify(benchmark::State &state) {
    initState(state_test);
    initRNG(&rng_test);
    mpz_class alpha, M = 123456789;
    Params pp = Setup(alpha, N, TM);
    UAV_h uavH;
    vector<UAV> UAVs = KeyGen(pp, alpha, uavH);
    int t = TM;
    vector<mpz_class> S;
    vector<ECP2> PKs;
    for (int i = 0; i < t; ++i) {
        S.push_back(UAVs[i].ID);
        PKs.push_back(UAVs[i].PK[t-2]);
    }
    vector<parSig> sigmas = collectSig(pp, UAVs, t, M, S);
    mpz_class sk_v = rand_mpz(state_test);
    mpz_class PK_v = pow_mpz(pp.g, sk_v, pp.q);
    Sigma sig = AggSig(sigmas, pp, uavH, PK_v);
    for (auto _: state) {
        Verify(sig, sk_v, pp, M, t,PKs);
    }
}

static void BM_BatchVerify(benchmark::State &state) {
    initState(state_test);
    initRNG(&rng_test);
    mpz_class alpha, M = 123456789;
    Params pp = Setup(alpha, N, TM);
    UAV_h uavH;
    vector<UAV> UAVs = KeyGen(pp, alpha, uavH);
    int t = TM;
    vector<mpz_class> S;
    vector<ECP2> PKs;
    for (int i = 0; i < t; ++i) {
        S.push_back(UAVs[i].ID);
        PKs.push_back(UAVs[i].PK[t-2]);
    }
    vector<parSig> sigmas = collectSig(pp, UAVs, t, M, S);
    mpz_class sk_v = rand_mpz(state_test);
    mpz_class PK_v = pow_mpz(pp.g, sk_v, pp.q);
    Sigma sig = AggSig(sigmas, pp, uavH, PK_v);
    for (auto _: state) {
        BatchVerify(sig, sk_v, pp, M, t,PKs);
    }
}

void BM_hash(benchmark::State &state) {
    initRNG(&rng_test);
    BIG hash, q;
    BIG_rcopy(q, CURVE_Order);

    char buffer[193];
    octet oc = getOctet(193);
    octet ocs = getOctet(1);

    for (auto _:state){
        for (int i = 0; i < 1000; ++i){
            ECP2 P2 = randECP2(rng_test);
            ECP2_toOctet(&oc, &P2, false);
            ocs = concat_Octet(&ocs, &oc);
        }
        hashZp256(hash, &ocs, q);
    }
}

static void BM_SwarmSplitting(benchmark::State &state) {
    initState(state_test);
    initRNG(&rng_test);
    mpz_class alpha;
    Params pp = Setup(alpha, N, TM);
    UAV_h uavH;
    vector<UAV> UAVs = KeyGen(pp, alpha, uavH);
    vector<UAV> subSwarm;
    int subSwarmSize = N;
    for (int i = 0; i < subSwarmSize; ++i) {
        subSwarm.push_back(UAVs[i]);
    }
    for (auto _: state) {
        SwarmSplitting(pp, uavH, subSwarm);
    }
}

static void BM_SwarmSplittingOptimized(benchmark::State &state) {
    initState(state_test);
    initRNG(&rng_test);
    mpz_class alpha;
    Params pp = Setup(alpha, N, TM);
    UAV_h uavH;
    vector<UAV> UAVs = KeyGen(pp, alpha, uavH);
    vector<UAV> subSwarm;
    int subSwarmSize = N;
    for (int i = 0; i < subSwarmSize; ++i) {
        subSwarm.push_back(UAVs[i]);
    }
    for (auto _: state) {
        SwarmSplittingOptimized(pp, uavH, subSwarm);
    }
}

// 注册基准测试
BENCHMARK(BM_Setup);
BENCHMARK(BM_KeyGen);
BENCHMARK(BM_Sign);
BENCHMARK(BM_Tran);
BENCHMARK(BM_Verify);
BENCHMARK(BM_BatchVerify);
BENCHMARK(BM_hash);
BENCHMARK(BM_SwarmSplitting);
BENCHMARK(BM_SwarmSplittingOptimized);


// benchmark main
BENCHMARK_MAIN();



