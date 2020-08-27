//===- AVR.cpp ------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// AVR is a Harvard-architecture 8-bit micrcontroller designed for small
// baremetal programs. All AVR-family processors have 32 8-bit registers.
// The tiniest AVR has 32 byte RAM and 1 KiB program memory, and the largest
// one supports up to 2^24 data address space and 2^22 code address space.
//
// Since it is a baremetal programming, there's usually no loader to load
// ELF files on AVRs. You are expected to link your program against address
// 0 and pull out a .text section from the result using objcopy, so that you
// can write the linked code to on-chip flush memory. You can do that with
// the following commands:
//
//   ld.lld -Ttext=0 -o foo foo.o
//   objcopy -O binary --only-section=.text foo output.bin
//
// Note that the current AVR support is very preliminary so you can't
// link any useful program yet, though.
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "Symbols.h"
#include "Target.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm/Object/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
class AVR final : public TargetInfo {
public:
  AVR();
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};
} // namespace

AVR::AVR() { noneRel = R_AVR_NONE; }

RelExpr AVR::getRelExpr(RelType type, const Symbol &s,
                        const uint8_t *loc) const {
  switch (type) {
  case R_AVR_7_PCREL:
  case R_AVR_13_PCREL:
    return R_PC;
  default:
    return R_ABS;
  }
}

static void writeLDI(uint8_t *loc, uint64_t val) {
  write16le(loc, (read16le(loc) & 0xf0f0) | (val & 0xf0) << 4 | (val & 0x0f));
}

void AVR::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_AVR_8:
    checkUInt(loc, val, 8, rel);
    *loc = val;
    break;
  case R_AVR_16:
    // Note: this relocation is often used between code and data space, which
    // are 0x800000 apart in the output ELF file. The bitmask cuts off the high
    // bit.
    write16le(loc, val & 0xffff);
    break;
  case R_AVR_16_PM:
    checkAlignment(loc, val, 2, rel);
    checkUInt(loc, val >> 1, 16, rel);
    write16le(loc, val >> 1);
    break;
  case R_AVR_32:
    checkUInt(loc, val, 32, rel);
    write32le(loc, val);
    break;

  case R_AVR_LDI:
    checkUInt(loc, val, 8, rel);
    writeLDI(loc, val & 0xff);
    break;

  case R_AVR_LO8_LDI_NEG:
    writeLDI(loc, -val & 0xff);
    break;
  case R_AVR_LO8_LDI:
    writeLDI(loc, val & 0xff);
    break;
  case R_AVR_HI8_LDI_NEG:
    writeLDI(loc, (-val >> 8) & 0xff);
    break;
  case R_AVR_HI8_LDI:
    writeLDI(loc, (val >> 8) & 0xff);
    break;
  case R_AVR_HH8_LDI_NEG:
    writeLDI(loc, (-val >> 16) & 0xff);
    break;
  case R_AVR_HH8_LDI:
    writeLDI(loc, (val >> 16) & 0xff);
    break;
  case R_AVR_MS8_LDI_NEG:
    writeLDI(loc, (-val >> 24) & 0xff);
    break;
  case R_AVR_MS8_LDI:
    writeLDI(loc, (val >> 24) & 0xff);
    break;

  case R_AVR_LO8_LDI_PM:
    checkAlignment(loc, val, 2, rel);
    writeLDI(loc, (val >> 1) & 0xff);
    break;
  case R_AVR_HI8_LDI_PM:
    checkAlignment(loc, val, 2, rel);
    writeLDI(loc, (val >> 9) & 0xff);
    break;
  case R_AVR_HH8_LDI_PM:
    checkAlignment(loc, val, 2, rel);
    writeLDI(loc, (val >> 17) & 0xff);
    break;

  case R_AVR_LO8_LDI_PM_NEG:
    checkAlignment(loc, val, 2, rel);
    writeLDI(loc, (-val >> 1) & 0xff);
    break;
  case R_AVR_HI8_LDI_PM_NEG:
    checkAlignment(loc, val, 2, rel);
    writeLDI(loc, (-val >> 9) & 0xff);
    break;
  case R_AVR_HH8_LDI_PM_NEG:
    checkAlignment(loc, val, 2, rel);
    writeLDI(loc, (-val >> 17) & 0xff);
    break;

  case R_AVR_PORT5:
    checkUInt(loc, val, 5, rel);
    write16le(loc, (read16le(loc) & 0xff07) | (val << 3));
    break;
  case R_AVR_PORT6:
    checkUInt(loc, val, 6, rel);
    write16le(loc, (read16le(loc) & 0xf9f0) | (val & 0x30) << 5 | (val & 0x0f));
    break;

  // Since every jump destination is word aligned we gain an extra bit
  case R_AVR_7_PCREL: {
    checkInt(loc, val, 7, rel);
    checkAlignment(loc, val, 2, rel);
    const uint16_t target = (val - 2) >> 1;
    write16le(loc, (read16le(loc) & 0xfc07) | ((target & 0x7f) << 3));
    break;
  }
  case R_AVR_13_PCREL: {
    checkAlignment(loc, val, 2, rel);
    const uint16_t target = (val - 2) >> 1;
    write16le(loc, (read16le(loc) & 0xf000) | (target & 0xfff));
    break;
  }

  case R_AVR_6:
    checkInt(loc, val, 6, rel);
    write16le(loc, (read16le(loc) & 0xd3f8) | (val & 0x20) << 8 |
                       (val & 0x18) << 7 | (val & 0x07));
    break;
  case R_AVR_6_ADIW:
    checkInt(loc, val, 6, rel);
    write16le(loc, (read16le(loc) & 0xff30) | (val & 0x30) << 2 | (val & 0x0F));
    break;

  case R_AVR_CALL: {
    uint16_t hi = val >> 17;
    uint16_t lo = val >> 1;
    write16le(loc, read16le(loc) | ((hi >> 1) << 4) | (hi & 1));
    write16le(loc + 2, lo);
    break;
  }
  default:
    error(getErrorLocation(loc) + "unrecognized relocation " +
          toString(rel.type));
  }
}

TargetInfo *elf::getAVRTargetInfo() {
  static AVR target;
  return &target;
}
