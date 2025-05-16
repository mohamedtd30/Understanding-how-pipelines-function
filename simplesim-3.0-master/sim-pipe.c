#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"

static struct regs_t regs;
static struct mem_t *mem = NULL;
static counter_t sim_num_refs = 0;
static unsigned int max_insts;

struct stage_latch {
  int busy;
  md_inst_t IR;
  md_addr_t PC;
  md_addr_t NPC;
  md_addr_t addr;
  int out1;
  int out2;
  int in1;
  int in2;
  int in3;
  enum md_opcode op;
  int will_exit;
  int reg_value;
  int mem_value;
  int reg_result_ready;
  int pending_write;
};

static struct stage_latch if_id_s, id_ex_s, ex_mem_s, mem_wb_s, wb_finished_s;
static int do_forwarding = 0;
static counter_t sim_cycle = 0;
static counter_t data_hazard_stalls = 0;
static counter_t control_hazard_stalls = 0;
static int pipe_exit_now = 0;

void
sim_reg_options(struct opt_odb_t *odb)
{
  opt_reg_uint(odb, "-max:inst", "maximum number of inst's to execute",
         &max_insts, 0,
         TRUE, NULL);
  
  opt_reg_flag(odb, "-do:forwarding", "enable data forwarding",
         &do_forwarding, FALSE,
         TRUE, NULL);
}

void
sim_check_options(struct opt_odb_t *odb, int argc, char **argv)
{
}

void
sim_reg_stats(struct stat_sdb_t *sdb)
{
  stat_reg_counter(sdb, "sim_num_insn",
         "total number of instructions executed",
         &sim_num_insn, sim_num_insn, NULL);
  stat_reg_counter(sdb, "sim_num_refs",
         "total number of loads and stores executed",
         &sim_num_refs, 0, NULL);
  stat_reg_counter(sdb, "sim_num_cycles",
         "total number of cycles executed",
         &sim_cycle, 0, NULL);
  stat_reg_counter(sdb, "sim_data_hazard_stalls",
         "total number of data hazard stalls",
         &data_hazard_stalls, 0, NULL);
  stat_reg_counter(sdb, "sim_control_hazard_stalls",
         "total number of control hazard stalls",
         &control_hazard_stalls, 0, NULL);
}

void
sim_init(void)
{
  sim_num_refs = 0;
  sim_cycle = 0;
  data_hazard_stalls = 0;
  control_hazard_stalls = 0;
  pipe_exit_now = 0;

  regs_init(&regs);
  mem = mem_create("mem");
  mem_init(mem);

  if_id_s.busy = 0;
  id_ex_s.busy = 0;
  ex_mem_s.busy = 0;
  mem_wb_s.busy = 0;
  wb_finished_s.busy = 0;
}

void
sim_load_prog(char *fname,
          int argc, char **argv,
          char **envp)
{
  ld_load_prog(fname, argc, argv, envp, &regs, mem, TRUE);
  dlite_init(md_reg_obj, dlite_mem_obj, dlite_mstate_obj);
}

void
sim_aux_config(FILE *stream)
{
  fprintf(stream, "Pipelines stages: IF, ID, EX, MEM, WB\n");
  fprintf(stream, "Data forwarding: %s\n", do_forwarding ? "enabled" : "disabled");
}

void
sim_aux_stats(FILE *stream)
{
}

void
sim_uninit(void)
{
}

#define SET_NPC(EXPR)		(regs.regs_NPC = (EXPR))
#define CPC			(regs.regs_PC)
#define GPR(N)			(regs.regs_R[N])
#define SET_GPR(N,EXPR)		(regs.regs_R[N] = (EXPR))

#if defined(TARGET_PISA)
#define FPR_L(N)		(regs.regs_F.l[(N)])
#define SET_FPR_L(N,EXPR)	(regs.regs_F.l[(N)] = (EXPR))
#define FPR_F(N)		(regs.regs_F.f[(N)])
#define SET_FPR_F(N,EXPR)	(regs.regs_F.f[(N)] = (EXPR))
#define FPR_D(N)		(regs.regs_F.d[(N) >> 1])
#define SET_FPR_D(N,EXPR)	(regs.regs_F.d[(N) >> 1] = (EXPR))
#define SET_HI(EXPR)		(regs.regs_C.hi = (EXPR))
#define HI			(regs.regs_C.hi)
#define SET_LO(EXPR)		(regs.regs_C.lo = (EXPR))
#define LO			(regs.regs_C.lo)
#define FCC			(regs.regs_C.fcc)
#define SET_FCC(EXPR)		(regs.regs_C.fcc = (EXPR))

#elif defined(TARGET_ALPHA)
#define FPR_Q(N)		(regs.regs_F.q[N])
#define SET_FPR_Q(N,EXPR)	(regs.regs_F.q[N] = (EXPR))
#define FPR(N)			(regs.regs_F.d[(N)])
#define SET_FPR(N,EXPR)		(regs.regs_F.d[(N)] = (EXPR))
#define FPCR			(regs.regs_C.fpcr)
#define SET_FPCR(EXPR)		(regs.regs_C.fpcr = (EXPR))
#define UNIQ			(regs.regs_C.uniq)
#define SET_UNIQ(EXPR)		(regs.regs_C.uniq = (EXPR))

#else
#error No ISA target defined...
#endif

#define READ_BYTE(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_BYTE(mem, addr))
#define READ_HALF(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_HALF(mem, addr))
#define READ_WORD(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_WORD(mem, addr))
#ifdef HOST_HAS_QWORD
#define READ_QWORD(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_QWORD(mem, addr))
#endif

#define WRITE_BYTE(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_BYTE(mem, addr, (SRC)))
#define WRITE_HALF(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_HALF(mem, addr, (SRC)))
#define WRITE_WORD(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_WORD(mem, addr, (SRC)))
#ifdef HOST_HAS_QWORD
#define WRITE_QWORD(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_QWORD(mem, addr, (SRC)))
#endif

#define SYSCALL(INST)	sys_syscall(&regs, mem_access, mem, INST, TRUE)
#define RS_FIELD(INST)		(((INST) >> 21) & 0x1f)
#define RT_FIELD(INST)		(((INST) >> 16) & 0x1f)
#define RD_FIELD(INST)		(((INST) >> 11) & 0x1f)

static int
check_data_hazard(int src_reg) 
{
  if (src_reg == 0)
    return 0;

  if (id_ex_s.busy && id_ex_s.pending_write && 
      (id_ex_s.out1 == src_reg || id_ex_s.out2 == src_reg)) {
    if (do_forwarding && id_ex_s.reg_result_ready)
      return 0;
    return 1;
  }
  
  if (ex_mem_s.busy && ex_mem_s.pending_write && 
      (ex_mem_s.out1 == src_reg || ex_mem_s.out2 == src_reg)) {
    if (do_forwarding && !(MD_OP_FLAGS(ex_mem_s.op) & F_LOAD))
      return 0;
    return 1;
  }
  
  if (mem_wb_s.busy && mem_wb_s.pending_write && 
      (mem_wb_s.out1 == src_reg || mem_wb_s.out2 == src_reg)) {
    if (do_forwarding)
      return 0;
    return 1;
  }

  return 0;
}

static int
get_forwarded_value(int reg_num)
{
  if (reg_num == 0)
    return 0;

  if (ex_mem_s.busy && ex_mem_s.pending_write && 
      (ex_mem_s.out1 == reg_num || ex_mem_s.out2 == reg_num)) {
    if (!(MD_OP_FLAGS(ex_mem_s.op) & F_LOAD)) {
      return ex_mem_s.reg_value;
    }
  }
  
  if (mem_wb_s.busy && mem_wb_s.pending_write && 
      (mem_wb_s.out1 == reg_num || mem_wb_s.out2 == reg_num)) {
    return mem_wb_s.reg_value;
  }
  
  return GPR(reg_num);
}

static int
check_for_exit(enum md_opcode op)
{
  if (MD_OP_FLAGS(op) & F_TRAP) {
    if (GPR(0) == 1) {
      return 1;
    }
  }
  
  return 0;
}

void instruction_fetch(void)
{
  md_inst_t inst;
  md_addr_t pc;
  
  if (pipe_exit_now || sim_exit_now)
    return;
  
  if (if_id_s.busy)
    return;
    
  pc = regs.regs_PC;
  
  if (!pc || pc == 0x0)
    return;
  
  MD_FETCH_INST(inst, mem, pc);
  
  if_id_s.IR = inst;
  if_id_s.PC = pc;
  if_id_s.NPC = pc + sizeof(md_inst_t);
  if_id_s.busy = 1;
  if_id_s.will_exit = 0;
  
  if (!id_ex_s.busy || (MD_OP_FLAGS(id_ex_s.op) & F_CTRL) == 0) {
    regs.regs_PC = pc + sizeof(md_inst_t);
  }
  
  if (verbose) {
    myfprintf(stderr, "fetch: %10lld %5lld [xor: 0x%08x] @ 0x%08p: ",
              sim_cycle, sim_num_insn, md_xor_regs(&regs), pc);
    md_print_insn(inst, pc, stderr);
    fprintf(stderr, "\n");
  }
}

void instruction_decode(void)
{
  md_inst_t inst;
  enum md_opcode op;
  register md_addr_t addr;
  enum md_fault_type fault;
  int stall = 0;
  
  if (pipe_exit_now || sim_exit_now)
    return;
    
  if (id_ex_s.busy)
    return;
  
  if (!if_id_s.busy)
    return;
  
  inst = if_id_s.IR;
  
  MD_SET_OPCODE(op, inst);
  
  int in1 = 0, in2 = 0, in3 = 0;
  int out1 = 0, out2 = 0;
  int pending_write = 0;
  
  if (MD_OP_FLAGS(op) & F_ICOMP) {
    out1 = RD_FIELD(inst);
    in1 = RS_FIELD(inst);
    in2 = RT_FIELD(inst);
    pending_write = 1;
  } 
  else if (MD_OP_FLAGS(op) & F_LOAD) {
    out1 = RT_FIELD(inst);
    in1 = RS_FIELD(inst);
    pending_write = 1;
  }
  else if (MD_OP_FLAGS(op) & F_STORE) {
    in1 = RT_FIELD(inst);
    in2 = RS_FIELD(inst);
  }
  else if (MD_OP_FLAGS(op) & F_CTRL) {
    in1 = RS_FIELD(inst);
    in2 = RT_FIELD(inst);
    
    if (MD_OP_FLAGS(op) & F_CALL) {
      out1 = 31;
      pending_write = 1;
    }
  }
  
  if (in1 && check_data_hazard(in1)) {
    stall = 1;
    data_hazard_stalls++;
  }
  if (in2 && check_data_hazard(in2)) {
    stall = 1;
    data_hazard_stalls++;
  }
  if (in3 && check_data_hazard(in3)) {
    stall = 1;
    data_hazard_stalls++;
  }
  
  int branch_inst = ((MD_OP_FLAGS(op) & F_CTRL) != 0);
  if (branch_inst) {
    control_hazard_stalls++;
  }
  
  if (stall) {
    return;
  }
  
  if (do_forwarding) {
    if (in1) (void)get_forwarded_value(in1);
    if (in2) (void)get_forwarded_value(in2);
  }
  
  addr = 0; 
  fault = md_fault_none;
  
  switch (op) {
    #define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,O2,I1,I2,I3)    \
    case OP:                                                         \
      SYMCAT(OP,_IMPL);                                              \
      break;
    #define DEFLINK(OP,MSK,NAME,MASK,SHIFT)                          \
      case OP:                                                        \
      panic("attempted to execute a linking opcode");
    #define CONNECT(OP)
    #define DECLARE_FAULT(FAULT)                                      \
      { fault = (FAULT); break; }
    #include "machine.def"
    default:
      panic("attempted to execute a bogus opcode");
  }
  
  if (fault != md_fault_none)
    fatal("fault (%d) detected @ 0x%08p", fault, if_id_s.PC);
  
  int will_exit = check_for_exit(op);
  
  id_ex_s.IR = inst;
  id_ex_s.PC = if_id_s.PC;
  id_ex_s.NPC = if_id_s.NPC;
  id_ex_s.op = op;
  id_ex_s.in1 = in1;
  id_ex_s.in2 = in2;
  id_ex_s.in3 = in3;
  id_ex_s.out1 = out1;
  id_ex_s.out2 = out2;
  id_ex_s.reg_result_ready = 0;
  id_ex_s.will_exit = will_exit;
  id_ex_s.pending_write = pending_write;
  
  if (MD_OP_FLAGS(op) & F_MEM) {
    id_ex_s.addr = addr;
    if (MD_OP_FLAGS(op) & F_STORE)
      id_ex_s.mem_value = GPR(in1);
  }
  
  id_ex_s.busy = 1;
  if_id_s.busy = 0;
  
  if (verbose) {
    myfprintf(stderr, "decode: %10lld %5lld [xor: 0x%08x] @ 0x%08p: ",
              sim_cycle, sim_num_insn, md_xor_regs(&regs), if_id_s.PC);
    md_print_insn(inst, if_id_s.PC, stderr);
    fprintf(stderr, "\n");
  }
}

void execute(void)
{
  if (pipe_exit_now || sim_exit_now)
    return;
    
  if (ex_mem_s.busy)
    return;
    
  if (!id_ex_s.busy)
    return;
    
  int branch_inst = ((MD_OP_FLAGS(id_ex_s.op) & F_CTRL) != 0);
  md_addr_t branch_target = 0;
  
  if (branch_inst) {
    if (regs.regs_NPC != id_ex_s.NPC) {
      branch_target = regs.regs_NPC;
      regs.regs_PC = branch_target;
      if_id_s.busy = 0;
    }
  }
  
  if (MD_OP_FLAGS(id_ex_s.op) & (F_ICOMP|F_FCOMP)) {
    id_ex_s.reg_result_ready = 1;
    
    if (id_ex_s.out1)
      id_ex_s.reg_value = GPR(id_ex_s.out1);
  }
  
  ex_mem_s = id_ex_s;
  
  ex_mem_s.busy = 1;
  id_ex_s.busy = 0;
  
  if (verbose) {
    myfprintf(stderr, "execute: %10lld %5lld [xor: 0x%08x] @ 0x%08p: ",
              sim_cycle, sim_num_insn, md_xor_regs(&regs), ex_mem_s.PC);
    md_print_insn(ex_mem_s.IR, ex_mem_s.PC, stderr);
    fprintf(stderr, "\n");
  }
}

void memory_access(void)
{
  if (pipe_exit_now || sim_exit_now)
    return;
    
  if (mem_wb_s.busy)
    return;
    
  if (!ex_mem_s.busy)
    return;
    
  if (MD_OP_FLAGS(ex_mem_s.op) & F_MEM) {
    if (MD_OP_FLAGS(ex_mem_s.op) & F_LOAD) {
      if (ex_mem_s.out1)
        ex_mem_s.reg_value = GPR(ex_mem_s.out1);
        
      sim_num_refs++;
    }
    else if (MD_OP_FLAGS(ex_mem_s.op) & F_STORE) {
      sim_num_refs++;
    }
  }
  
  mem_wb_s = ex_mem_s;
  
  mem_wb_s.busy = 1;
  ex_mem_s.busy = 0;
  
  if (verbose) {
    myfprintf(stderr, "memory: %10lld %5lld [xor: 0x%08x] @ 0x%08p: ",
              sim_cycle, sim_num_insn, md_xor_regs(&regs), mem_wb_s.PC);
    md_print_insn(mem_wb_s.IR, mem_wb_s.PC, stderr);
    fprintf(stderr, "\n");
  }
}

void writeback(void)
{
  if (pipe_exit_now || sim_exit_now)
    return;
    
  if (!mem_wb_s.busy)
    return;
    
  if (mem_wb_s.pending_write) {
  }
  
  sim_num_insn++;
  
  if (mem_wb_s.will_exit) {
    pipe_exit_now = 1;
    sim_exit_now = 1;
  }
  
  wb_finished_s = mem_wb_s;
  
  mem_wb_s.busy = 0;
  
  if (verbose) {
    myfprintf(stderr, "writeback: %10lld %5lld [xor: 0x%08x] @ 0x%08p: ",
              sim_cycle, sim_num_insn, md_xor_regs(&regs), wb_finished_s.PC);
    md_print_insn(wb_finished_s.IR, wb_finished_s.PC, stderr);
    fprintf(stderr, "\n");
  }
}

void
sim_main(void)
{
  fprintf(stderr, "sim: ** starting pipeline simulation **\n");

  if (dlite_check_break(regs.regs_PC, 0, 0, 0, 0))
    dlite_main(regs.regs_PC - sizeof(md_inst_t),
         regs.regs_PC, sim_num_insn, &regs, mem);

  int max_cycles = 500000;
  int cycle_limit_reached = 0;
  
  md_addr_t last_PC = 0;
  int PC_stable_count = 0;
  
  while (!pipe_exit_now && !sim_exit_now) {
    regs.regs_R[MD_REG_ZERO] = 0;
#ifdef TARGET_ALPHA
    regs.regs_F.d[MD_REG_ZERO] = 0.0;
#endif

    writeback();
    memory_access();
    execute();
    instruction_decode();
    instruction_fetch();

    sim_cycle++;

    if (!if_id_s.busy && !id_ex_s.busy && !ex_mem_s.busy && !mem_wb_s.busy) {
      if (!regs.regs_PC || regs.regs_PC == 0x0) {
        fprintf(stderr, "sim: ** program execution completed **\n");
        break;
      }
      
      if (regs.regs_PC == last_PC) {
        PC_stable_count++;
        if (PC_stable_count > 5) {
          fprintf(stderr, "sim: ** program appears to be complete (PC stable at 0x%08llx) **\n", 
                  (unsigned long long)regs.regs_PC);
          break;
        }
      } else {
        PC_stable_count = 0;
        last_PC = regs.regs_PC;
      }
    } else {
      PC_stable_count = 0;
      last_PC = regs.regs_PC;
    }

    if (dlite_check_break(regs.regs_PC, 0, 0, 0, 0))
      dlite_main(regs.regs_PC - sizeof(md_inst_t),
         regs.regs_PC, sim_num_insn, &regs, mem);

    if (max_insts && sim_num_insn >= max_insts) {
      fprintf(stderr, "sim: ** max instruction count reached **\n");
      break;
    }
    
    if (sim_cycle >= max_cycles) {
      fprintf(stderr, "sim: ** cycle limit reached, simulation terminated **\n");
      cycle_limit_reached = 1;
      break;
    }
  }
  
  if (pipe_exit_now || sim_exit_now)
    fprintf(stderr, "sim: ** program exit detected, simulation terminated **\n");
  else if (!cycle_limit_reached)
    fprintf(stderr, "sim: ** all instructions committed **\n");
    
  fprintf(stderr, "\nPipeline Simulation Statistics:\n");
  fprintf(stderr, "Total instructions executed:  %lld\n", sim_num_insn);
  fprintf(stderr, "Total cycles:                 %lld\n", sim_cycle);
  if (sim_cycle > 0) {
    fprintf(stderr, "Control hazard stalls:        %lld\n", control_hazard_stalls);
  }
}
