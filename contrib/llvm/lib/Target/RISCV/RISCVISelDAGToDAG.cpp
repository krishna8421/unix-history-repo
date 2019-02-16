//===-- RISCVISelDAGToDAG.cpp - A dag to dag inst selector for RISCV ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the RISCV target.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "MCTargetDesc/RISCVMCTargetDesc.h"
#include "RISCVTargetMachine.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "riscv-isel"

// RISCV-specific code to select RISCV machine instructions for
// SelectionDAG operations.
namespace {
class RISCVDAGToDAGISel final : public SelectionDAGISel {
  const RISCVSubtarget *Subtarget;

public:
  explicit RISCVDAGToDAGISel(RISCVTargetMachine &TargetMachine)
      : SelectionDAGISel(TargetMachine) {}

  StringRef getPassName() const override {
    return "RISCV DAG->DAG Pattern Instruction Selection";
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    Subtarget = &MF.getSubtarget<RISCVSubtarget>();
    return SelectionDAGISel::runOnMachineFunction(MF);
  }

  void PostprocessISelDAG() override;

  void Select(SDNode *Node) override;

  bool SelectInlineAsmMemoryOperand(const SDValue &Op, unsigned ConstraintID,
                                    std::vector<SDValue> &OutOps) override;

  bool SelectAddrFI(SDValue Addr, SDValue &Base);

// Include the pieces autogenerated from the target description.
#include "RISCVGenDAGISel.inc"

private:
  void doPeepholeLoadStoreADDI();
  void doPeepholeBuildPairF64SplitF64();
};
}

void RISCVDAGToDAGISel::PostprocessISelDAG() {
  doPeepholeLoadStoreADDI();
  doPeepholeBuildPairF64SplitF64();
}

void RISCVDAGToDAGISel::Select(SDNode *Node) {
  unsigned Opcode = Node->getOpcode();
  MVT XLenVT = Subtarget->getXLenVT();

  // If we have a custom node, we have already selected
  if (Node->isMachineOpcode()) {
    LLVM_DEBUG(dbgs() << "== "; Node->dump(CurDAG); dbgs() << "\n");
    Node->setNodeId(-1);
    return;
  }

  // Instruction Selection not handled by the auto-generated tablegen selection
  // should be handled here.
  EVT VT = Node->getValueType(0);
  if (Opcode == ISD::Constant && VT == XLenVT) {
    auto *ConstNode = cast<ConstantSDNode>(Node);
    // Materialize zero constants as copies from X0. This allows the coalescer
    // to propagate these into other instructions.
    if (ConstNode->isNullValue()) {
      SDValue New = CurDAG->getCopyFromReg(CurDAG->getEntryNode(), SDLoc(Node),
                                           RISCV::X0, XLenVT);
      ReplaceNode(Node, New.getNode());
      return;
    }
  }
  if (Opcode == ISD::FrameIndex) {
    SDLoc DL(Node);
    SDValue Imm = CurDAG->getTargetConstant(0, DL, XLenVT);
    int FI = cast<FrameIndexSDNode>(Node)->getIndex();
    EVT VT = Node->getValueType(0);
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, VT);
    ReplaceNode(Node, CurDAG->getMachineNode(RISCV::ADDI, DL, VT, TFI, Imm));
    return;
  }

  // Select the default instruction.
  SelectCode(Node);
}

bool RISCVDAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, unsigned ConstraintID, std::vector<SDValue> &OutOps) {
  switch (ConstraintID) {
  case InlineAsm::Constraint_i:
  case InlineAsm::Constraint_m:
    // We just support simple memory operands that have a single address
    // operand and need no special handling.
    OutOps.push_back(Op);
    return false;
  default:
    break;
  }

  return true;
}

bool RISCVDAGToDAGISel::SelectAddrFI(SDValue Addr, SDValue &Base) {
  if (auto FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), Subtarget->getXLenVT());
    return true;
  }
  return false;
}

// Merge an ADDI into the offset of a load/store instruction where possible.
// (load (add base, off), 0) -> (load base, off)
// (store val, (add base, off)) -> (store val, base, off)
void RISCVDAGToDAGISel::doPeepholeLoadStoreADDI() {
  SelectionDAG::allnodes_iterator Position(CurDAG->getRoot().getNode());
  ++Position;

  while (Position != CurDAG->allnodes_begin()) {
    SDNode *N = &*--Position;
    // Skip dead nodes and any non-machine opcodes.
    if (N->use_empty() || !N->isMachineOpcode())
      continue;

    int OffsetOpIdx;
    int BaseOpIdx;

    // Only attempt this optimisation for I-type loads and S-type stores.
    switch (N->getMachineOpcode()) {
    default:
      continue;
    case RISCV::LB:
    case RISCV::LH:
    case RISCV::LW:
    case RISCV::LBU:
    case RISCV::LHU:
    case RISCV::LWU:
    case RISCV::LD:
    case RISCV::FLW:
    case RISCV::FLD:
      BaseOpIdx = 0;
      OffsetOpIdx = 1;
      break;
    case RISCV::SB:
    case RISCV::SH:
    case RISCV::SW:
    case RISCV::SD:
    case RISCV::FSW:
    case RISCV::FSD:
      BaseOpIdx = 1;
      OffsetOpIdx = 2;
      break;
    }

    // Currently, the load/store offset must be 0 to be considered for this
    // peephole optimisation.
    if (!isa<ConstantSDNode>(N->getOperand(OffsetOpIdx)) ||
        N->getConstantOperandVal(OffsetOpIdx) != 0)
      continue;

    SDValue Base = N->getOperand(BaseOpIdx);

    // If the base is an ADDI, we can merge it in to the load/store.
    if (!Base.isMachineOpcode() || Base.getMachineOpcode() != RISCV::ADDI)
      continue;

    SDValue ImmOperand = Base.getOperand(1);

    if (auto Const = dyn_cast<ConstantSDNode>(ImmOperand)) {
      ImmOperand = CurDAG->getTargetConstant(
          Const->getSExtValue(), SDLoc(ImmOperand), ImmOperand.getValueType());
    } else if (auto GA = dyn_cast<GlobalAddressSDNode>(ImmOperand)) {
      ImmOperand = CurDAG->getTargetGlobalAddress(
          GA->getGlobal(), SDLoc(ImmOperand), ImmOperand.getValueType(),
          GA->getOffset(), GA->getTargetFlags());
    } else {
      continue;
    }

    LLVM_DEBUG(dbgs() << "Folding add-immediate into mem-op:\nBase:    ");
    LLVM_DEBUG(Base->dump(CurDAG));
    LLVM_DEBUG(dbgs() << "\nN: ");
    LLVM_DEBUG(N->dump(CurDAG));
    LLVM_DEBUG(dbgs() << "\n");

    // Modify the offset operand of the load/store.
    if (BaseOpIdx == 0) // Load
      CurDAG->UpdateNodeOperands(N, Base.getOperand(0), ImmOperand,
                                 N->getOperand(2));
    else // Store
      CurDAG->UpdateNodeOperands(N, N->getOperand(0), Base.getOperand(0),
                                 ImmOperand, N->getOperand(3));

    // The add-immediate may now be dead, in which case remove it.
    if (Base.getNode()->use_empty())
      CurDAG->RemoveDeadNode(Base.getNode());
  }
}

// Remove redundant BuildPairF64+SplitF64 pairs. i.e. cases where an f64 is
// built of two i32 values, only to be split apart again. This must be done
// here as a peephole optimisation as the DAG has not been fully legalized at
// the point BuildPairF64/SplitF64 nodes are created in RISCVISelLowering, so
// some nodes would not yet have been replaced with libcalls.
void RISCVDAGToDAGISel::doPeepholeBuildPairF64SplitF64() {
  SelectionDAG::allnodes_iterator Position(CurDAG->getRoot().getNode());
  ++Position;

  while (Position != CurDAG->allnodes_begin()) {
    SDNode *N = &*--Position;
    // Skip dead nodes and any nodes other than SplitF64Pseudo.
    if (N->use_empty() || !N->isMachineOpcode() ||
        !(N->getMachineOpcode() == RISCV::SplitF64Pseudo))
      continue;

    // If the operand to SplitF64 is a BuildPairF64, the split operation is
    // redundant. Just use the operands to BuildPairF64 as the result.
    SDValue F64Val = N->getOperand(0);
    if (F64Val.isMachineOpcode() &&
        F64Val.getMachineOpcode() == RISCV::BuildPairF64Pseudo) {
      LLVM_DEBUG(
          dbgs() << "Removing redundant SplitF64Pseudo and replacing uses "
                    "with BuildPairF64Pseudo operands:\n");
      LLVM_DEBUG(dbgs() << "N:    ");
      LLVM_DEBUG(N->dump(CurDAG));
      LLVM_DEBUG(dbgs() << "F64Val: ");
      LLVM_DEBUG(F64Val->dump(CurDAG));
      LLVM_DEBUG(dbgs() << "\n");
      SDValue From[] = {SDValue(N, 0), SDValue(N, 1)};
      SDValue To[] = {F64Val.getOperand(0), F64Val.getOperand(1)};
      CurDAG->ReplaceAllUsesOfValuesWith(From, To, 2);
    }
  }
  CurDAG->RemoveDeadNodes();
}

// This pass converts a legalized DAG into a RISCV-specific DAG, ready
// for instruction scheduling.
FunctionPass *llvm::createRISCVISelDag(RISCVTargetMachine &TM) {
  return new RISCVDAGToDAGISel(TM);
}
