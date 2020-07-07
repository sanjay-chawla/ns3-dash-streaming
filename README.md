Dash Streaming in NS-3
================================

## Overview:
Setup for simulating Dash streaming over LTE. The code is based on NS-3.30 and requires additional configuration based on simulation preferences.
The examples here are based on AMuSt framework

## Setup
### Option 1 (Preferred): Using your existing ns-3 setup i.e. you have the control over changes  
1. Setup NS-3 and verify tests work before using this setup
2. Setup AMuSt libdash and content folders. (TODO: Add example structure)
3. Copy the from this repo to NS-3 folder. *Carefully merge the wscript files*
4. Configure for DASH usage
```shell
./waf configure --with-dash=../AMuSt-libdash/libdash/ --enable-examples --enable-tests
```
5. Build
```shell
./waf build
```
6. Run
```shell
./waf --run dash-lte-simulator
```
### Option 2: Using the entire ns-3 setup. Version dependant and tricky!
Using existing setup of ns-3 (version 3.30) from [here](https://github.com/sanjay-chawla/ns3-dash-streaming-full "Full code repo") and follow the step 3 onwards from option 1.

## Scenarios
The scenarios are the desired configuration of setup and depends on the use case and available resources.
- Scenario 1: Simulated nodes and network

- Scenario 2: Virtual nodes and simulated network

- Scenario 3: Direct code execution (TODO)

## References

1. Christian Kreuzberger, Daniel Posch, Hermann Hellwagner "AMuSt Framework - Adaptive Multimedia Streaming Simulation Framework for ns-3 and ndnSIM", https://github.com/ChristianKreuzberger/AMust-Simulator/
2. https://github.com/haraldott/dash
