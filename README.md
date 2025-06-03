# RTS-UAV: Runtime Threshold Signature-Based Unmanned Aerial Vehicle Swarm Authentication with Autonomous Splitting Support
This repository provides the implementation of **RTS**, a runtime threshold signature scheme proposed in our paper. It also includes implementations of three existing signature schemes — **FROST**, **MTS**, and **MuSig2** — which are used for experimental comparison.

---

## ⚙️ Build Instructions


### 0️⃣ Install Required Dependencies

```bash
sudo apt update
sudo apt install -y git cmake python3 build-essential m4
```

### 1️⃣ Clone repository with submodules and build with CMake:

```bash
git clone --recurse-submodules https://github.com/rookie-papers/RTS-UAV.git
cd RTS-UAV
mkdir build && cd build
cmake ..
make -j
```

### 2️⃣ Running Benchmarks
Each scheme builds into an individual benchmark executable.

```bash
./benchmark_RTS
./benchmark_FROST
./benchmark_MTS
./benchmark_MuSig2
```

### 3️⃣ Threshold Configuration

All signature schemes in this project use a default threshold value of **128**. If you want to test performance with a different threshold value, please manually modify the corresponding source files by updating the following line:

```cpp
#define T 128   // Change this to your desired threshold [MTS & FROST]
#define TM 128  // Change this to your desired threshold [RTS]
#define N 128   // Change this to your desired threshold [MuSig2]
```

## 📦 Dependencies

All dependencies are included as Git submodules:

| Library           | Source URL                                                                       |
|-------------------|----------------------------------------------------------------------------------|
| GMP               | [github.com/rookie-papers/GMP](https://github.com/rookie-papers/GMP) (via fork with CMake support) |
| MIRACL Core       | [github.com/miracl/core](https://github.com/miracl/core)                         |
| Google Benchmark  | [github.com/google/benchmark](https://github.com/google/benchmark)               |

You don't need to install them manually — they are automatically configured and built with CMake.
