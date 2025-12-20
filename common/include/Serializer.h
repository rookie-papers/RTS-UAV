#include "Tools.h"
#include "../../RTS-websocket/include/RTS.h"

using namespace RTS_web;

/**
 * @struct TransmissionPackage
 * @brief A data structure used to encapsulate transmission information between entities.
 *
 * This structure contains public parameters, signature, keys, and other
 * essential elements used during a multi-party communication protocol.
 */
typedef struct {
    Params pp;
    mpz_class M;        ///< Secret key of participant i
    int t;            ///< Public key of the original signer
    UAV uav;
    vector<mpz_class> registeredIDs;
//    long long timestamp;   ///< Current timestamp (ms)
} TransmissionPackage;

/**
 * @brief Serializes a TransmissionPackage into a string for transmission.
 * @param pkg The TransmissionPackage to serialize.
 * @return A string representation of the package.
 */
std::string Package_to_str(TransmissionPackage pkg);

/**
 * @brief Deserializes a string into a TransmissionPackage.
 * @param str The string representation of a TransmissionPackage.
 * @return The deserialized TransmissionPackage.
 */
TransmissionPackage str_to_Package(string str);

/**
 * @brief Displays the contents of a TransmissionPackage for debugging or logging.
 * @param pkg The TransmissionPackage to display.
 */
void showPackage(TransmissionPackage pkg);


/**
 * @brief Serializes a parSig (partial signature) into a string for transmission.
 * @param sig The parSig object to serialize.
 * @return A string representation of the parSig.
 */
std::string parSig_to_str(const parSig &sig);

/**
 * @brief Deserializes a string into a parSig (partial signature) object.
 * @param str The serialized parSig string.
 * @return The reconstructed parSig object.
 */
parSig str_to_parSig(const std::string &str);

/**
 * @brief Displays the contents of a parSig for debugging or logging.
 * @param sig The parSig object to display.
 */
void showParSig(parSig sig);


/**
 * @brief Serializes a Sigma (final aggregated signature) into a string for transmission.
 * @param sg The Sigma object to serialize.
 * @return A string representation of the Sigma.
 */
std::string Sigma_to_str(const Sigma &sg);

/**
 * @brief Deserializes a string into a Sigma (final aggregated signature) object.
 * @param str The serialized Sigma string.
 * @return The reconstructed Sigma object.
 */
Sigma str_to_Sigma(const std::string &str);

/**
 * @brief Displays the contents of a Sigma signature for debugging or logging.
 * @param sg The Sigma object to display.
 */
void showSigma(Sigma sg);


/**
 * Converts an mpz_class to a std::string
 * @param value The mpz_class to be converted
 * @return The converted string
 */
std::string mpz_to_str(const mpz_class& value);

/**
 * Converts a string to mpz_class
 * @param str The string to be converted
 * @return The converted mpz_class
 */
mpz_class str_to_mpz(const std::string& str);

/**
 * Converts an ECP point to a std::string
 * @param ecp The ECP point to be converted
 * @return The converted string
 */
std::string ECP_to_str(ECP ecp);

/**
 * Converts a string to an ECP point
 * @param str The string to be converted
 * @return The converted ECP point
 */
ECP str_to_ECP(const std::string& str);

/**
 * Converts an ECP2 point to a std::string
 * @param ecp2 The ECP2 point to be converted
 * @param compressed Whether to use compressed format
 * @return The converted string
 */
std::string ECP2_to_str(ECP2 ecp2, bool compressed = true);

/**
 * Converts a string to an ECP2 point
 * @param hex_string The string to be converted
 * @return The converted ECP2 point
 */
ECP2 str_to_ECP2(const std::string& hex_string);

/**
 * Converts an FP12 to a std::string
 * @param fp12 The FP12 to be converted
 * @return The converted string
 */
std::string FP12_to_str(FP12 fp12);

/**
 * Converts a string to an FP12
 * @param hex_string The string to be converted
 * @return The converted FP12
 */
FP12 str_to_FP12(const std::string& hex_string);

/**
 * Converts an array of mpz_class to a std::string
 * @param mpzs The array of mpz_class values to be converted
 * @return The converted string
 */
std::string mpzArr_to_str(const std::vector<mpz_class>& mpzs);

/**
 * Converts a string to an array of mpz_class
 * @param str The string to be converted
 * @return The converted array of mpz_class
 */
std::vector<mpz_class> str_to_mpzArr(const std::string& str);

/**
 * Converts an array of ECP points to a std::string
 * @param ecps The array of ECP points to be converted
 * @return The converted string
 */
std::string ECPArr_to_str(const std::vector<ECP>& ecps);

/**
 * Converts a string to an array of ECP points
 * @param str The string to be converted
 * @return The converted array of ECP points
 */
std::vector<ECP> str_to_ECPArr(const std::string& str);

/**
 * Converts an array of ECP2 points to a std::string
 * @param ecp2s The array of ECP2 points to be converted
 * @param compressed Whether to use compressed format
 * @return The converted string
 */
std::string ECP2Arr_to_str(const std::vector<ECP2>& ecp2s, bool compressed = true);

/**
 * Converts a string to an array of ECP2 points
 * @param str The string to be converted
 * @return The converted array of ECP2 points
 */
std::vector<ECP2> str_to_ECP2Arr(const std::string& str);