🚦 Understanding How Pipelines Function
🧠 1. Problem Definition
The goal of this project is to build a cycle-accurate 5-stage pipeline simulator using the SimpleScalar toolset. The simulator models a simplified version of the MIPS pipeline as described in Hennessy and Patterson, with the following assumptions:

✅ Perfect instruction and data caches (no I-cache or D-cache misses)

🚫 No branch prediction

🧮 Split-phase register access (write in the first half, read in the second half of a cycle)

🧭 Branch resolution occurs in the EX stage (not ID)

🎯 The simulator must:
Correctly simulate instruction flow through all pipeline stages

Detect and stall on data hazards and control hazards

Model pipeline behavior with and without result forwarding

⚠️ 2. Problems Encountered
During development, several challenges were identified:

❌ No Existing Pipeline Simulator
SimpleScalar lacks a built-in 5-stage pipeline simulator. sim-outorder exists but models out-of-order superscalar execution, which doesn’t fit our goal.

⚙️ Functional Simulation in ID Stage
SimpleScalar executes instructions functionally in the ID stage, making it hard to simulate real pipeline timing without custom logic.

🔁 Pipeline Register Management
Implementing and synchronizing pipeline latches (if_id_s, id_ex_s, etc.) was non-trivial, especially when handling stalls and forwarding paths.

🛠️ 3. Our Solution
We developed a custom simulator named sim-pipe.c to implement the classic 5-stage MIPS pipeline:

🧩 Key Components:
Pipeline stages implemented as individual functions:

instruction_fetch()

instruction_decode()

execute()

memory_access()

write_back()

Reverse-order simulation loop:
Run pipeline stages in order WB → MEM → EX → ID → IF so each stage can read data before the previous stage overwrites it.

Pipeline latches:
Custom structs (e.g., stage_latch) hold data between stages:

Fields: in1, in2, out1, op, etc.

Populated using DEFINST for execution metadata and hazard detection

Hazard handling:

Branch hazards: detected in ID, resolved in EX, cause a stall in IF

Data hazards: handled in ID using input/output register tracking; stalls inserted when needed

✅ 4. Model Verification
We used test-math to compile and verify the simulator:

bash
Copy
Edit
/home/mb/CompArch/bin/ssbig-na-sstrix-gcc -o test-math test-math.S -nostdlib -O0
🔍 Running our simulator:
Without forwarding:

bash
Copy
Edit
./sim-pipe -v -max:inst 20 tests/bin/test-math
With forwarding:

bash
Copy
Edit
./sim-pipe -v -max:inst 20 -do:forwarding true tests/bin/test-math
🧪 Reference comparison using sim-outorder:
bash
Copy
Edit
sim-outorder -issue:inorder true \
             -decode:width 1 \
             -issue:width 1 \
             -commit:width 1 \
             -ptrace trace.out 0:4 tests/bin/test-math
📈 Visualizing the trace:
bash
Copy
Edit
perl pipeview.pl trace.out
This visual inspection helped confirm the correct timing and behavior of our simulator, although it differed from our precise 5-stage implementation.

🖼️ 5. Output Samples
✅ With Forwarding


❌ Without Forwarding


📊 Reference (sim-outorder)
