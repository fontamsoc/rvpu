// SPDX-License-Identifier: GPL-2.0-only
// 20260605 (c) William Fonkou Tambe

#ifndef HWDRVGPIO_H
#define HWDRVGPIO_H

// Structure representing a GPIO device.
// Initial state before use:
// - The field addr must be valid.
// - The field outval must be initial value.
typedef struct {
	void* addr; // Device address.
	uintptr_t outval; // Output value.
	uintptr_t iocnt; // Count of IOs.
	uintptr_t clkfreq; // Clock frequency in Hz used by the device.
} hwdrvgpio_t;

// Configure IOs where "arg" is a bitmap where each bit
// 0/1 configures corresponding IO as an input/output.
// dsc->iocnt gets set to the IO count.
static inline void hwdrvgpio_configureio (hwdrvgpio_t *dsc, uintptr_t arg) {
	void* addrDat = dsc->addr;
	void* addrCmd = (addrDat + sizeof(uintptr_t));
	uintptr_t dat;
	do {
		*(volatile uintptr_t *)addrCmd = ((arg << 1) | 0/*CMDCONFIGUREIO*/);
		dat = *(volatile uintptr_t *)addrCmd;
	} while ((dat & 1) != 0/*CMDCONFIGUREIO*/);
	dsc->iocnt = ((intptr_t)dat >> 1);
}

// Configure clockcycle count used to debounce applicable inputs.
// dsc->clkfreq gets set to the clock frequency in Hz used by the device.
static inline void hwdrvgpio_setdebounce (hwdrvgpio_t *dsc, uintptr_t arg) {
	void* addrDat = dsc->addr;
	void* addrCmd = (addrDat + sizeof(uintptr_t));
	uintptr_t dat;
	do {
		*(volatile uintptr_t *)addrCmd = ((arg << 1) | 1/*CMDSETDEBOUNCE*/);
		dat = *(volatile uintptr_t *)addrCmd;
	} while ((dat & 1) != 1/*CMDSETDEBOUNCE*/);
	dsc->clkfreq = ((intptr_t)dat >> 1);
}

// Reset all output bits.
static inline void hwdrvgpio_rst (hwdrvgpio_t *dsc, uintptr_t arg) {
	*(volatile uintptr_t *)dsc->addr = arg;
	dsc->outval = arg;
}

// Set selected output bits.
static inline void hwdrvgpio_set (hwdrvgpio_t *dsc, uintptr_t arg) {
	arg |= dsc->outval;
	if (arg ^ dsc->outval) { // Update only if there is a difference.
		*(volatile uintptr_t *)dsc->addr = arg;
		dsc->outval = arg;
	}
}

// Clear selected output bits.
static inline void hwdrvgpio_clr (hwdrvgpio_t *dsc, uintptr_t arg) {
	arg = (dsc->outval & ~arg);
	if (arg ^ dsc->outval) { // Update only if there is a difference.
		*(volatile uintptr_t *)dsc->addr = arg;
		dsc->outval = arg;
	}
}

// Toggle selected output bits.
static inline void hwdrvgpio_tgl (hwdrvgpio_t *dsc, uintptr_t arg) {
	arg ^= dsc->outval;
	*(volatile uintptr_t *)dsc->addr = arg;
	dsc->outval = arg;
}

// Get input bits.
static inline uintptr_t hwdrvgpio_get (hwdrvgpio_t *dsc) {
	return *(volatile uintptr_t *)dsc->addr;
}

// BIT(b) generates a single-bit mask shifting 1 to position b, while
// BITs(...) generates a multi-bits mask accepting up to 64 offsets.
#define BIT(b) (((uintptr_t)1) << (b))
#define __BITMASK_1(b)       (BIT(b))
#define __BITMASK_2(b, ...)  (BIT(b) | __BITMASK_1(__VA_ARGS__))
#define __BITMASK_3(b, ...)  (BIT(b) | __BITMASK_2(__VA_ARGS__))
#define __BITMASK_4(b, ...)  (BIT(b) | __BITMASK_3(__VA_ARGS__))
#define __BITMASK_5(b, ...)  (BIT(b) | __BITMASK_4(__VA_ARGS__))
#define __BITMASK_6(b, ...)  (BIT(b) | __BITMASK_5(__VA_ARGS__))
#define __BITMASK_7(b, ...)  (BIT(b) | __BITMASK_6(__VA_ARGS__))
#define __BITMASK_8(b, ...)  (BIT(b) | __BITMASK_7(__VA_ARGS__))
#define __BITMASK_9(b, ...)  (BIT(b) | __BITMASK_8(__VA_ARGS__))
#define __BITMASK_10(b, ...) (BIT(b) | __BITMASK_9(__VA_ARGS__))
#define __BITMASK_11(b, ...) (BIT(b) | __BITMASK_10(__VA_ARGS__))
#define __BITMASK_12(b, ...) (BIT(b) | __BITMASK_11(__VA_ARGS__))
#define __BITMASK_13(b, ...) (BIT(b) | __BITMASK_12(__VA_ARGS__))
#define __BITMASK_14(b, ...) (BIT(b) | __BITMASK_13(__VA_ARGS__))
#define __BITMASK_15(b, ...) (BIT(b) | __BITMASK_14(__VA_ARGS__))
#define __BITMASK_16(b, ...) (BIT(b) | __BITMASK_15(__VA_ARGS__))
#define __BITMASK_17(b, ...) (BIT(b) | __BITMASK_16(__VA_ARGS__))
#define __BITMASK_18(b, ...) (BIT(b) | __BITMASK_17(__VA_ARGS__))
#define __BITMASK_19(b, ...) (BIT(b) | __BITMASK_18(__VA_ARGS__))
#define __BITMASK_20(b, ...) (BIT(b) | __BITMASK_19(__VA_ARGS__))
#define __BITMASK_21(b, ...) (BIT(b) | __BITMASK_20(__VA_ARGS__))
#define __BITMASK_22(b, ...) (BIT(b) | __BITMASK_21(__VA_ARGS__))
#define __BITMASK_23(b, ...) (BIT(b) | __BITMASK_22(__VA_ARGS__))
#define __BITMASK_24(b, ...) (BIT(b) | __BITMASK_23(__VA_ARGS__))
#define __BITMASK_25(b, ...) (BIT(b) | __BITMASK_24(__VA_ARGS__))
#define __BITMASK_26(b, ...) (BIT(b) | __BITMASK_25(__VA_ARGS__))
#define __BITMASK_27(b, ...) (BIT(b) | __BITMASK_26(__VA_ARGS__))
#define __BITMASK_28(b, ...) (BIT(b) | __BITMASK_27(__VA_ARGS__))
#define __BITMASK_29(b, ...) (BIT(b) | __BITMASK_28(__VA_ARGS__))
#define __BITMASK_30(b, ...) (BIT(b) | __BITMASK_29(__VA_ARGS__))
#define __BITMASK_31(b, ...) (BIT(b) | __BITMASK_30(__VA_ARGS__))
#define __BITMASK_32(b, ...) (BIT(b) | __BITMASK_31(__VA_ARGS__))
#define __BITMASK_33(b, ...) (BIT(b) | __BITMASK_32(__VA_ARGS__))
#define __BITMASK_34(b, ...) (BIT(b) | __BITMASK_33(__VA_ARGS__))
#define __BITMASK_35(b, ...) (BIT(b) | __BITMASK_34(__VA_ARGS__))
#define __BITMASK_36(b, ...) (BIT(b) | __BITMASK_35(__VA_ARGS__))
#define __BITMASK_37(b, ...) (BIT(b) | __BITMASK_36(__VA_ARGS__))
#define __BITMASK_38(b, ...) (BIT(b) | __BITMASK_37(__VA_ARGS__))
#define __BITMASK_39(b, ...) (BIT(b) | __BITMASK_38(__VA_ARGS__))
#define __BITMASK_40(b, ...) (BIT(b) | __BITMASK_39(__VA_ARGS__))
#define __BITMASK_41(b, ...) (BIT(b) | __BITMASK_40(__VA_ARGS__))
#define __BITMASK_42(b, ...) (BIT(b) | __BITMASK_41(__VA_ARGS__))
#define __BITMASK_43(b, ...) (BIT(b) | __BITMASK_42(__VA_ARGS__))
#define __BITMASK_44(b, ...) (BIT(b) | __BITMASK_43(__VA_ARGS__))
#define __BITMASK_45(b, ...) (BIT(b) | __BITMASK_44(__VA_ARGS__))
#define __BITMASK_46(b, ...) (BIT(b) | __BITMASK_45(__VA_ARGS__))
#define __BITMASK_47(b, ...) (BIT(b) | __BITMASK_46(__VA_ARGS__))
#define __BITMASK_48(b, ...) (BIT(b) | __BITMASK_47(__VA_ARGS__))
#define __BITMASK_49(b, ...) (BIT(b) | __BITMASK_48(__VA_ARGS__))
#define __BITMASK_50(b, ...) (BIT(b) | __BITMASK_49(__VA_ARGS__))
#define __BITMASK_51(b, ...) (BIT(b) | __BITMASK_50(__VA_ARGS__))
#define __BITMASK_52(b, ...) (BIT(b) | __BITMASK_51(__VA_ARGS__))
#define __BITMASK_53(b, ...) (BIT(b) | __BITMASK_52(__VA_ARGS__))
#define __BITMASK_54(b, ...) (BIT(b) | __BITMASK_53(__VA_ARGS__))
#define __BITMASK_55(b, ...) (BIT(b) | __BITMASK_54(__VA_ARGS__))
#define __BITMASK_56(b, ...) (BIT(b) | __BITMASK_55(__VA_ARGS__))
#define __BITMASK_57(b, ...) (BIT(b) | __BITMASK_56(__VA_ARGS__))
#define __BITMASK_58(b, ...) (BIT(b) | __BITMASK_57(__VA_ARGS__))
#define __BITMASK_59(b, ...) (BIT(b) | __BITMASK_58(__VA_ARGS__))
#define __BITMASK_60(b, ...) (BIT(b) | __BITMASK_59(__VA_ARGS__))
#define __BITMASK_61(b, ...) (BIT(b) | __BITMASK_60(__VA_ARGS__))
#define __BITMASK_62(b, ...) (BIT(b) | __BITMASK_61(__VA_ARGS__))
#define __BITMASK_63(b, ...) (BIT(b) | __BITMASK_62(__VA_ARGS__))
#define __BITMASK_64(b, ...) (BIT(b) | __BITMASK_63(__VA_ARGS__))
#define __BITMASK__OVERFLOW(...) static_assert(0, "BITs() supports at most 64 arguments")
#define __BITMASK_SEL( \
     _1, _2, _3, _4, _5, _6, _7, _8, \
     _9,_10,_11,_12,_13,_14,_15,_16, \
    _17,_18,_19,_20,_21,_22,_23,_24, \
    _25,_26,_27,_28,_29,_30,_31,_32, \
    _33,_34,_35,_36,_37,_38,_39,_40, \
    _41,_42,_43,_44,_45,_46,_47,_48, \
    _49,_50,_51,_52,_53,_54,_55,_56, \
    _57,_58,_59,_60,_61,_62,_63,_64, \
    N, ...) __BITMASK_##N
#define BITs(...) __BITMASK_SEL(__VA_ARGS__, \
    64,63,62,61,60,59,58,57, \
    56,55,54,53,52,51,50,49, \
    48,47,46,45,44,43,42,41, \
    40,39,38,37,36,35,34,33, \
    32,31,30,29,28,27,26,25, \
    24,23,22,21,20,19,18,17, \
    16,15,14,13,12,11,10, 9, \
     8, 7, 6, 5, 4, 3, 2, 1, \
    _OVERFLOW)(__VA_ARGS__)

#endif /* HWDRVGPIO_H */
