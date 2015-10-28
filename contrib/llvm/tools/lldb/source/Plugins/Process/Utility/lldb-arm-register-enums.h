//===-- lldb-arm-register-enums.h -----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_arm_register_enums_h
#define lldb_arm_register_enums_h

namespace lldb_private
{
    // LLDB register codes (e.g. RegisterKind == eRegisterKindLLDB)

    //---------------------------------------------------------------------------
    // Internal codes for all ARM registers.
    //---------------------------------------------------------------------------
    enum
    {
        k_first_gpr_arm = 0,
        gpr_r0_arm = k_first_gpr_arm,
        gpr_r1_arm,
        gpr_r2_arm,
        gpr_r3_arm,
        gpr_r4_arm,
        gpr_r5_arm,
        gpr_r6_arm,
        gpr_r7_arm,
        gpr_r8_arm,
        gpr_r9_arm,
        gpr_r10_arm,
        gpr_r11_arm,
        gpr_r12_arm,
        gpr_r13_arm, gpr_sp_arm = gpr_r13_arm,
        gpr_r14_arm, gpr_lr_arm = gpr_r14_arm,
        gpr_r15_arm, gpr_pc_arm = gpr_r15_arm,
        gpr_cpsr_arm,

        k_last_gpr_arm = gpr_cpsr_arm,

        k_first_fpr_arm,
        fpu_s0_arm = k_first_fpr_arm,
        fpu_s1_arm,
        fpu_s2_arm,
        fpu_s3_arm,
        fpu_s4_arm,
        fpu_s5_arm,
        fpu_s6_arm,
        fpu_s7_arm,
        fpu_s8_arm,
        fpu_s9_arm,
        fpu_s10_arm,
        fpu_s11_arm,
        fpu_s12_arm,
        fpu_s13_arm,
        fpu_s14_arm,
        fpu_s15_arm,
        fpu_s16_arm,
        fpu_s17_arm,
        fpu_s18_arm,
        fpu_s19_arm,
        fpu_s20_arm,
        fpu_s21_arm,
        fpu_s22_arm,
        fpu_s23_arm,
        fpu_s24_arm,
        fpu_s25_arm,
        fpu_s26_arm,
        fpu_s27_arm,
        fpu_s28_arm,
        fpu_s29_arm,
        fpu_s30_arm,
        fpu_s31_arm,
        fpu_fpscr_arm,
        k_last_fpr_arm = fpu_fpscr_arm,
        exc_exception_arm,
        exc_fsr_arm,
        exc_far_arm,

        dbg_bvr0_arm,
        dbg_bvr1_arm,
        dbg_bvr2_arm,
        dbg_bvr3_arm,
        dbg_bvr4_arm,
        dbg_bvr5_arm,
        dbg_bvr6_arm,
        dbg_bvr7_arm,
        dbg_bvr8_arm,
        dbg_bvr9_arm,
        dbg_bvr10_arm,
        dbg_bvr11_arm,
        dbg_bvr12_arm,
        dbg_bvr13_arm,
        dbg_bvr14_arm,
        dbg_bvr15_arm,
        dbg_bcr0_arm,
        dbg_bcr1_arm,
        dbg_bcr2_arm,
        dbg_bcr3_arm,
        dbg_bcr4_arm,
        dbg_bcr5_arm,
        dbg_bcr6_arm,
        dbg_bcr7_arm,
        dbg_bcr8_arm,
        dbg_bcr9_arm,
        dbg_bcr10_arm,
        dbg_bcr11_arm,
        dbg_bcr12_arm,
        dbg_bcr13_arm,
        dbg_bcr14_arm,
        dbg_bcr15_arm,
        dbg_wvr0_arm,
        dbg_wvr1_arm,
        dbg_wvr2_arm,
        dbg_wvr3_arm,
        dbg_wvr4_arm,
        dbg_wvr5_arm,
        dbg_wvr6_arm,
        dbg_wvr7_arm,
        dbg_wvr8_arm,
        dbg_wvr9_arm,
        dbg_wvr10_arm,
        dbg_wvr11_arm,
        dbg_wvr12_arm,
        dbg_wvr13_arm,
        dbg_wvr14_arm,
        dbg_wvr15_arm,
        dbg_wcr0_arm,
        dbg_wcr1_arm,
        dbg_wcr2_arm,
        dbg_wcr3_arm,
        dbg_wcr4_arm,
        dbg_wcr5_arm,
        dbg_wcr6_arm,
        dbg_wcr7_arm,
        dbg_wcr8_arm,
        dbg_wcr9_arm,
        dbg_wcr10_arm,
        dbg_wcr11_arm,
        dbg_wcr12_arm,
        dbg_wcr13_arm,
        dbg_wcr14_arm,
        dbg_wcr15_arm,

        k_num_registers_arm,
        k_num_gpr_registers_arm = k_last_gpr_arm - k_first_gpr_arm + 1,
        k_num_fpr_registers_arm = k_last_fpr_arm - k_first_fpr_arm + 1
    };
}

#endif // #ifndef lldb_arm64_register_enums_h
