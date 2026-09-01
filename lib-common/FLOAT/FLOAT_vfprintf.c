#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include "FLOAT.h"

extern char _vfprintf_internal;
extern char _ppfs_setargs;
extern char _fpmaxtostr;
extern int __stdio_fwrite(char *buf, int len, FILE *stream);

__attribute__((used)) static int format_FLOAT(FILE *stream, FLOAT f) {
	/* TODO: Format a FLOAT argument `f' and write the formating
	 * result to `stream'. Keep the precision of the formating
	 * result with 6 by truncating. For example:
	 *              f          result
	 *         0x00010000    "1.000000"
	 *         0x00013333    "1.199996"
	 */

	char buf[80];
	int64_t signed_value = f;
	uint32_t magnitude = signed_value < 0 ?
			(uint32_t)(-signed_value) : (uint32_t)signed_value;
	uint32_t integer = magnitude >> 16;
	uint32_t fraction = (uint32_t)(((uint64_t)(magnitude & 0xffff) * 1000000) >> 16);
	int len;

	if(f < 0) {
		len = sprintf(buf, "-%u.%06u", integer, fraction);
	}
	else {
		len = sprintf(buf, "%u.%06u", integer, fraction);
	}
	return __stdio_fwrite(buf, len, stream);
}

static int make_code_writable(void *address, uint32_t length) {
	const uintptr_t page_mask = 4095;
	uintptr_t start = (uintptr_t)address & ~page_mask;
	uintptr_t end = ((uintptr_t)address + length + page_mask) & ~page_mask;

	/* NEMU's entire 128 MB physical address space is writable. Linux links
	 * this test at 0x08048000 and protects its text pages. */
	if((uintptr_t)address < 0x08000000) {
		return 1;
	}
	return mprotect((void *)start, end - start,
			PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
}

static void modify_vfprintf() {
	/* TODO: Implement this function to hijack the formating of "%f"
	 * argument during the execution of `_vfprintf_internal'. Below
	 * is the code section in _vfprintf_internal() relative to the
	 * hijack.
	 */

#if 0
	else if (ppfs->conv_num <= CONV_A) {  /* floating point */
		ssize_t nf;
		nf = _fpmaxtostr(stream,
				(__fpmax_t)
				(PRINT_INFO_FLAG_VAL(&(ppfs->info),is_long_double)
				 ? *(long double *) *argptr
				 : (long double) (* (double *) *argptr)),
				&ppfs->info, FP_OUT );
		if (nf < 0) {
			return -1;
		}
		*count += nf;

		return 0;
	} else if (ppfs->conv_num <= CONV_S) {  /* wide char or string */
#endif

	/* You should modify the run-time binary to let the code above
	 * call `format_FLOAT' defined in this source file, instead of
	 * `_fpmaxtostr'. When this function returns, the action of the
	 * code above should do the following:
	 */

#if 0
	else if (ppfs->conv_num <= CONV_A) {  /* floating point */
		ssize_t nf;
		nf = format_FLOAT(stream, *(FLOAT *) *argptr);
		if (nf < 0) {
			return -1;
		}
		*count += nf;

		return 0;
	} else if (ppfs->conv_num <= CONV_S) {  /* wide char or string */
#endif

	uint8_t *code = (uint8_t *)&_vfprintf_internal;
	uint8_t *call = code + 0x306;
	int32_t relative;

	if(!make_code_writable(code, 0x30b)) {
		return;
	}

	/* These offsets belong to the fixed uClibc object supplied with the
	 * assignment. Refuse to patch an unexpected binary layout. */
	if(code[0x2e4] != 0xdb || code[0x2e5] != 0x2a ||
			code[0x2e8] != 0xdd || code[0x2e9] != 0x02 ||
			code[0x2f9] != 0x83 || code[0x2fa] != 0xec ||
			code[0x2fb] != 0x0c || code[0x2fc] != 0xdb ||
			code[0x2fd] != 0x3c || code[0x2fe] != 0x24 ||
			call[0] != 0xe8) {
		return;
	}

	/* Suppress fldt/fldl. Keep 24 bytes of alignment/padding, push the
	 * raw FLOAT value, then redirect the existing call instruction. */
	code[0x2e4] = code[0x2e5] = 0x90;
	code[0x2e8] = code[0x2e9] = 0x90;
	code[0x2fb] = 0x08;
	code[0x2fc] = 0xff;
	code[0x2fd] = 0x32;
	code[0x2fe] = 0x90;

	relative = (int32_t)((uint8_t *)format_FLOAT - (call + 5));
	memcpy(call + 1, &relative, sizeof(relative));
}

static void modify_ppfs_setargs() {
	/* TODO: Implement this function to modify the action of preparing
	 * "%f" arguments for _vfprintf_internal() in _ppfs_setargs().
	 * Below is the code section in _vfprintf_internal() relative to
	 * the modification.
	 */

#if 0
	enum {                          /* C type: */
		PA_INT,                       /* int */
		PA_CHAR,                      /* int, cast to char */
		PA_WCHAR,                     /* wide char */
		PA_STRING,                    /* const char *, a '\0'-terminated string */
		PA_WSTRING,                   /* const wchar_t *, wide character string */
		PA_POINTER,                   /* void * */
		PA_FLOAT,                     /* float */
		PA_DOUBLE,                    /* double */
		__PA_NOARG,                   /* non-glibc -- signals non-arg width or prec */
		PA_LAST
	};

	/* Flag bits that can be set in a type returned by `parse_printf_format'.  */
	/* WARNING -- These differ in value from what glibc uses. */
#define PA_FLAG_MASK		(0xff00)
#define __PA_FLAG_CHAR		(0x0100) /* non-gnu -- to deal with hh */
#define PA_FLAG_SHORT		(0x0200)
#define PA_FLAG_LONG		(0x0400)
#define PA_FLAG_LONG_LONG	(0x0800)
#define PA_FLAG_LONG_DOUBLE	PA_FLAG_LONG_LONG
#define PA_FLAG_PTR		(0x1000) /* TODO -- make dynamic??? */

	while (i < ppfs->num_data_args) {
		switch(ppfs->argtype[i++]) {
			case (PA_INT|PA_FLAG_LONG_LONG):
				GET_VA_ARG(p,ull,unsigned long long,ppfs->arg);
				break;
			case (PA_INT|PA_FLAG_LONG):
				GET_VA_ARG(p,ul,unsigned long,ppfs->arg);
				break;
			case PA_CHAR:	/* TODO - be careful */
				/* ... users could use above and really want below!! */
			case (PA_INT|__PA_FLAG_CHAR):/* TODO -- translate this!!! */
			case (PA_INT|PA_FLAG_SHORT):
			case PA_INT:
				GET_VA_ARG(p,u,unsigned int,ppfs->arg);
				break;
			case PA_WCHAR:	/* TODO -- assume int? */
				/* we're assuming wchar_t is at least an int */
				GET_VA_ARG(p,wc,wchar_t,ppfs->arg);
				break;
				/* PA_FLOAT */
			case PA_DOUBLE:
				GET_VA_ARG(p,d,double,ppfs->arg);
				break;
			case (PA_DOUBLE|PA_FLAG_LONG_DOUBLE):
				GET_VA_ARG(p,ld,long double,ppfs->arg);
				break;
			default:
				/* TODO -- really need to ensure this can't happen */
				assert(ppfs->argtype[i-1] & PA_FLAG_PTR);
			case PA_POINTER:
			case PA_STRING:
			case PA_WSTRING:
				GET_VA_ARG(p,p,void *,ppfs->arg);
				break;
			case __PA_NOARG:
				continue;
		}
		++p;
	}
#endif

	/* You should modify the run-time binary to let the `PA_DOUBLE'
	 * branch execute the code in the `(PA_INT|PA_FLAG_LONG_LONG)'
	 * branch. Comparing to the original `PA_DOUBLE' branch, the
	 * target branch will also prepare a 64-bit argument, without
	 * introducing floating point instructions. When this function
	 * returns, the action of the code above should do the following:
	 */

#if 0
	while (i < ppfs->num_data_args) {
		switch(ppfs->argtype[i++]) {
			case (PA_INT|PA_FLAG_LONG_LONG):
			here:
				GET_VA_ARG(p,ull,unsigned long long,ppfs->arg);
				break;
			// ......
				/* PA_FLOAT */
			case PA_DOUBLE:
				goto here;
				GET_VA_ARG(p,d,double,ppfs->arg);
				break;
			// ......
		}
		++p;
	}
#endif

	uint8_t *code = (uint8_t *)&_ppfs_setargs;

	if(!make_code_writable(code, 0xa6)) {
		return;
	}

	/* PA_DOUBLE starts with `lea 8(%edx), %ebx'. Redirect it to the
	 * PA_INT|PA_FLAG_LONG_LONG branch at offset 0xa3 instead. */
	if(code[0x71] != 0x8d || code[0x72] != 0x5a || code[0x73] != 0x08) {
		return;
	}
	code[0x71] = 0xeb;
	code[0x72] = 0x30;
	code[0x73] = 0x90;
}

void init_FLOAT_vfprintf() {
	modify_vfprintf();
	modify_ppfs_setargs();
}
