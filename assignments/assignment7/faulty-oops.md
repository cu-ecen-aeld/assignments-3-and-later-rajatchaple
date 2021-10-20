# Fault tracing

```Fault can be traced by using trace where keyword 'Oops' can be identified. This also indicates until what address, the code was run indicating the instruction which caused fault. ```

``` This address can be traced back in objdumo of the .ko file to find exactly what instruction caused the fault. As objdump also provides source file name, it can be easily traced in the source c code to find out cause of the fault.```

Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000046
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
Data abort info:
  ISV = 0, ISS = 0x00000046
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=000000004206b000
[0000000000000000] pgd=0000000041fe0003, p4d=0000000041fe0003, pud=0000000041fe0003, pmd=0000000000000000
Internal error: Oops: 96000046 [#1] SMP
    
        Fault occured here as mentioned with keyword Oops
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 151 Comm: sh Tainted: G           O      5.10.7 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO BTYPE=--)
pc : faulty_write+0x10/0x20 [faulty]
lr : vfs_write+0xc0/0x290
sp : ffffffc010c4bdb0
x29: ffffffc010c4bdb0 x28: ffffff8001fef080 
x27: 0000000000000000 x26: 0000000000000000 
x25: 0000000000000000 x24: 0000000000000000 
x23: 0000000000000000 x22: ffffffc010c4be30 
x21: 00000000004c9940 x20: ffffff800200b800 
x19: 0000000000000012 x18: 0000000000000000 
x17: 0000000000000000 x16: 0000000000000000 
x15: 0000000000000000 x14: 0000000000000000 
x13: 0000000000000000 x12: 0000000000000000 
x11: 0000000000000000 x10: 0000000000000000 
x9 : 0000000000000000 x8 : 0000000000000000 
x7 : 0000000000000000 x6 : 0000000000000000 
x5 : ffffff800221bce8 x4 : ffffffc008677000 
x3 : ffffffc010c4be30 x2 : 0000000000000012 
x1 : 0000000000000000 x0 : 0000000000000000 
Call trace:
 **faulty_write+0x10** /0x20 [faulty]
 ksys_write+0x6c/0x100
 __arm64_sys_write+0x1c/0x30
 el0_svc_common.constprop.0+0x9c/0x1c0
 do_el0_svc+0x70/0x90
 el0_svc+0x14/0x20
 el0_sync_handler+0xb0/0xc0
 el0_sync+0x174/0x180
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 

    This line indicates that code ran until memory location b900003f and then there was fault

---[ end trace 0fda98cae0daddd5 ]---



# Objdump

./faulty.ko:     file format elf64-littleaarch64


Disassembly of section .text:

0000000000000000 <faulty_write>:
   0:	d2800001 	mov	x1, #0x0                   	// #0
   
    Code here copies value 0 into x1 register which would be then dereferenced as indicated in c code below

   4:	d2800000 	mov	x0, #0x0                   	// #0
   8:	d503233f 	paciasp
   c:	d50323bf 	autiasp
  10:	b900003f 	str	wzr, [x1]
   
    Store instruction here tried to dread value from NULL pointer (pointer value is 0 as indicated in trace above. It mentions offset of 10 in fault_write)

  14:	d65f03c0 	ret
  18:	d503201f 	nop
  1c:	d503201f 	nop

0000000000000020 <faulty_read>:
  20:	d503233f 	paciasp
  24:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
  28:	910003fd 	mov	x29, sp
  2c:	a90153f3 	stp	x19, x20, [sp, #16]
  30:	d5384114 	mrs	x20, sp_el0
  34:	910093e0 	add	x0, sp, #0x24
  38:	f9420a83 	ldr	x3, [x20, #1040]
  3c:	f90017e3 	str	x3, [sp, #40]
  40:	d2800003 	mov	x3, #0x0                   	// #0
  44:	9100e3e4 	add	x4, sp, #0x38
  48:	12800003 	mov	w3, #0xffffffff            	// #-1
  4c:	d503201f 	nop
  50:	38001403 	strb	w3, [x0], #1
  54:	eb04001f 	cmp	x0, x4
  58:	54ffffc1 	b.ne	50 <faulty_read+0x30>  // b.any
  5c:	d5384103 	mrs	x3, sp_el0
  60:	b9402c64 	ldr	w4, [x3, #44]
  64:	f100105f 	cmp	x2, #0x4
  68:	d2800093 	mov	x19, #0x4                   	// #4
  6c:	9a939053 	csel	x19, x2, x19, ls  // ls = plast
  70:	f9400460 	ldr	x0, [x3, #8]
  74:	37a80344 	tbnz	w4, #21, dc <faulty_read+0xbc>
  78:	f9400063 	ldr	x3, [x3]
  7c:	aa0103e2 	mov	x2, x1
  80:	7206007f 	tst	w3, #0x4000000
  84:	540002c1 	b.ne	dc <faulty_read+0xbc>  // b.any
  88:	ab130042 	adds	x2, x2, x19
  8c:	9a8083e0 	csel	x0, xzr, x0, hi  // hi = pmore
  90:	da9f3042 	csinv	x2, x2, xzr, cc  // cc = lo, ul, last
  94:	fa00005f 	sbcs	xzr, x2, x0
  98:	9a9f87e2 	cset	x2, ls  // ls = plast
  9c:	aa1303e0 	mov	x0, x19
  a0:	b5000302 	cbnz	x2, 100 <faulty_read+0xe0>
  a4:	d503201f 	nop
  a8:	7100001f 	cmp	w0, #0x0
  ac:	91104294 	add	x20, x20, #0x410
  b0:	93407c00 	sxtw	x0, w0
  b4:	9a931000 	csel	x0, x0, x19, ne  // ne = any
  b8:	f94017e1 	ldr	x1, [sp, #40]
  bc:	f9400282 	ldr	x2, [x20]
  c0:	eb020021 	subs	x1, x1, x2
  c4:	d2800002 	mov	x2, #0x0                   	// #0
  c8:	54000361 	b.ne	134 <faulty_read+0x114>  // b.any
  cc:	a94153f3 	ldp	x19, x20, [sp, #16]
  d0:	a8c37bfd 	ldp	x29, x30, [sp], #48
  d4:	d50323bf 	autiasp
  d8:	d65f03c0 	ret
  dc:	9340dc22 	sbfx	x2, x1, #0, #56
  e0:	8a020022 	and	x2, x1, x2
  e4:	ab130042 	adds	x2, x2, x19
  e8:	9a8083e0 	csel	x0, xzr, x0, hi  // hi = pmore
  ec:	da9f3042 	csinv	x2, x2, xzr, cc  // cc = lo, ul, last
  f0:	fa00005f 	sbcs	xzr, x2, x0
  f4:	9a9f87e2 	cset	x2, ls  // ls = plast
  f8:	aa1303e0 	mov	x0, x19
  fc:	b4fffd62 	cbz	x2, a8 <faulty_read+0x88>
 100:	d503201f 	nop
 104:	9340dc22 	sbfx	x2, x1, #0, #56
 108:	d5384100 	mrs	x0, sp_el0
 10c:	8a020022 	and	x2, x1, x2
 110:	f9400403 	ldr	x3, [x0, #8]
 114:	ea23005f 	bics	xzr, x2, x3
 118:	9a9f0020 	csel	x0, x1, xzr, eq  // eq = none
 11c:	d503229f 	csdb
 120:	910093e1 	add	x1, sp, #0x24
 124:	aa1303e2 	mov	x2, x19
 128:	94000000 	bl	0 <__arch_copy_to_user>
 12c:	d503201f 	nop
 130:	17ffffde 	b	a8 <faulty_read+0x88>
 134:	94000000 	bl	0 <__stack_chk_fail>
 138:	d503201f 	nop
 13c:	d503201f 	nop

0000000000000140 <faulty_init>:
 140:	d503233f 	paciasp
 144:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
 148:	90000004 	adrp	x4, 0 <faulty_write>
 14c:	910003fd 	mov	x29, sp
 150:	f9000bf3 	str	x19, [sp, #16]
 154:	90000013 	adrp	x19, 0 <faulty_write>
 158:	b9400260 	ldr	w0, [x19]
 15c:	90000003 	adrp	x3, 0 <faulty_write>
 160:	91000084 	add	x4, x4, #0x0
 164:	91000063 	add	x3, x3, #0x0
 168:	52802002 	mov	w2, #0x100                 	// #256
 16c:	52800001 	mov	w1, #0x0                   	// #0
 170:	94000000 	bl	0 <__register_chrdev>
 174:	37f800a0 	tbnz	w0, #31, 188 <faulty_init+0x48>
 178:	b9400261 	ldr	w1, [x19]
 17c:	350000e1 	cbnz	w1, 198 <faulty_init+0x58>
 180:	b9000260 	str	w0, [x19]
 184:	52800000 	mov	w0, #0x0                   	// #0
 188:	f9400bf3 	ldr	x19, [sp, #16]
 18c:	a8c27bfd 	ldp	x29, x30, [sp], #32
 190:	d50323bf 	autiasp
 194:	d65f03c0 	ret
 198:	52800000 	mov	w0, #0x0                   	// #0
 19c:	f9400bf3 	ldr	x19, [sp, #16]
 1a0:	a8c27bfd 	ldp	x29, x30, [sp], #32
 1a4:	d50323bf 	autiasp
 1a8:	d65f03c0 	ret
 1ac:	d503201f 	nop

00000000000001b0 <cleanup_module>:
 1b0:	d503233f 	paciasp
 1b4:	90000000 	adrp	x0, 0 <faulty_write>
 1b8:	a9bf7bfd 	stp	x29, x30, [sp, #-16]!
 1bc:	52802002 	mov	w2, #0x100                 	// #256
 1c0:	52800001 	mov	w1, #0x0                   	// #0
 1c4:	910003fd 	mov	x29, sp
 1c8:	b9400000 	ldr	w0, [x0]
 1cc:	90000003 	adrp	x3, 0 <faulty_write>
 1d0:	91000063 	add	x3, x3, #0x0
 1d4:	94000000 	bl	0 <__unregister_chrdev>
 1d8:	a8c17bfd 	ldp	x29, x30, [sp], #16
 1dc:	d50323bf 	autiasp
 1e0:	d65f03c0 	ret
 1e4:	d500409f 	msr	pan, #0x0
 1e8:	d500419f 	msr	pan, #0x1

Disassembly of section .plt:

0000000000000300 <.plt>:
	...

Disassembly of section .text.ftrace_trampoline:

0000000000000302 <.text.ftrace_trampoline>:
	...


# C code 

## faulty read

ssize_t faulty_read(struct file *filp, char __user *buf,
        size_t count, loff_t *pos)
{
  int i;
  int ret;
  char stack_buf[4];

  /* Let's try a buffer overflow  */
  for (i = 0; i < 20; i++)
    *(stack_buf + i) = 0xff;
  if (count > 4)
    count = 4; /* copy 4 bytes to the user */
  ret = copy_to_user(buf, stack_buf, count);
  if (!ret)
    return count;
  return ret;
}

## faulty write

ssize_t faulty_write (struct file *filp, const char __user *buf, size_t count,
    loff_t *pos)
{
  /* make a simple fault by dereferencing a NULL pointer */

  __*(int *)0 = 0;__
 
    This line is trying to read a value from NULL pointer

  return 0;
}
