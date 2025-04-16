#include <pair_BLS12381.h>
#include <randapi.h>
#include "gmpxx.h"
#include <iostream>

using namespace B384_58;
using namespace BLS12381;
using namespace core;
using namespace std;


int main() {
    mpz_t a;
    mpz_init(a);
    mpz_set_str(a, "123456789012345678901234567890", 10);
    mpz_mul(a, a, a);
    gmp_printf("x = %Zd\n",a);
    mpz_clear(a);

    // 定义两个大整数（C++ 封装类型）
    mpz_class aa("123456789012345678901234567890");
    mpz_class bb("987654321098765432109876543210");
    mpz_class sum = aa + bb;

    gmp_printf("%Zx", aa.get_mpz_t());
    cout << endl;
    gmp_printf("%Zx", bb.get_mpz_t());
    cout << endl;
    gmp_printf("%Zx", sum.get_mpz_t());
    cout << endl;

    BIG x;
    BIG_rcopy(x, CURVE_Order);
    BIG_output(x);   // 输出 z
    printf("\n");
    return 0;
}
