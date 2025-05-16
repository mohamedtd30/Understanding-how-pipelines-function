# 5-Stage Pipeline Simulator
### Understanding Pipeline Functionality

## 🧠 1. Problem Definition  
**The goal of the project** is to build a cycle-accurate 5-stage pipeline simulator using the SimpleScalar toolset. The simulator models a simplified version of the MIPS pipeline described in Hennessy and Patterson, with specific assumptions:

- Perfect instruction and data caches (no I-cache or D-cache misses).
- No branch prediction.
- Split-phase register access (write in the first half, read in the second half of a cycle).
- Branch resolution occurs in the EX stage (not ID).

**The simulator must:**
- Correctly simulate instruction flow through the pipeline stages.
- Detect and stall on data hazards and control hazards.
- Model pipeline behavior both with and without result forwarding.

---

## 🚀 2. Problems Encountered  
During development, several technical challenges arose:

**No Existing Pipeline Simulator:**  
SimpleScalar does not provide a built-in simulator that directly implements a 5-stage pipeline. Although sim-outorder exists, it models a superscalar, out-of-order architecture and does not align with our goals.

**Functional Simulation in ID Stage:**  
Due to the architecture of SimpleScalar, instruction execution occurs functionally in the ID stage, which differs from real pipeline timing. Care was required to model pipeline timing while still executing in ID.

**Pipeline Register Management:**  
Correct handling of pipeline latches (if_id_s, id_ex_s, etc.) was tricky, especially when modeling stalls and forwarding paths.

---

## 🛠️ 3. How We Solved Them  
We created a new file sim-pipe.c to implement the custom 5-stage pipeline, since no existing simulator handled this.

The simulator uses 5 functions, one for each stage:  
`instruction_fetch()`, `instruction_decode()`, `execute()`, `memory_access()`, `write_back()`.

A **reverse-order simulation loop** (WB → MEM → EX → ID → IF) is used so that each stage can read data from the previous stage before it is overwritten.

Pipeline latches (`stage_latch` structs) were used to carry information between stages. Fields like `in1`, `in2`, `out1`, `op`, etc., are populated using `DEFINST` to support hazard detection.

**Hazard Handling:**
- Branch hazards are detected in the ID stage and cause a stall in the IF stage until resolution in EX.
- Data hazards are detected using the register inputs/outputs and handled by stalling the pipeline in the ID stage.

**Verification Testing:**
```bash
# To verify our model, We tested our pipeline simulator using:
./sim-pipe -v -max:inst 20 tests/bin/test-math 
./sim-pipe -v -max:inst 20 -do:forwarding true tests/bin/test-math

# Additionally, we explored using sim-outorder, with these custom settings:
sim-outorder -issue:inorder true -decode:width 1 -issue:width 1 -commit:width 1 -ptrace trace.out 0:4 tests/bin/test-math
perl pipeview.pl trace.out
```

## 📤 4. Output Samples
**Using sim-pipe with forwarding:**

![Screenshot 2025-05-16 112905](https://github.com/user-attachments/assets/220cbb76-9ad1-445f-a1cb-c75b2609cb3a)      ![Screenshot 2025-05-16 112851](https://github.com/user-attachments/assets/b3930718-f0a2-460b-a1bc-60ad4b1ab2b0)

**Using sim-pipe without forwarding:**

![Screenshot 2025-05-16 112939](https://github.com/user-attachments/assets/d5cf0698-51cc-43e3-952f-0abb0426b921)     ![Screenshot 2025-05-16 112927](https://github.com/user-attachments/assets/274579fb-c5cd-4394-ba4d-6965af8213c5)

**Using sim-outorder:**

![Screenshot 2025-05-16 113308](https://github.com/user-attachments/assets/9a87a1a9-797f-4bfa-99e7-91250c7b9a47)

**Course Code & Title:** ECE322C Computer Architecture

**Course Coordinator:** Dr. May Salama

**Team members**

**Mohamed Toukhy Mohamed Ahmed**

**Mahmoud Atef Mahmoud Abdelaziz**

**Mahmoud Atef Mahmoud Gohnem**

**Ali Hossam Ali**

**Yassin Ahmed Nassr**

