#include "../include/Serializer.h"


std::string Package_to_str(TransmissionPackage pkg) {
    std::ostringstream oss;
    oss << to_string(pkg.pp.n) << "#"
        << to_string(pkg.pp.tm) << "#"
        << mpz_to_str(pkg.pp.q) << "#"
        << ECP2_to_str(pkg.pp.P2) << "#"
        << mpz_to_str(pkg.pp.g) << "#"
        << mpz_to_str(pkg.pp.beta) << "#"
        << ECP2Arr_to_str(pkg.pp.PK) << "#"

        << mpz_to_str(pkg.M) << "#"
        << to_string(pkg.t) << "#"

        << mpz_to_str(pkg.uav.ID) << "#"
        << mpzArr_to_str(pkg.uav.c1) << "#"
        << mpzArr_to_str(pkg.uav.c2) << "#"
        << ECP2Arr_to_str(pkg.uav.PK);
    return oss.str();
}


TransmissionPackage str_to_Package(string str) {

    TransmissionPackage pkg;
    std::vector<std::string> fields;
    size_t start = 0, end;
    while ((end = str.find('#', start)) != std::string::npos) {
        fields.push_back(str.substr(start, end - start));
        start = end + 1;
    }
    fields.push_back(str.substr(start));
    if (fields.size() != 13) {
        throw std::runtime_error("Invalid transmission package format.");
    }
    pkg.pp.n = stoi(fields[0]);
    pkg.pp.tm = stoi(fields[1]);
    pkg.pp.q = str_to_mpz(fields[2]);
    pkg.pp.P2 = str_to_ECP2(fields[3]);
    pkg.pp.g = str_to_mpz(fields[4]);
    pkg.pp.beta = str_to_mpz(fields[5]);
    pkg.pp.PK = str_to_ECP2Arr(fields[6]);

    pkg.M = str_to_mpz(fields[7]);
    pkg.t = stoi(fields[8]);

    pkg.uav.ID = str_to_mpz(fields[9]);
    pkg.uav.c1 = str_to_mpzArr(fields[10]);
    pkg.uav.c2 = str_to_mpzArr(fields[11]);
    pkg.uav.PK = str_to_ECP2Arr(fields[12]);

    return pkg;
}

void showPackage(TransmissionPackage pkg) {
    cout << "===== Params =====" << endl;
    cout << "n: " << pkg.pp.n << endl;
    cout << "tm: " << pkg.pp.tm << endl;
    cout << "q: ";
    show_mpz(pkg.pp.q.get_mpz_t());
    cout << "P2: ";
    ECP2_output(&pkg.pp.P2);   // 你项目里应有类似 ECP_output 的 G2 输出函数
    cout << "g: ";
    show_mpz(pkg.pp.g.get_mpz_t());
    cout << "beta: ";
    show_mpz(pkg.pp.beta.get_mpz_t());
    cout << "PK (" << pkg.pp.PK.size() << " items):" << endl;
    for (size_t i = 0; i < pkg.pp.PK.size(); i++) {
        cout << "  PK[" << i << "]: ";
        ECP2_output(&pkg.pp.PK[i]);
    }

    cout << "M: ";
    show_mpz(pkg.M.get_mpz_t());
    cout << "t: " << pkg.t << endl;
    cout << "uav.ID: ";
    show_mpz(pkg.uav.ID.get_mpz_t());

    cout << "uav.c1 (" << pkg.uav.c1.size() << " items):" << endl;
    for (size_t i = 0; i < pkg.uav.c1.size(); i++) {
        show_mpz(pkg.uav.c1[i].get_mpz_t());
    }
    cout << "uav.c2 (" << pkg.uav.c2.size() << " items):" << endl;
    for (size_t i = 0; i < pkg.uav.c2.size(); i++) {
        show_mpz(pkg.uav.c2[i].get_mpz_t());
    }
    cout << "uav.PK (" << pkg.uav.PK.size() << " items):" << endl;
    for (size_t i = 0; i < pkg.uav.PK.size(); i++) {
        ECP2_output(&pkg.uav.PK[i]);
    }
    cout << "==================" << endl;
}



std::string parSig_to_str(const parSig &sig) {
    std::ostringstream oss;
    oss << mpz_to_str(sig.cj) << "#"
        << ECP_to_str(sig.sig) << "#"
        << mpz_to_str(sig.ID);
    return oss.str();
}

parSig str_to_parSig(const std::string &str) {
    parSig sig;

    std::vector<std::string> fields;
    size_t start = 0, end;

    while ((end = str.find('#', start)) != std::string::npos) {
        fields.push_back(str.substr(start, end - start));
        start = end + 1;
    }
    fields.push_back(str.substr(start));

    if (fields.size() != 3) {
        throw std::runtime_error("Invalid parSig format.");
    }

    sig.cj = str_to_mpz(fields[0]);
    sig.sig = str_to_ECP(fields[1]);
    sig.ID = str_to_mpz(fields[2]);

    return sig;
}


void showParSig(parSig sig) {
    std::cout << "===== parSig =====" << std::endl;

    std::cout << "cj: ";
    show_mpz(sig.cj.get_mpz_t());

    std::cout << "sig: ";
    ECP_output(&sig.sig);

    std::cout << "ID: ";
    show_mpz(sig.ID.get_mpz_t());

    std::cout << "==================" << std::endl;
}

std::string Sigma_to_str(const Sigma &sg) {
    std::ostringstream oss;

    oss << mpzArr_to_str(sg.aux) << "#"
        << ECPArr_to_str(sg.sig) << "#"
        << mpzArr_to_str(sg.IDs);

    return oss.str();
}

Sigma str_to_Sigma(const std::string &str) {
    Sigma sg;

    std::vector<std::string> fields;
    size_t start = 0, end;

    while ((end = str.find('#', start)) != std::string::npos) {
        fields.push_back(str.substr(start, end - start));
        start = end + 1;
    }
    fields.push_back(str.substr(start));

    if (fields.size() != 3) {
        throw std::runtime_error("Invalid Sigma format.");
    }

    sg.aux = str_to_mpzArr(fields[0]);
    sg.sig = str_to_ECPArr(fields[1]);
    sg.IDs = str_to_mpzArr(fields[2]);

    return sg;
}

void showSigma(Sigma sg) {
    cout << "===== Sigma =====" << endl;

    cout << "aux (" << sg.aux.size() << " items):" << endl;
    for (size_t i = 0; i < sg.aux.size(); i++) {
        cout << "  aux[" << i << "]: ";
        show_mpz(sg.aux[i].get_mpz_t());
    }

    cout << "sig (" << sg.sig.size() << " items):" << endl;
    for (size_t i = 0; i < sg.sig.size(); i++) {
        cout << "  sig[" << i << "]: ";
        ECP_output(&sg.sig[i]);   // 和你 showPackage 的风格一致
    }

    cout << "IDs (" << sg.IDs.size() << " items):" << endl;
    for (size_t i = 0; i < sg.IDs.size(); i++) {
        cout << "  IDs[" << i << "]: ";
        show_mpz(sg.IDs[i].get_mpz_t());
    }

    cout << "=================" << endl;
}



// ----------------------------------------------------------------------------

std::string mpz_to_str(const mpz_class &value) {
    return value.get_str(16);
}

mpz_class str_to_mpz(const string &str) {
    return mpz_class(str, 16);
}

std::string ECP_to_str(ECP ecp) {
    BIG x, y;// 每个坐标的长度是96个字节
    ECP_get(x, y, &ecp);
    return mpz_to_str(BIG_to_mpz(x)) + "," + mpz_to_str(BIG_to_mpz(y));
}

ECP str_to_ECP(const std::string &str) {
    ECP ecp;
    int pos = str.find(",");
    BIG x, y;
    str_to_BIG(str.substr(0, pos), x);
    str_to_BIG(str.substr(pos + 1), y);
    ECP_set(&ecp, x, y);
    return ecp;
}

std::string ECP2_to_str(ECP2 ecp2, bool compressed) {
    char buffer[2 * 48 * 2];
    octet S;
    S.val = buffer;
    S.max = sizeof(buffer);
    S.len = 0;
    ECP2_toOctet(&S, const_cast<ECP2 *>(&ecp2), compressed);
    std::string hex_string;
    for (int i = 0; i < S.len; i++) {
        char hex[3];
        sprintf(hex, "%02X", (unsigned char) S.val[i]);
        hex_string.append(hex);
    }
    return hex_string;
}

ECP2 str_to_ECP2(const std::string &hex_string) {
    ECP2 ecp2;
    size_t len = hex_string.length() / 2;
    char buffer[len];
    for (size_t i = 0; i < len; i++) {
        sscanf(hex_string.substr(i * 2, 2).c_str(), "%2hhX", &buffer[i]);
    }
    octet S;
    S.val = buffer;
    S.max = len;
    S.len = len;
    if (ECP2_fromOctet(&ecp2, &S) != 1) {
        std::cerr << "Invalid ECP2 point representation." << std::endl;
    }
    return ecp2;
}


std::string FP12_to_str(FP12 fp12) {
    char buffer[24 * 48];
    octet S;
    S.val = buffer;
    S.max = sizeof(buffer);
    S.len = 0;
    FP12_toOctet(&S, const_cast<FP12 *>(&fp12));
    std::string hex_string;
    for (int i = 0; i < S.len; i++) {
        char hex[3];
        sprintf(hex, "%02X", (unsigned char) S.val[i]);
        hex_string.append(hex);
    }
    return hex_string;
}

FP12 str_to_FP12(const std::string &hex_string) {
    FP12 fp12;
    size_t len = hex_string.length() / 2;
    char buffer[len];
    for (size_t i = 0; i < len; i++) {
        sscanf(hex_string.substr(i * 2, 2).c_str(), "%2hhX", &buffer[i]);
    }
    octet S;
    S.val = buffer;
    S.max = len;
    S.len = len;
    FP12_fromOctet(&fp12, &S);
    return fp12;
}

std::string mpzArr_to_str(const std::vector<mpz_class> &mpzs) {
    string str;
    for (int i = 0; i < mpzs.size(); i++) {
        str += mpzs[i].get_str() + ",";
    }
    return str;
}

std::vector<mpz_class> str_to_mpzArr(const std::string &str) {
    vector<mpz_class> mpzs;
    stringstream ss(str);
    string item;
    while (getline(ss, item, ',')) {
        mpzs.push_back(mpz_class(item));
    }
    return mpzs;
}

std::string ECPArr_to_str(const std::vector<ECP> &ecps) {
    std::ostringstream oss;
    for (size_t i = 0; i < ecps.size(); ++i) {
        if (i != 0) oss << ";";
        oss << ECP_to_str(ecps[i]);
    }
    return oss.str();
}

std::vector<ECP> str_to_ECPArr(const std::string &str) {
    std::vector<ECP> ecps;
    size_t start = 0;
    size_t end = str.find(';');
    while (end != std::string::npos) {
        ecps.emplace_back(str_to_ECP(str.substr(start, end - start))); // 直接构造 ECP
        start = end + 1;
        end = str.find(';', start);
    }
    if (start < str.size()) {
        ecps.emplace_back(str_to_ECP(str.substr(start)));
    }
    return ecps;
}


std::string ECP2Arr_to_str(const std::vector<ECP2> &ecp2s, bool compressed) {
    std::ostringstream oss;
    for (size_t i = 0; i < ecp2s.size(); ++i) {
        if (i != 0) oss << ";";
        oss << ECP2_to_str(ecp2s[i], compressed);
    }
    return oss.str();
}

vector<ECP2> str_to_ECP2Arr(const std::string &str) {
    std::vector<ECP2> ecp2s;
    size_t start = 0;
    size_t end = str.find(';');
    while (end != std::string::npos) {
        ecp2s.emplace_back(str_to_ECP2(str.substr(start, end - start))); // 直接构造 ECP
        start = end + 1;
        end = str.find(';', start);
    }
    if (start < str.size()) {
        ecp2s.emplace_back(str_to_ECP2(str.substr(start)));
    }
    return ecp2s;
}