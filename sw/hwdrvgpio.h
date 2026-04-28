// SPDX-License-Identifier: GPL-2.0-only
// 20260518 (c) William Fonkou Tambe

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

// Initialize outputs state.
static inline void hwdrvgpio_ini (hwdrvgpio_t *dsc, uintptr_t arg) {
	*(volatile uintptr_t *)dsc->addr = arg;
	dsc->outval = arg;
}

// Set outputs.
static inline void hwdrvgpio_set (hwdrvgpio_t *dsc, uintptr_t arg) {
	arg |= dsc->outval;
	if (arg ^ dsc->outval) { // Update only if there is a difference.
		*(volatile uintptr_t *)dsc->addr = arg;
		dsc->outval = arg;
	}
}

// Clear outputs.
static inline void hwdrvgpio_clr (hwdrvgpio_t *dsc, uintptr_t arg) {
	arg = (dsc->outval & ~arg);
	if (arg ^ dsc->outval) { // Update only if there is a difference.
		*(volatile uintptr_t *)dsc->addr = arg;
		dsc->outval = arg;
	}
}

// Toggle outputs.
static inline void hwdrvgpio_tgl (hwdrvgpio_t *dsc, uintptr_t arg) {
	arg ^= dsc->outval;
	*(volatile uintptr_t *)dsc->addr = arg;
	dsc->outval = arg;
}

// Get inputs.
static inline uintptr_t hwdrvgpio_get (hwdrvgpio_t *dsc) {
	return *(volatile uintptr_t *)dsc->addr;
}

#define BIT(n) (1UL << (n))
#define GPIO_SET(dsc, pin) hwdrvgpio_set(dsc, BIT(pin))
#define GPIO_CLR(dsc, pin) hwdrvgpio_clr(dsc, BIT(pin))
#define GPIO_TGL(dsc, pin) hwdrvgpio_tgl(dsc, BIT(pin))
#define GPIO_GET(dsc, pin) ((hwdrvgpio_get(dsc) >> (pin)) & 1)

#endif /* HWDRVGPIO_H */
