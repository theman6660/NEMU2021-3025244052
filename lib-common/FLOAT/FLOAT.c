#include "FLOAT.h"
#include <stdint.h>

FLOAT F_mul_F(FLOAT a, FLOAT b) {
	return (FLOAT)(((long long)a * b) / (1 << 16));
}

FLOAT F_div_F(FLOAT a, FLOAT b) {
	/* Dividing two 64-bit integers needs the support of another library
	 * `libgcc', other than newlib. It is a dirty work to port `libgcc'
	 * to NEMU. In fact, it is unnecessary to perform a "64/64" division
	 * here. A "64/32" division is enough.
	 *
	 * To perform a "64/32" division, you can use the x86 instruction
	 * `div' or `idiv' by inline assembly. We provide a template for you
	 * to prevent you from uncessary details.
	 *
	 *     asm volatile ("??? %2" : "=a"(???), "=d"(???) : "r"(???), "a"(???), "d"(???));
	 *
	 * If you want to use the template above, you should fill the "???"
	 * correctly. For more information, please read the i386 manual for
	 * division instructions, and search the Internet about "inline assembly".
	 * It is OK not to use the template above, but you should figure
	 * out another way to perform the division.
	 */

	int quotient, remainder;
	nemu_assert(b != 0);

	asm volatile (
		"idivl %2"
		: "=a"(quotient), "=d"(remainder)
		: "rm"(b), "a"((uint32_t)a << 16), "d"(a >> 16)
		: "cc"
	);
	(void)remainder;
	return quotient;
}

FLOAT f2F(float a) {
	/* You should figure out how to convert `a' into FLOAT without
	 * introducing x87 floating point instructions. Else you can
	 * not run this code in NEMU before implementing x87 floating
	 * point instructions, which is contrary to our expectation.
	 *
	 * Hint: The bit representation of `a' is already on the
	 * stack. How do you retrieve it to another variable without
	 * performing arithmetic operations on it directly?
	 */

	union {
		float floating;
		uint32_t bits;
	} value;
	uint32_t mantissa;
	uint32_t exponent;
	uint64_t magnitude;
	int shift;

	value.floating = a;
	exponent = (value.bits >> 23) & 0xff;
	if(exponent == 0) {
		return 0;
	}
	nemu_assert(exponent != 0xff);

	mantissa = (value.bits & 0x7fffff) | 0x800000;
	shift = (int)exponent - 134;
	if(shift >= 0) {
		magnitude = (uint64_t)mantissa << shift;
	}
	else {
		magnitude = mantissa >> (-shift);
	}

	nemu_assert(magnitude <= 0x80000000ull);
	if(value.bits >> 31) {
		return -(FLOAT)magnitude;
	}
	return (FLOAT)magnitude;
}

FLOAT Fabs(FLOAT a) {
	return a < 0 ? -a : a;
}

/* Functions below are already implemented */

FLOAT sqrt(FLOAT x) {
	FLOAT dt, t = int2F(2);

	do {
		dt = F_div_int((F_div_F(x, t) - t), 2);
		t += dt;
	} while(Fabs(dt) > f2F(1e-4));

	return t;
}

FLOAT pow(FLOAT x, FLOAT y) {
	/* we only compute x^0.333 */
	FLOAT t2, dt, t = int2F(2);

	do {
		t2 = F_mul_F(t, t);
		dt = (F_div_F(x, t2) - t) / 3;
		t += dt;
	} while(Fabs(dt) > f2F(1e-4));

	return t;
}

