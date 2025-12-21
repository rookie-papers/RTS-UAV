#include "../../common/include/Tools.h"
#include <atomic>

namespace RTS_web {
    extern csprng rng_websocket;
    extern gmp_randstate_t state_gmp_websocket;
    extern  std::atomic<int> serialNumber;

    // Aggregator
    typedef struct {
        mpz_class ID;
        mpz_class alpha;
    } UAV_h;

    // Ordinary signer in the signer group, where c1 and c2 are shares of the reconstruction key
    typedef struct {
        vector<mpz_class> c1;
        vector<mpz_class> c2;
        vector<ECP2> PK;
        mpz_class ID;
        int serialNumber;
    } UAV;

    typedef struct {
        int n;               // Number of signers
        int tm;              // Maximum threshold
        mpz_class q;         // Order of the curve
        ECP2 P2;             // Generator of G2
        mpz_class g;         // Group generator
        mpz_class beta;      // System parameter
        vector<ECP2> PK;     // Public key set
    } Params;

    typedef struct {
        mpz_class cj;        // Signature component
        ECP sig;             // Signature point in G1
        short index;         // Signer index
    } parSig;

    typedef struct {
        vector<mpz_class> aux;  // Auxiliary values
        vector<ECP> sig;        // Signatures
        vector<short> indices;  // Signer IDs
    } Sigma;

    /**
     * @brief Gets all prime factors of q-1 for the BLS12-381 curve order q
     * @return Vector of prime factors
     */
    vector<mpz_class> getFactors();

    /**
     * @brief Checks if x is coprime with all elements in factors
     * @param x Value to check
     * @param factors Prime factors of q-1
     * @return Returns true if coprime, otherwise false
     */
    bool isCoprime(mpz_class x, vector<mpz_class> factors);

    /**
     * @brief Hashes beHashed to a value coprime with q-1
     * @param beHashed Value to hash
     * @param module Modulus value
     * @param factors Prime factors of q-1
     * @return Hashed value that is coprime with q-1
     */
    mpz_class hashToCoprime(mpz_class beHashed, mpz_class module, vector<mpz_class> factors);

    /**
     * @brief Initializes the CTS system
     * @param alpha Aggregator’s private key
     * @param n Number of signers
     * @param tm Maximum threshold
     * @return Public parameters of the CTS system
     */
    Params Setup(mpz_class &alpha, int n, int tm);

    /**
     * @brief Generates the public key set for CTS
     * @param b Secret factors
     * @return Public key set
     */
    vector<ECP2> getPK(vector<mpz_class> b);

    /**
     * @brief Generates the share reconstruction key for a single signer
     * @param pp System public parameters
     * @param d Array of random values
     * @param b Array of random values
     * @param id Signer’s ID
     * @return Share reconstruction key and ID for the signer
     */
    UAV getUAV(Params pp, vector<mpz_class> d, vector<mpz_class> b, mpz_class id);

    /**
     * @brief Generates share reconstruction keys for all signers
     * @param params System public parameters
     * @param alpha Aggregator’s private key
     * @param uavH Aggregator
     * @return Share reconstruction keys and IDs for all signers
     */
    vector<UAV> KeyGen(Params &params, mpz_class alpha, UAV_h &uavH);

/**
     * @brief Generates a partial signature for a specific UAV.
     * * This function reconstructs the current signer set S based on the provided bitmap
     * and the global registered IDs. It then computes the Lagrange coefficient
     * for the signer and generates the signature share.
     * * @param pp System public parameters.
     * @param uav The UAV instance performing the signing (contains its private share and serial number).
     * @param t The threshold value required for reconstruction.
     * @param M The message hash or integer representation to be signed.
     * @param bitmap A binary string (or byte array) indicating which UAVs are selected to sign.
     * @param registeredIDs The global list of all registered UAV IDs, used to map bitmap bits to actual IDs.
     * @return parSig A partial signature structure containing the signature share and the signer's index.
     */
    parSig Sign(Params pp, UAV uav, int t, mpz_class M, const std::string& bitmap,
                const vector<mpz_class>& registeredIDs);

/**
     * @brief Simulates the collection of partial signatures from the selected signer group.
     * * This function parses the bitmap to determine which UAVs are selected,
     * invokes the Sign function for each selected UAV, and collects the results.
     * * @param pp System public parameters.
     * @param UAVs The list of all available UAV objects.
     * @param t The threshold value.
     * @param M The message to be signed.
     * @param bitmap The selection bitmap received from the Verifier.
     * @param registeredIDs The global list of all registered UAV IDs.
     * @return vector<parSig> A vector containing the partial signatures from all selected UAVs.
     */
    vector<parSig> collectSig(Params pp, vector<UAV> UAVs, int t, mpz_class M, const std::string& bitmap,
                              const vector<mpz_class>& registeredIDs);

/**
     * @brief Comparator function for sorting partial signatures.
     * * Sorts two partial signatures in ascending order based on their signer index.
     * This is required before aggregation to ensure the order of signatures matches
     * the order of Public Keys during verification.
     * * @param a The first partial signature.
     * @param b The second partial signature.
     * @return true If a.index < b.index.
     * @return false Otherwise.
     */
    bool compareParSig(const parSig &a, const parSig &b);

    /**
     * @brief Aggregator converts collected partial signatures for verification
     * @param parSigs Collected partial signatures
     * @param pp System public parameters
     * @param uavH Aggregator
     * @param PK_v Verifier’s public key
     * @return Aggregated signature converted by the aggregator
     */
    Sigma AggSig(vector<parSig> parSigs, Params pp, UAV_h uavH, mpz_class PK_v);

    /**
     * @brief Computes the Lagrange coefficient for a given signer
     * @param pp System public parameters
     * @param ID Signer’s ID
     * @param t Threshold required by the verifier
     * @return Lagrange coefficient for the signer
     */
    vector<mpz_class> getPi_0s(Params pp, vector<mpz_class> ID, int t);

    /**
     * @brief Computes the Lagrange coefficient for a given signer
     * @param pp System public parameters
     * @param ID All signer’s ID
     * @param t Threshold required by the verifier
     * @param myID Signer’s ID
     * @return Lagrange coefficient for the signer
     */
    mpz_class getPi_0(Params pp, vector<mpz_class> ID, int t, mpz_class myID);

/**
     * @brief Verifies the validity of an aggregated signature.
     * * This function reconstructs the set of participating Public Keys and IDs using
     * the indices embedded in the Sigma structure and the provided global registries.
     * It then performs the bilinear pairing check to validate the signature.
     * * @param sigma The aggregated signature structure containing signature components and signer indices.
     * @param sk_v The Verifier's private key (used to unblind the signature).
     * @param pp System public parameters.
     * @param M The original message that was signed.
     * @param globalIDs The global registry of all UAV IDs (indices in Sigma refer to positions in this vector).
     * @param globalPKs The global registry of all UAV Public Keys (indices in Sigma refer to positions in this vector).
     * @return int Returns 1 if the signature is valid, 0 otherwise.
     */
    int Verify(Sigma sigma, mpz_class sk_v, Params pp, mpz_class M,
               const vector<mpz_class>& globalIDs,
               const vector<ECP2>& globalPKs);
}