# ZimaBlade distributed computing cluster — First run

This adds a basic two-worker C++ prime-counting computation to an **existing** Git repository.
It is intended for Ubuntu Server or Linux; the POSIX socket API used here is not directly compilable with native Windows MSVC.

## 1. Build on each ZimaBlade

```bash
sudo apt update
sudo apt install -y git build-essential cmake
cd ~/ZimaBlade-C-Plus-Plus-Computing-Cluster-Project
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 2
```

## 2. Run from three SSH sessions

Controller:

```bash
./build/cluster-controller 1000000 5050
```

Worker 1:

```bash
./build/cluster-worker CONTROLLER_LAN_OR_TAILSCALE_IP 5050
```

Worker 2:

```bash
./build/cluster-worker CONTROLLER_LAN_OR_TAILSCALE_IP 5050
```

Replace `CONTROLLER_LAN_OR_TAILSCALE_IP` with the controller's actual address.
Expected result: `Total primes in [1, 1000000]: 78498`.

## Security and operation

Only run this demo on a trusted local network or tailnet. The demo TCP service has no client authentication or application-level encryption. Never expose port 5050 via public port forwarding. Limit port access with a host firewall or a Tailscale policy.
The current controller requires exactly two workers, and every run exits after one computation.
