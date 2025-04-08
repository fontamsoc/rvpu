// SPDX-License-Identifier: GPL-2.0-only
// 20250414 (c) William Fonkou Tambe

#ifndef HWDRVGPIO_H
#define HWDRVGPIO_H

// Structure representing a GPIO device.
// The field addr must be valid.
typedef struct {
	// Device address.
	void* addr;
	// Count of IOs.
	uintptr_t iocnt;
	// Clock frequency in Hz used by the device.
	uintptr_t clkfreq;
} hwdrvgpio_t;

// Commands.
#define HWDRVGPIO_CMDCONFIGUREIO 0
#define HWDRVGPIO_CMDSETDEBOUNCE 1

// Configure IOs where "arg" is a bitmap where each bit
// 0/1 configures corresponding IO as an input/output.
// dev->iocnt gets set to the IO count.
static inline void hwdrvgpio_configureio (hwdrvgpio_t *dev, uintptr_t arg) {
	void* addrDat = dev->addr;
	void* addrCmd = (addrDat + 64);
	uintptr_t dat;
	do {
		*(volatile uintptr_t *)addrCmd = ((arg<<1) | HWDRVGPIO_CMDCONFIGUREIO);
		dat = *(volatile uintptr_t *)addrCmd;
	} while ((dat & 0b1) != HWDRVGPIO_CMDCONFIGUREIO);
	dev->iocnt = ((intptr_t)dat >> 1);
}

// Configure clockcycle count used to debounce applicable inputs.
// dev->clkfreq gets set to the clock frequency in Hz used by the device.
static inline void hwdrvgpio_setdebounce (hwdrvgpio_t *dev, uintptr_t arg) {
	void* addrDat = dev->addr;
	void* addrCmd = (addrDat + 64);
	uintptr_t dat;
	do {
		*(volatile uintptr_t *)addrCmd = ((arg<<1) | HWDRVGPIO_CMDSETDEBOUNCE);
		dat = *(volatile uintptr_t *)addrCmd;
	} while ((dat & 0b1) != HWDRVGPIO_CMDSETDEBOUNCE);
	dev->clkfreq = ((intptr_t)dat >> 1);
}

// Set outputs.
static inline void hwdrvgpio_set (hwdrvgpio_t *dev, uintptr_t arg) {
	*(volatile uintptr_t *)dev->addr = arg;
}

// Andset outputs.
static inline void hwdrvgpio_andset (hwdrvgpio_t *dev, uintptr_t arg) {
	_atomic_and((volatile uintptr_t *)dev->addr, arg);
}

// Orset outputs.
static inline void hwdrvgpio_orset (hwdrvgpio_t *dev, uintptr_t arg) {
	_atomic_or((volatile uintptr_t *)dev->addr, arg);
}

// Xorset outputs.
static inline void hwdrvgpio_xorset (hwdrvgpio_t *dev, uintptr_t arg) {
	_atomic_xor((volatile uintptr_t *)dev->addr, arg);
}

// Get inputs.
static inline uintptr_t hwdrvgpio_get (hwdrvgpio_t *dev, uintptr_t arg) {
	return *(volatile uintptr_t *)dev->addr;
}

#endif /* HWDRVGPIO_H */
