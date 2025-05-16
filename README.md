Understanding-how-pipelines-function

🧠 1. Problem Definition
The goal of the project is to build a cycle-accurate 5-stage pipeline simulator using the SimpleScalar toolset. The simulator models a simplified version of the MIPS pipeline described in Hennessy and Patterson, with specific assumptions:

Perfect instruction and data caches (no I-cache or D-cache misses).

No branch prediction.

Split-phase register access (write in the first half, read in the second half of a cycle).

Branch resolution occurs in the EX stage (not ID).

The simulator must:

Correctly simulate instruction flow through the pipeline stages.

Detect and stall on data hazards and control hazards.

Model pipeline behavior both with and without result forwarding.

🚀 2. Problems Encountered
During development, several technical challenges arose:

No Existing Pipeline Simulator:
SimpleScalar does not provide a built-in simulator that directly implements a 5-stage pipeline. Although sim-outorder exists, it models a superscalar, out-of-order architecture and does not align with our goals.

Functional Simulation in ID Stage:
Due to the architecture of SimpleScalar, instruction execution occurs functionally in the ID stage, which differs from real pipeline timing. Care was required to model pipeline timing while still executing in ID.

Pipeline Register Management:
Correct handling of pipeline latches (if_id_s, id_ex_s, etc.) was tricky, especially when modeling stalls and forwarding paths.

🛠️ 3. How We Solved Them
We created a new file sim-pipe.c to implement the custom 5-stage pipeline, since no existing simulator handled this.

The simulator uses 5 functions, one for each stage: instruction_fetch(), instruction_decode(), execute(), memory_access(), write_back().

A reverse-order simulation loop (WB → MEM → EX → ID → IF) is used so that each stage can read data from the previous stage before it is overwritten.

Pipeline latches (stage_latch structs) were used to carry information between stages. Fields like in1, in2, out1, op, etc., are populated using DEFINST to support hazard detection.

Branch hazards are detected in the ID stage and cause a stall in the IF stage until resolution in EX.

Data hazards are detected using the register inputs/outputs and handled by stalling the pipeline in the ID stage.

To verify our model, We then tested our pipeline simulator using:

./sim-pipe -v -max:inst 20 tests/bin/test-math
./sim-pipe -v -max:inst 20 -do:forwarding true tests/bin/test-math

Additionally, we explored using sim-outorder to compare against a reference model, with these custom settings:

bash
Copy
Edit
sim-outorder -issue:inorder true -decode:width 1 -issue:width 1 -commit:width 1 -ptrace trace.out 0:4 tests/bin/test-math
We visualized the pipeline trace using:

bash
Copy
Edit
perl pipeview.pl trace.out

📤 4. Output Samples
Using sim-pipe with forwarding: 
![Screenshot 2025-05-16 112905](https://github.com/user-attachments/assets/38a3c4ad-4146-462e-8126-f05230cccf0f)
![Screenshot 2025-05-16 112851](https://github.com/user-attachments/assets/5786e232-3a9a-4ad4-acfb-2797995a9e45)

Using sim-pipe without forwarding: 
![Screenshot 2025-05-16 112939](https://github.com/user-attachments/assets/a833180c-a3ce-4473-ab54-7e18e5019a51)
![Screenshot 2025-05-16 112927](https://github.com/user-attachments/assets/e38a7915-2b4d-4f50-8c18-5269c8e8469a)

Using sim-outorder:
![Screenshot 2025-05-16 113308](https://github.com/user-attachments/assets/1834c3a1-848d-44ad-bfc8-0d223b9d51fd)

