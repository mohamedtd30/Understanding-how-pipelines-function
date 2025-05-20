# 🚀 Simulation of 5-Stage MIPS Pipeline

A simulation project that models and analyzes a **5-stage MIPS pipeline processor** using the SimpleScalar toolset, with a focus on understanding pipeline behavior, data/control hazards, and modeling performance with and without result forwarding.

---

## 📚 Table of Contents

- [💡 Acknowledgments](#-acknowledgments)
- [🔍 Problem Description](#-problem-description)
- [🧰 Prerequisites](#-prerequisites)
- [🖥️ Environment Setup](#-environment-setup)
- [✏️ Simulation Code Implementation](#-simulation-code-implementation)
- [🏁 Compile and Run](#-compile-and-run)
- [🧪 Testing and Validation](#-testing-and-validation)
- [🧩 Challenges Encountered](#-challenges-encountered)
- [🛠️ Handling Challenges](#-handling-challenges)
- [📝 Conclusion](#-conclusion)
- [📌 References](#-references)
- [👥 Contributors](#-contributors)

---

## 💡 Acknowledgments

Special thanks to **Dr. May Salama**, course coordinator of *ECE322C – Computer Architecture*, for guidance throughout this project.

---

## 🔍 Problem Description

The goal of this project is to build a **cycle-accurate 5-stage pipeline simulator** using the SimpleScalar toolset. The simulator models a simplified version of the classic MIPS pipeline as described by Hennessy and Patterson.

**Assumptions:**

- Perfect instruction and data caches (no misses).
- No branch prediction.
- Split-phase register access (write in first half, read in second half).
- Branch resolution occurs in the EX stage.

The simulator must:

- Simulate instruction flow through IF, ID, EX, MEM, WB stages.
- Handle stalls due to data/control hazards.
- Support both with and without result forwarding.

---

## 🧰 Prerequisites

- C programming knowledge
- Understanding of MIPS architecture
- Familiarity with pipeline hazards (RAW, WAR, WAW)
- SimpleScalar toolset

---

## 🖥️ Environment Setup

1. Clone the SimpleScalar repository:
   ```bash
   git clone https://github.com/sslab-gatech/simplescalar.git
2.Install required build tools:
  ```bash
`  sudo apt install build-essential
```
3.Navigate to the SimpleScalar directory and compile:
```
cd simplescalar
make config-alpha
make
```

## ✏️ Simulation Code Implementation

### Simple 5-Stage Pipeline Simulator

This project implements a simulator for a classic RISC 5-stage instruction pipeline, inspired by the Alpha architecture and the SimpleScalar toolset. The pipeline consists of the following stages:
- **IF**: Instruction Fetch
- **ID**: Instruction Decode / Register Read
- **EX**: Execute / Address Calculation
- **MEM**: Memory Access
- **WB**: Write Back

### Features

- **Accurate Pipeline Timing:** Simulates the flow of instructions through the pipeline with correct timing for each stage.
- **Data Hazard Detection:** Detects RAW (Read After Write) data hazards and introduces stalls when needed.
- **Control Hazard Handling:** Implements branch hazard detection and pipeline flushes for taken branches, resolving branches in the EX stage.
- **Optional Data Forwarding:** Can be enabled to reduce data hazard stalls.
- **Statistics:** Reports total instructions, cycles, data hazard stalls, and control hazard stalls.

### How It Works

- Each pipeline stage is implemented as a function. Stages execute in reverse order each cycle to model realistic pipeline behavior.
- Latches (pipeline registers) connect the stages and hold instruction and control information.
- Branches are resolved in the EX stage. When a branch is taken, the fetch stage is flushed to prevent wrong-path instructions from executing.
- The simulator can be run with command-line options for verbose output and instruction limits.

### Example Usage

```bash
./sim-pipe -v -max:inst 20 tests/bin/test-math
```

### Output

The simulator prints each pipeline stage’s activity per cycle (with `-v`), along with pipeline statistics at the end, such as:

```
Total instructions executed:  20
Total cycles:                 33
Control hazard stalls:         7
Data hazard stalls:            4
```

### Notes

- The code is intended for educational purposes and to help understand how pipelined processors work.
- See the source code comments for implementation details on hazard detection, forwarding, and pipeline flushing.

## 🏑 Compile and Run

### simulator compilation
```bash
make sim-pipe
```
### Without Forwarding:
```bash
./sim-pipe -v -max:inst 20 tests/bin/test-math
```

### With Forwarding:
```bash
./sim-pipe -v -max:inst 20 -do:forwarding true tests/bin/test-math
```

### sim-outorder with custom settings
```bash
# run
./sim-outorder -issue:inorder true -decode:width 1 -issue:width 1 -commit:width 1 -ptrace trace.out 0:4 tests/bin/test-math

perl pipeview.pl trace.out
```
---
## 🧪 Testing and Validation

We validated our implementation by:

- ✅ Running test binaries such as `test-math` with and without forwarding.
- 🔁 Comparing cycle-accurate results visually and through trace analysis.
- 🔍 Cross-validating with `sim-outorder` under restricted configurations.
**Using sim-pipe with forwarding:**

![Screenshot 2025-05-16 112905](https://github.com/user-attachments/assets/220cbb76-9ad1-445f-a1cb-c75b2609cb3a)      ![Screenshot 2025-05-16 112851](https://github.com/user-attachments/assets/b3930718-f0a2-460b-a1bc-60ad4b1ab2b0)

**Using sim-pipe without forwarding:**

![Screenshot 2025-05-16 112939](https://github.com/user-attachments/assets/d5cf0698-51cc-43e3-952f-0abb0426b921)     ![Screenshot 2025-05-16 112927](https://github.com/user-attachments/assets/274579fb-c5cd-4394-ba4d-6965af8213c5)

**Using sim-outorder:**

![Screenshot 2025-05-16 113308](https://github.com/user-attachments/assets/9a87a1a9-797f-4bfa-99e7-91250c7b9a47)

---
## 🧹 Challenges Encountered

**No Existing Pipeline Simulator:**  
SimpleScalar does not provide a built-in simulator that directly implements a 5-stage pipeline. Although sim-outorder exists, it models a superscalar, out-of-order architecture and does not align with our goals.

**Functional Simulation in ID Stage:**  
Due to the architecture of SimpleScalar, instruction execution occurs functionally in the ID stage, which differs from real pipeline timing. Care was required to model pipeline timing while still executing in ID.

**Pipeline Register Management:**  
Correct handling of pipeline latches (if_id_s, id_ex_s, etc.) was tricky, especially when modeling stalls and forwarding paths.

---

## 🛠️ Handling Challenges
We created a new file sim-pipe.c to implement the custom 5-stage pipeline, since no existing simulator handled this.

The simulator uses 5 functions, one for each stage:  
instruction_fetch(), instruction_decode(), execute(), memory_access(), write_back().

A **reverse-order simulation loop** (WB → MEM → EX → ID → IF) is used so that each stage can read data from the previous stage before it is overwritten.
Pipeline latches (stage_latch structs) were used to carry information between stages.

**Hazard Handling:**
- Branch hazards are detected in the ID stage and cause a stall in the IF stage until resolution in EX.
- Data hazards are detected using the register inputs/outputs and handled by stalling the pipeline in the ID stage.

**Loader Bug Fix:**
   - Corrected a bug in the loader:
     ```c
     // Original:
     if (fread(p, shdr.s_size, 1, fobj) < 1)

     // Fixed:
     if (shdr.s_size > 0 && (fread(p, shdr.s_size, 1, fobj) < 1))

     ```
**Exploring sim-outorder with Custom Settings**
To investigate pipeline timing in a controlled, in-order, single-issue scenario, we ran sim-outorder with the following configuration:

```bash
sim-outorder \
  -issue:inorder true \
  -decode:width 1 \
  -issue:width 1 \
  -commit:width 1 \
  -ptrace trace.out 0:4 tests/bin/test-math
```
This command generates a cycle-accurate instruction trace, which we convert into a visual pipeline diagram using:

```bash
perl pipeview.pl trace.out
```

---
## 📝 Conclusion

This project culminates in the successful implementation of a **cycle-accurate 5-stage MIPS pipeline simulator**, inspired by the architecture described in *Hennessy and Patterson's* textbook. The simulator, built using the SimpleScalar toolset, effectively models the core concepts of instruction-level parallelism and pipelined execution in a simplified MIPS environment.

Key achievements of this project include:

- ✅ Accurate simulation of the 5 classical pipeline stages: **Instruction Fetch (IF), Instruction Decode (ID), Execute (EX), Memory Access (MEM), and Write Back (WB)**.
- 🔁 Implementation of **stalling and forwarding mechanisms** to resolve data hazards, with configurable options to enable or disable forwarding.
- 🧠 Correct **branch handling logic**, with resolution in the EX stage and appropriate control hazard stalling in the IF stage.
- 🧪 Thorough **testing and verification** against known functional outputs, with visual and trace-based validation.
- 🛠️ Development of custom hazard detection logic and pipeline control, overcoming SimpleScalar's limitations such as functional execution in the ID stage.

## 📌 References

- **Computer Architecture: A Quantitative Approach** by John L. Hennessy and David A. Patterson
  The foundational textbook describing the MIPS pipeline architecture and concepts of instruction-level parallelism.

- **SimpleScalar Toolset**  
  [http://www.simplescalar.com/](http://www.simplescalar.com/)  
  The simulation framework used to implement and test the custom 5-stage pipeline.

- **ECE322C – Computer Architecture**  
  Course materials and lectures provided by Dr. May Salama, guiding the theoretical and practical aspects of pipeline simulation.
- **pipeview.pl**  
  A Perl script used to visualize pipeline execution traces generated by SimpleScalar’s `sim-outorder`.

## 👥 Contributors 

- [Mohamed Toukhy Mohamed Ahmed](https://github.com/mohamedtd30)
- [Mahmoud Atef Mahmoud Abdelaziz](https://github.com/Mahmoud8513)
- [Mahmoud Atef Mahmoud Gohnem](https://github.com/atefghonem10)
- [Ali Hossam Ali](https://github.com/AliHossamTesla)
- [Yassin Ahmed Nasr](https://github.com/yassinjag5)

  



