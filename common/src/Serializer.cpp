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
        << ECP2Arr_to_str(pkg.uav.PK) << "#"
        << to_string(pkg.uav.serialNumber) << "#"
        << mpzArr_to_str(pkg.registeredIDs);
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
    if (fields.size() != 15) {
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
    pkg.uav.serialNumber = stoi(fields[13]);
    pkg.registeredIDs = str_to_mpzArr(fields[14]);

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



// 序列化： cj # sig # index
std::string parSig_to_str(const parSig &sig) {
    std::ostringstream oss;
    oss << mpz_to_str(sig.cj) << "#"
        << ECP_to_str(sig.sig) << "#"
        << sig.index;
    return oss.str();
}

// 反序列化
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
    try {
        sig.index = static_cast<short>(std::stoi(fields[2]));
    } catch (...) {
        throw std::runtime_error("Invalid index format in parSig.");
    }

    return sig;
}


void showParSig(parSig& sig) {
    cout << "--- Partial Signature ---" << endl;
    cout << "Index (SerialNumber): " << sig.index << endl;

    cout << "cj: ";
    show_mpz(sig.cj.get_mpz_t());

    cout << "sig (G1): ";
    // 注意：这里假设你有一个输出 G1 点的函数，就像 ECP2_output 一样
    // 如果没有，请替换为你项目中输出 ECP 类型的方法
    ECP_output(&sig.sig);
    cout << endl;
    cout << "-------------------------" << endl;
}

// 序列化 Sigma (适配 indices)
std::string Sigma_to_str(const Sigma &sg) {
    std::ostringstream oss;

    // 1. 序列化 Aux (mpz数组，保持原样)
    oss << mpzArr_to_str(sg.aux) << "#";

    // 2. 序列化 Sig (ECP数组，保持原样)
    oss << ECPArr_to_str(sg.sig) << "#";

    // 3. [修改] 序列化 indices (short数组 -> 逗号分隔字符串)
    for (size_t i = 0; i < sg.indices.size(); ++i) {
        oss << sg.indices[i];
        if (i != sg.indices.size() - 1) {
            oss << ",";
        }
    }

    return oss.str();
}

// 反序列化 Sigma (适配 indices)
Sigma str_to_Sigma(const std::string &str) {
    Sigma sg;

    std::vector<std::string> fields;
    size_t start = 0, end;

    // 按 '#' 分割字符串
    while ((end = str.find('#', start)) != std::string::npos) {
        fields.push_back(str.substr(start, end - start));
        start = end + 1;
    }
    fields.push_back(str.substr(start));

    if (fields.size() != 3) {
        throw std::runtime_error("Invalid Sigma format.");
    }

    // 1. 解析 Aux
    sg.aux = str_to_mpzArr(fields[0]);

    // 2. 解析 Sig
    sg.sig = str_to_ECPArr(fields[1]);

    // 3. [修改] 解析 indices (逗号分隔字符串 -> vector<short>)
    std::string indicesStr = fields[2];
    if (!indicesStr.empty()) {
        std::stringstream ss(indicesStr);
        std::string item;
        while (std::getline(ss, item, ',')) {
            if (!item.empty()) {
                // string -> int -> short
                sg.indices.push_back(static_cast<short>(std::stoi(item)));
            }
        }
    }

    return sg;
}

void showSigma(Sigma& sigma) {
    cout << "===== Aggregated Sigma =====" << endl;
    // 1. 打印 Indices (这是导致你报错的关键部分)
    cout << "Indices (" << sigma.indices.size() << " items): [ ";
    for (size_t i = 0; i < sigma.indices.size(); ++i) {
        cout << sigma.indices[i] << (i == sigma.indices.size() - 1 ? "" : ", ");
    }
    cout << " ]" << endl;

    // 2. 打印 Aux
    cout << "Aux (" << sigma.aux.size() << " items):" << endl;
    for (size_t i = 0; i < sigma.aux.size(); i++) {
        cout << "  aux[" << i << "]: ";
        show_mpz(sigma.aux[i].get_mpz_t());
    }

    // 3. 打印 Signatures
    cout << "Signatures (" << sigma.sig.size() << " items):" << endl;
    for (size_t i = 0; i < sigma.sig.size(); i++) {
        cout << "  sig[" << i << "]: ";
        ECP_output(&sigma.sig[i]); // 同样假设存在 ECP_output
        cout << endl; // 换行
    }
    cout << "============================" << endl;
}

// ----------------------------------------------------------------------------

std::string mpz_to_str(const mpz_class &value) {
    return value.get_str(16);
}

mpz_class str_to_mpz(const string &str) {
    return mpz_class(str, 16);
}

//std::string ECP_to_str(ECP ecp) {
//    BIG x, y;// 每个坐标的长度是96个字节
//    ECP_get(x, y, &ecp);
//    return mpz_to_str(BIG_to_mpz(x)) + "," + mpz_to_str(BIG_to_mpz(y));
//}
//
//ECP str_to_ECP(const std::string &str) {
//    ECP ecp;
//    int pos = str.find(",");
//    BIG x, y;
//    str_to_BIG(str.substr(0, pos), x);
//    str_to_BIG(str.substr(pos + 1), y);
//    ECP_set(&ecp, x, y);
//    return ecp;
//}

std::string ECP_to_str(ECP ecp) {
    char buffer[64];
    octet O;
    O.len = 0;
    O.max = sizeof(buffer);
    O.val = buffer;
    ECP_toOctet(&O, &ecp, true);
    std::string binaryData(O.val, O.len);
    return binToHex(binaryData);
}

ECP str_to_ECP(const std::string &str) {
    ECP ecp;
    std::string binaryData = hexToBin(str);
    char buffer[64];
    if(binaryData.size() > 64) throw std::runtime_error("ECP string too long");
    memcpy(buffer, binaryData.data(), binaryData.size());
    octet O;
    O.len = (int)binaryData.size();
    O.max = sizeof(buffer);
    O.val = buffer;
    if (ECP_fromOctet(&ecp, &O) != 1) {
        std::cerr << "[Error] Failed to deserialize ECP (point not on curve)." << std::endl;
        ECP_inf(&ecp);
    }
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


static std::string binToHex(const std::string& input) {
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();
    std::string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i) {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}

static std::string hexToBin(const std::string& input) {
    size_t len = input.length();
    if (len & 1) throw std::invalid_argument("Odd length");
    std::string output;
    output.reserve(len / 2);
    for (size_t i = 0; i < len; i += 2) {
        std::string byteString = input.substr(i, 2);
        char byte = (char)strtol(byteString.c_str(), NULL, 16);
        output.push_back(byte);
    }
    return output;
}