#if defined(__XTENSA__)
  .text
  .global zk_arch_idle_hint_asm
zk_arch_idle_hint_asm:
  nop
  ret.n
#elif defined(__arm__) || defined(__thumb__)
  .text
  .global zk_arch_idle_hint_asm
zk_arch_idle_hint_asm:
  wfi
  bx lr
#elif defined(__riscv)
  .text
  .global zk_arch_idle_hint_asm
zk_arch_idle_hint_asm:
  wfi
  ret
#elif defined(__x86_64__) || defined(__i386__)
  .text
  .global zk_arch_idle_hint_asm
zk_arch_idle_hint_asm:
  pause
  ret
#else
  .text
  .global zk_arch_idle_hint_asm
zk_arch_idle_hint_asm:
  ret
#endif
