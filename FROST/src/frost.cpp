#include "../include/frost.h"

gmp_randstate_t state;

#define N 128
#define T 128
#define LEN 2
#define INDEX 1

void InitializeCoefficients(std::vector<mpz_class> &coefficients) {
    coefficients.clear();
    for (int i = 0; i < T; i++) {
        mpz_class ai = rand_mpz(state);
        coefficients.push_back(ai);
    }
}

Params Setup(std::vector<mpz_class> &coefficients) {
    initState(state);
    InitializeCoefficients(coefficients);
    Params params;
    BIG order;
    BIG_rcopy(order, CURVE_Order);
    params.p = BIG_to_mpz(order);
    ECP_generator(&params.P1);
    ECP_generator(&params.PK);
    ECP_mul(params.PK, coefficients[0]);
    return params;
}

User KeyGen(const Params &params, int index, const std::vector<mpz_class> &coefficients) {
    initState(state);
    User user;
    user.i = index;
    user.si = computePoly(coefficients, index, params.p);
    ECP_generator(&user.Yi);
    ECP_mul(user.Yi, user.si);
    return user;
}

std::vector<std::vector<PairDE>> PreProcess(Params &params, std::vector<User> &users) {
    std::vector<std::vector<PairDE>> processedList;
    for (int i = 0; i < N; i++) {
        std::vector<PairDE> userPairs;
        for (int j = 0; j < LEN; j++) {
            PairDE pair;
            ECP Di, Ei;
            mpz_class di = rand_mpz(state);
            ECP_copy(&Di, &params.P1);
            ECP_mul(Di, di);
            users[i].di.push_back(di);
            users[i].Di.push_back(Di);

            mpz_class ei = rand_mpz(state);
            ECP_copy(&Ei, &params.P1);
            ECP_mul(Ei, ei);
            users[i].ei.push_back(ei);
            users[i].Ei.push_back(Ei);

            pair.Di = Di;
            pair.Ei = Ei;
            userPairs.push_back(pair);
        }
        processedList.push_back(userPairs);
    }
    return processedList;
}

std::vector<PairDE> SelectSigner(const std::vector<User> &users) {
    std::vector<PairDE> selectedSigners;
    for (int i = 0; i < T; i++) {
        PairDE pair;
        pair.Di = users[i].Di[INDEX];
        pair.Ei = users[i].Ei[INDEX];
        selectedSigners.push_back(pair);
    }
    return selectedSigners;
}

mpz_class H1(int index, mpz_class message, std::vector<PairDE> &B) {
    octet hash = getOctet(49);
    octet temp = getOctet(49);
    for (auto &pair: B) {
        ECP_toOctet(&temp, &pair.Di, true);
        concatOctet(&hash, &temp);
        ECP_toOctet(&temp, &pair.Ei, true);
        concatOctet(&hash, &temp);
    }
    message += index;
    temp = mpzToOctet(message);
    concatOctet(&hash, &temp);
    BIG order, ret;
    BIG_rcopy(order, CURVE_Order);
    hashZp256(ret, &hash, order);

    free(hash.val);
    free(temp.val);

    return BIG_to_mpz(ret);
}

mpz_class H2(ECP R, ECP Y, mpz_class message) {
    octet hash = getOctet(49);
    octet temp = getOctet(49);
    ECP_toOctet(&temp, &R, true);
    concatOctet(&hash, &temp);
    ECP_toOctet(&temp, &Y, true);
    concatOctet(&hash, &temp);
    temp = mpzToOctet(message);
    concatOctet(&hash, &temp);
    BIG order, ret;
    BIG_rcopy(order, CURVE_Order);
    hashZp256(ret, &hash, order);

    free(hash.val);
    free(temp.val);

    return BIG_to_mpz(ret);
}

mpz_class ComputeLagrangeCoefficient(const std::vector<int> &x, int index, mpz_class modulus) {
    mpz_class result = 1;
    for (size_t i = 0; i < x.size(); ++i) {
        if (i == index) continue;
        mpz_class numerator = (-x[i] % modulus + modulus) % modulus;
        mpz_class denominator = ((x[index] - x[i]) % modulus + modulus) % modulus;
        mpz_class denominatorInverse = invert_mpz(denominator, modulus);
        result = (result * (numerator * denominatorInverse) % modulus) % modulus;
    }
    return result;
}

ECP ComputeR(std::vector<PairDE> &B, mpz_class message) {
    ECP R;
    ECP_inf(&R);
    for (size_t i = 0; i < B.size(); i++) {
        mpz_class phi = H1(i, message, B);
        ECP temp;
        ECP_copy(&temp, &B[i].Ei);
        ECP_mul(temp, phi);
        ECP_add(&temp, &B[i].Di);
        ECP_add(&R, &temp);
    }
    return R;
}

mpz_class Sign(const User &user, const Params &params, mpz_class message, std::vector<PairDE> &B) {
    ECP R = ComputeR(B, message);
    mpz_class c = H2(R, params.PK, message);
    std::vector<int> x;
    for (int i = 0; i < T; i++) {
        x.push_back(i);
    }
    mpz_class lambda = ComputeLagrangeCoefficient(x, user.i, params.p);
    mpz_class phi = H1(user.i, message, B);
    return (user.di[INDEX] + user.ei[INDEX] * phi + lambda * user.si * c) % params.p;
}

Sigma Aggregate(const std::vector<mpz_class> &zValues, const Params &params, mpz_class message, std::vector<PairDE> &B,
                const std::vector<User> &users) {
    Sigma sigma;
    sigma.z = 0;
    for (const auto &z: zValues) {
        sigma.z = (sigma.z + z) % params.p;
    }
    sigma.R = ComputeR(B, message);
    return sigma;
}

bool Verify(Sigma &sigma, Params &params, mpz_class message) {
    ECP left;
    ECP_generator(&left);
    ECP_mul(left, sigma.z);
    ECP right;
    ECP_copy(&right, &params.PK);
    mpz_class c = H2(sigma.R, right, message);
    ECP_mul(right, c);
    ECP_add(&right, &sigma.R);
    return ECP_equals(&left, &right);
}

void TestFROST() {
    mpz_class message = 123456789;
    initState(state);

    std::vector<mpz_class> coefficients;
    Params params = Setup(coefficients);

    std::vector<User> users;
    for (int i = 0; i < N; i++) {
        users.push_back(KeyGen(params, i, coefficients));
    }

    PreProcess(params, users);

    std::vector<PairDE> selectedSigners = SelectSigner(users);

    std::vector<mpz_class> zValues;
    for (int i = 0; i < T; i++) {
        zValues.push_back(Sign(users[i], params, message, selectedSigners));
    }

    Sigma sigma = Aggregate(zValues, params, message, selectedSigners, users);

    bool result = Verify(sigma, params, message);
    std::cout << "Verify result: " << result << std::endl;
}

//int main() {
//    TestFROST();
//    return 0;
//}


#include "benchmark/benchmark.h"

static void FROST_Setup(benchmark::State &state1) {
    mpz_class message = 123456789;
    initState(state);
    std::vector<mpz_class> coefficients;
    for (auto _: state1) {
        Setup(coefficients);
    }
}

static void FROST_KeyGen(benchmark::State &state1) {

    mpz_class message = 123456789;
    initState(state);

    std::vector<mpz_class> coefficients;
    Params params = Setup(coefficients);

    for (auto _: state1) {
        KeyGen(params, N, coefficients);
    }
}

static void FROST_PreProcess(benchmark::State &state1) {

    mpz_class message = 123456789;
    initState(state);

    std::vector<mpz_class> coefficients;
    Params params = Setup(coefficients);

    std::vector<User> users;
    for (int i = 0; i < N; i++) {
        users.push_back(KeyGen(params, i, coefficients));
    }

    for (auto _: state1) {
        PreProcess(params, users);

    }
}

static void FROST_SelectSigner(benchmark::State &state1) {

    mpz_class message = 123456789;
    initState(state);

    std::vector<mpz_class> coefficients;
    Params params = Setup(coefficients);

    std::vector<User> users;
    for (int i = 0; i < N; i++) {
        users.push_back(KeyGen(params, i, coefficients));
    }
    PreProcess(params, users);
    for (auto _: state1) {
        SelectSigner(users);
    }
}

static void FROST_Sign(benchmark::State &state1) {

    mpz_class message = 123456789;
    initState(state);

    std::vector<mpz_class> coefficients;
    Params params = Setup(coefficients);

    std::vector<User> users;
    for (int i = 0; i < N; i++) {
        users.push_back(KeyGen(params, i, coefficients));
    }

    PreProcess(params, users);

    std::vector<PairDE> selectedSigners = SelectSigner(users);


    for (auto _: state1) {
        Sign(users[T - 1], params, message, selectedSigners);
    }
}

static void FROST_Aggregate(benchmark::State &state1) {

    mpz_class message = 123456789;
    initState(state);

    std::vector<mpz_class> coefficients;
    Params params = Setup(coefficients);

    std::vector<User> users;
    for (int i = 0; i < N; i++) {
        users.push_back(KeyGen(params, i, coefficients));
    }

    PreProcess(params, users);

    std::vector<PairDE> selectedSigners = SelectSigner(users);

    std::vector<mpz_class> zValues;
    for (int i = 0; i < T; i++) {
        zValues.push_back(Sign(users[i], params, message, selectedSigners));
    }


    for (auto _: state1) {
        Aggregate(zValues, params, message, selectedSigners, users);

    }
}

static void FROST_Verify(benchmark::State &state1) {
    mpz_class message = 123456789;
    initState(state);

    std::vector<mpz_class> coefficients;
    Params params = Setup(coefficients);

    std::vector<User> users;
    for (int i = 0; i < N; i++) {
        users.push_back(KeyGen(params, i, coefficients));
    }

    PreProcess(params, users);

    std::vector<PairDE> selectedSigners = SelectSigner(users);

    std::vector<mpz_class> zValues;
    for (int i = 0; i < T; i++) {
        zValues.push_back(Sign(users[i], params, message, selectedSigners));
    }

    Sigma sigma = Aggregate(zValues, params, message, selectedSigners, users);

    for (auto _: state1) {
        Verify(sigma, params, message);
    }
}



// 注册基准测试
BENCHMARK(FROST_Setup)->MinTime(1.0);
BENCHMARK(FROST_KeyGen);
BENCHMARK(FROST_PreProcess);
BENCHMARK(FROST_SelectSigner);
BENCHMARK(FROST_Sign);
BENCHMARK(FROST_Aggregate)->Iterations(100);
BENCHMARK(FROST_Verify)->MinTime(1.0);


// 基准测试的入口
BENCHMARK_MAIN();